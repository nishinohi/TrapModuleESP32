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
#define FORCE_SETTING_MODE_PIN 35
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
#define KEY_ACTIVE_START "active_start"
#define KEY_ACTIVE_END "active_end"
#define KEY_TRAP_MODE "trap_mode" // false:トラップ設置モード, true:トラップ起動モード
#define KEY_TRAP_FIRE "trap_fire"
#define KEY_GPS_LAT "lat"
#define KEY_GPS_LON "lon"
#define KEY_PARENT_NODE_ID "parent_id"
#define KEY_NODE_NUM "node_num"
#define KEY_WAKE_TIME "wake_time"
#define KEY_CURRENT_TIME "current_time"
// メッセージ JSON KEY
#define KEY_CONFIG_UPDATE "config_update"
#define KEY_REQUEST_MODULE_STATE "request_module_state"
#define KEY_MODULE_STATE "module_state"
#define KEY_NODE_LIST "node_list"
#define KEY_BATTERY_DEAD "battery_dead"
#define KEY_CURRENT_BATTERY "remaining_battery"
#define KEY_NODE_ID "module_id"
#define KEY_PICTURE "camera_image"
#define KEY_INIT_GPS "init_gps"
#define KEY_MESH_GRAPH "mesh_graph"
#define KEY_SYNC_SLEEP "sync_sleep"
#define KEY_CAMERA_ENABLE "camera"
#define KEY_PICTURE_FORMAT "picture_format"
// 稼働時間
#define WORK_TIME 180000 // 3分間稼働[msec]
// 起動時刻設定最小閾値[min]
#define WAKE_TIME_SET_MIN 15
// デフォルト設定値
#define DEF_TRAP_MODE false // 設置モード
#define DEF_TRAP_FIRE false // 罠作動済みフラグ
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
// 最大DeepSleep時間[sec]
#define MAX_SLEEP_TIME 86400 // ESP32 の場合停止時間は24時間でも大丈夫
#else
#define MAX_SLEEP_TIME 4200 // 70分[sec] 最大Sleep時間（本当は71.5分まで可能だが安全のため）
#endif
// Task 関連
#define SYNC_SLEEP_INTERVAL 3000    // 同期 DeepSleep 遅延時間[msec]
#define BATTERY_CHECK_INTERVAL 5000 // バッテリー残量チェック間隔[msec]
#define MODULE_STATE_INTERVAL 3000  // モジュール状態送信間隔ランダム[msec]
#define DEF_INTERVAL 1000           // メッセージ送信間隔[msec]
#define DEF_ITERATION 3             // メッセージ送信リトライ数
// バッテリー関連
// #define BATTERY_CHECK_ACTIVE
#define BATTERY_LIMIT 4460 // 放電終止電圧(0.9V) * 電池 4 本(1[V] = 1240)
#define VOLTAGE_DIVIDE 2   // 分圧比
#define REAL_BATTERY_VALUE(v) (v) * VOLTAGE_DIVIDE / 1240
// GPS ロケーション文字列長
#define GPS_STR_LEN 16
// camera
#define DEF_IMG_PATH "/image.jpg"
// multi task
#define TASK_MEMORY 4096
#define TASK_DELAY(delayMsec) vTaskDelay((delayMsec) / portTICK_RATE_MS)
#define CAMERA_TASK_NAME "cameraTask"

#endif // INCLUDE_GUARD_COMMON
