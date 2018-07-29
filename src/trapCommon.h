#ifndef INCLUDE_GUARD_COMMON
#define INCLUDE_GUARD_COMMON

#include <Arduino.h>
#include <SPIFFS.h>

// デバッグ
#define DEBUG_ESP_PORT Serial
#ifdef DEBUG_ESP_PORT
#define DEBUG_MSG_LN(...) DEBUG_ESP_PORT.println(__VA_ARGS__)
#define DEBUG_MSG_F(...) DEBUG_ESP_PORT.printf(__VA_ARGS__)
#define DEBUG_MSG(...) DEBUG_ESP_PORT.print(__VA_ARGS__)
#else
#define DEBUG_MSG_LN(...)
#define DEBUG_MSG_F(...)
#define DEBUG_MSG(...)
#endif
// 罠検知設定
// #define TRAP_CHECK_ACTIVE
#define TRAP_CHECK_PIN 14
// 罠設置モードでの強制起動用
#define FORCE_TRAP_MODE_PIN 32
// 接続モジュール数確認用LED
#define LED 13
#define BLINK_PERIOD 3000  // milliseconds until cycle repeat
#define BLINK_DURATION 100 // milliseconds LED is on for
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
#define KEY_PARENT_NODE_ID "ParentNodeId"
#define KEY_TRAP_FIRE "TrapFire"
#define KEY_GPS_LAT "GpsLat"
#define KEY_GPS_LON "GpsLon"
#define KEY_WAKE_TIME "WakeTime"
#define KEY_CURRENT_TIME "CurrentTime"
#define KEY_ACTIVE_START "ActiveStart"
#define KEY_ACTIVE_END "ActiveEnd"
#define KEY_NODE_NUM "NodeNum"
// メッセージ JSON KEY
#define KEY_CONFIG_UPDATE "ConfigUpdate"
#define KEY_SEND_MODULE_STATE "SendModuleState"
#define KEY_PARENT_INFO "parentInfo"
#define KEY_MODULE_STATE "ModuleState"
#define KEY_NODE_LIST "NodeList"
#define KEY_TRAP_FIRE_MESSAGE "TrapFireMessage"
#define KEY_BATTERY_DEAD "BatteryDead"
#define KEY_CURRENT_BATTERY "CurrentBattery"
#define KEY_NODE_ID "NodeId"
#define KEY_PICTURE "CameraImage"
#define KEY_INIT_GPS "InitGps"
#define KEY_GET_GPS "GetGps"
#define KEY_MESH_GRAPH "MeshGraph"
#define KEY_SYNC_SLEEP "SyncSleep"
#define KEY_REAL_TIME "RealTime"
#define KEY_CAMERA_ENABLE "CameraEnable"
#define KEY_PICTURE_FORMAT "PictureFormat"
// デフォルト設定値
#define DEF_SLEEP_INTERVAL 3600 // 60分間隔起動[sec]
#define DEF_WORK_TIME 180       // 3分間稼働[sec]
#define DEF_TRAP_MODE false     // 設置モード
#define DEF_TRAP_FIRE false     // 罠作動済みフラグ
#define DEF_GPS_LAT "\0"
#define DEF_GPS_LON "\0"
#define DEF_ACTIVE_START 0
#define DEF_ACTIVE_END 24
#define DEF_WAKE_TIME 0
#define DEF_CURRENT_TIME 0
#define DEF_NODE_NUM 0
#define DEF_NODEID 0
// 設定値上限下限値
#ifdef ESP32
#define MAX_SLEEP_INTERVAL 86400 // ESP32 の場合停止時間は24時間でも大丈夫
#else
#define MAX_SLEEP_INTERVAL 4200 // 70分[sec] 最大Sleep時間（本当は71.5分まで可能だが安全のため）
#endif
#define MIN_SLEEP_INTERVAL 10 // 10分[sec]
#define MIN_WORK_TIME 30 // 1分[sec]（これ以上短いと設定変更するためにアクセスする暇がない）
#define MAX_WORK_TIME 600 // 10分[sec]
#define MESH_WAIT_LIMIT 20 // メッシュネットワーク構築待機限界時間(_workTime - 20[sec])
// Task 関連
#define SYNC_SLEEP_INTERVAL 3000   // 同期 DeepSleep 遅延時間[msec]
#define SEND_MESSAGE_INTERVAL 1000 // メッセージ送信間隔[msec]
#define SEND_RETRY 3               // メッセージ送信リトライ数
// バッテリー関連
// #define BATTERY_CHECK_ACTIVE
#define BATTERY_LIMIT 3750 // 放電終止電圧(0.9V) * 電池 4 本(1[V] = 1024)
#define VOLTAGE_DIVIDE 6   // 分圧比
// GPS ロケーション文字列長
#define GPS_STR_LEN 16
// camera
#define DEF_IMG_PATH "/image.jpg"
// multi task
#define TASK_MEMORY 4096
#define TASK_DELAY(delayMsec) vTaskDelay((delayMsec) / portTICK_RATE_MS)
#define CAMERA_TASK_NAME "cameraTask"

// 親モジュール
#define CHILDREN_MAX 32                   // 罠作動モジュール保存最大数
#define GPS_TRY_COUNT 20                  // GPS 取得試行回数
#define GPS_GET_INTERVAL 10000            // GPS データ取得間隔[msec]
#define GPS_SEND_INTERVAL 5000            // GPS データ送信間隔[msec]
#define SEND_PARENT_INTERVAL 3000         // 親モジュール情報送信間隔
#define SEND_SYNC_SLEEP_INTERVAL 20000    // 同期停止メッセージ送信感覚
#define IMSI_LEN 16                       // ISMI(15桁の数字 + 1(終端文字))
#define KEY_FIRED_MODULES "FiredModules"  // JSON KEY
#define KEY_IS_PARENT "IsParent"          // 親モジュールとして振る舞うかどうか
#define CELLULAR_TASK_NAME "cellularTask" // 携帯モジュールタスク

#endif // INCLUDE_GUARD_COMMON
