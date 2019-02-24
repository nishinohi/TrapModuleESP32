#include "trapCommon.h"
#include "trapServer.h"

TrapModule *pTrapModule = TrapModule::getInstance();
TrapServer trapServer;

void setup() {
    Serial.begin(115200);
    SPIFFS.begin();
    WiFi.mode(WIFI_AP_STA);
    delay(50);
    DEBUG_MSG_LN("Trap Module Start");
    // Module
    pTrapModule->setupModule();
    // Server
    DEBUG_MSG_LN("server setup");
    trapServer.setupServer();
    DEBUG_MSG_LN("server begin");
    trapServer.beginServer();
}

void loop() { pTrapModule->update(); }