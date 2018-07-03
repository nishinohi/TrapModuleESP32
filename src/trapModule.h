#include <Arduino.h>
#include <SPIFFS.h>

// デバッグ
#define DEBUG_ESP_PORT Serial
#ifdef DEBUG_ESP_PORT
#define DEBUG_MSG_LN(...) DEBUG_ESP_PORT.println( __VA_ARGS__ )
#define DEBUG_MSG_F(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#define DEBUG_MSG(...) DEBUG_ESP_PORT.print( __VA_ARGS__ )
#else
#define DEBUG_MSG_LN(...)
#define DEBUG_MSG_F(...)
#define DEBUG_MSG(...)
#endif
// 罠検知設定
// #define TRAP_ACTIVE
#define TRAP_CHECK_PIN 14
// 罠設置モードでの強制起動用
#define FORCE_TRAP_MODE_PIN 32
// 接続モジュール数確認用LED
#define LED 13
#define BLINK_PERIOD 3000 // milliseconds until cycle repeat
#define BLINK_DURATION  100  // milliseconds LED is on for
// メッシュネットワーク設定
#define MESH_SSID "trapModule"
#define MESH_PASSWORD "123456789"
#define MESH_PORT 5555
// json buffer number
#define JSON_BUF_NUM 4096
// 設定値 JSON KEY
#define KEY_SLEEP_INTERVAL "SleepInterval"
#define KEY_WORK_TIME "WorkTime"
#define KEY_TRAP_MODE "TrapMode" // false:トラップ設置モード, true:トラップ起動モード
#define KEY_PARENT_MODULE_LIST "ParentModuleList"
#define KEY_TRAP_FIRE "TrapFire"
#define KEY_GPS_LAT "GpsLat"
#define KEY_GPS_LON "GpsLon"
#define KEY_WAKE_TIME "WakeTime"
#define KEY_CURRENT_TIME "CurrentTime"
#define KEY_ACTIVE_START "ActiveStart"
#define KEY_ACTIVE_END "ActiveEnd"
#define KEY_NODE_NUM "NodeNum"
// メッセージ JSON KEY
#define KEY_NODE_LIST "NodeList"
#define KEY_TRAP_FIRE_MESSAGE "TrapFireMessage"
#define KEY_BATTERY_DEAD_MESSAGE "BatteryDeadMessage"
#define KEY_NODE_ID "NodeId"
#define KEY_PICTURE "CameraImage"
#define KEY_INIT_GPS "InitGps"
#define KEY_GET_GPS "GetGps"
#define KEY_MESH_GRAPH "MeshGraph"
#define KEY_SYNC_SLEEP "SyncSleep"
#define KEY_REAL_TIME "RealTime"
#define KEY_YEAR "Year"
#define KEY_MONTH "Month"
#define KEY_DAY "Day"
#define KEY_HOUR "Hour"
#define KEY_MINUTE "Minute"
#define KEY_SECOND "Second"
#define KEY_CAMERA_ENABLE "CameraEnable"
// デフォルト設定値
#define DEF_SLEEP_INTERVAL 3600	// 60分間隔起動[sec]
#define DEF_WORK_TIME 180	// 3分間稼働[sec]
#define DEF_TRAP_MODE false // 設置モード
#define DEF_TRAP_FIRE false // 罠作動済みフラグ
#define DEF_GPS_LAT "\0"
#define DEF_GPS_LON "\0"
#define DEF_ACTIVE_START 0
#define DEF_ACTIVE_END 24
#define DEF_WAKE_TIME 0
#define DEF_CURRENT_TIME 0
#define DEF_NODE_NUM 0
// 設定値上限下限値
#ifdef ESP32
#define MAX_SLEEP_INTERVAL 86400 // ESP32 の場合停止時間は24時間でも大丈夫
#else
#define MAX_SLEEP_INTERVAL 4200	// 70分[sec] 最大Sleep時間（本当は71.5分まで可能だが安全のため）
#endif
#define MIN_SLEEP_INTERVAL 10	// 10分[sec]
#define MIN_WORK_TIME 30	// 1分[sec]（これ以上短いと設定変更するためにアクセスする暇がない）
#define MAX_WORK_TIME 600	// 10分[sec]
#define MESH_WAIT_LIMIT 20 // メッシュネットワーク構築待機限界時間(_workTime - 20[sec])
// Task 関連
#define CHECK_INTERVAL 2000 // モジュール監視間隔[msec]
#define TRAP_CHECK_DELAYED 3000 // 罠作動チェック間隔[msec]
#define BATTERY_CHECK_DELAYED 4000	// バッテリー残量チェック間隔(msec)
#define SYNC_SLEEP_INTERVAL 3000 // // 同期 DeepSleep 遅延時間[msec]
#define SEND_RETRY 3 // メッセージ送信リトライ数
// バッテリー関連
// #define BATTERY_CHECK_ACTIVE	// バッテリー残量チェックを行わない場合（分圧用抵抗が無いなど）はこの行をコメントアウト
#define DISCHARGE_END_VOLTAGE 610	// 放電終止電圧(1V = 1024)として 1/6 に分圧した場合の読み取り値
// GPS ロケーション文字列長
#define GPS_STR_LEN 16