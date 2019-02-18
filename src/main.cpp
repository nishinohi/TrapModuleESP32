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
    pinMode(FORCE_SETTING_MODE_PIN, INPUT);
    pinMode(LED, OUTPUT);
    // モジュール読み込み
    trapModule.loadModuleConfig();
    // 起動前チェック
    if (!trapModule.checkBeforeStart()) {
        trapModule.shiftDeepSleep();
    }
    DEBUG_MSG_LN("camera setup");
    trapModule.setupCamera();
    DEBUG_MSG_LN("mesh setup");
    trapModule.setupMesh(CONNECTION | SYNC); // painlessmesh 1.3v error
    trapModule.setupTask();
    // Server
    DEBUG_MSG_LN("server setup");
    trapServer.setupServer();
    DEBUG_MSG_LN("server begin");
    trapServer.beginServer();
}

void loop() { trapModule.update(); }