#include "bleSetting.h"
#include "trapCommon.h"
#include "trapServer.h"

TrapModule trapModule;
// TrapServer trapServer(&trapModule);
BleSetting bleSetting;

void setup() {
    Serial.begin(115200);
    SPIFFS.begin();
    WiFi.mode(WIFI_AP_STA);
    delay(50);
    DEBUG_MSG_LN("Trap Module Start");
    // GPIO 設定
    pinMode(TRAP_CHECK_PIN, INPUT);
    pinMode(FORCE_TRAP_MODE_PIN, INPUT);
    pinMode(LED, OUTPUT);
    // モジュール読み込み
    trapModule.loadModuleConfig();
    // 起動時刻チェック
    trapModule.checkStart();
    // mesh
    // ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE
    DEBUG_MSG_LN("camera setup");
    trapModule.setupCamera();
    DEBUG_MSG_LN("mesh setup");
    trapModule.setupMesh(ERROR);
    trapModule.setupTask();
    // Ble
    ModuleConfig *temp = &trapModule._config;
    bleSetting.init(&temp);
    bleSetting.start();
    // Server
    // DEBUG_MSG_LN("server setup");
    // trapServer.setupServer();
    // DEBUG_MSG_LN("server begin");
    // trapServer.beginServer();
}

void loop() { trapModule.update(); }