#include "trapCommon.h"
#include "trapServer.h"

TrapModule trapModule;
TrapServer trapServer(&trapModule);

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
    trapModule.setupMesh(ERROR);
    DEBUG_MSG_LN("mesh setup");
    trapModule.setupTask();
    DEBUG_MSG_LN("camera setup");
    trapModule.setupCamera();
    // Server
    trapServer.setupServer();
    DEBUG_MSG_LN("server setup");
    trapServer.beginServer();
    DEBUG_MSG_LN("server start");
}

void loop() { trapModule.update(); }