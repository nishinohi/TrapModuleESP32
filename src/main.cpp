#include "trapModule.h"
#include "trapServer.h"

TrapMesh trapMesh;
TrapServer trapServer(&trapMesh);

void setup() {
    Serial.begin(115200);
    SPIFFS.begin();
    WiFi.mode(WIFI_AP_STA);
    yield();
    delay(50);
    DEBUG_MSG_LN("Trap Module Start");
    // GPIO 設定
    pinMode(TRAP_CHECK_PIN, INPUT);
    pinMode(FORCE_TRAP_MODE_PIN, INPUT);
    pinMode(LED, OUTPUT);
    // モジュール読み込み
    trapMesh.loadModuleConfig();
    // 起動時刻チェック
    trapMesh.checkWakeTime();
    // mesh
    trapMesh.setupMesh();
    trapMesh.setupTask();
    trapMesh.initCamera();
    DEBUG_MSG_LN("mesh setup");
    // Server
    trapServer.setupServer();
    DEBUG_MSG_LN("server setup");
    trapServer.beginServer();
    DEBUG_MSG_LN("server start");
}

void loop() {
    trapMesh.update();
}