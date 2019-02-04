#include "cellular.h"

/**
 * モジュール起動
 */
bool Cellular::startModule() {}
bool Cellular::stopModule() {}
void Cellular::sendTrapModuleInfo(String &contents, const SendType sendType) {}

/**
 * モジュール電源ON
 */
bool Cellular::wakeup() {
    DEBUG_MSG_LN("--- START ---------------------------------------------------");
    DEBUG_MSG_LN("### I/O Initialize.");
    _wio.Init(CELLULAR_RX, CELLULAR_TX);
    DEBUG_MSG_LN("### Power supply ON.");
    _wio.PowerSupplyLTE(true);
    delay(500);
    DEBUG_MSG_LN("### Turn on or reset.");
    if (!_wio.TurnOnOrReset()) {
        DEBUG_MSG_LN("### ERROR! ###");
        return false;
    }
    return true;
}

/**
 * モジュール電源OFF
 */
bool Cellular::shutdown() {
    if (!_wio.TurnOff()) {
        DEBUG_MSG_LN("### ERROR! ###");
    }
    _wio.PowerSupplyLTE(false);
}

/**
 * SIM アクティベーション
 */
bool Cellular::activate(const char *apn, const char *useName, const char *password) {
    DEBUG_MSG_LN("### Activate SIM");
    if (!_wio.Activate(apn, useName, password)) {
        return false;
    }
    return true;
}

/**
 * SIM ディアクティベート
 */
bool Cellular::deactivate() {
    DEBUG_MSG_LN("### Deactivate SIM");
    if (!_wio.Deactivate()) {
        return false;
    }
    return true;
}

/**
 * Socket オープン
 */
int Cellular::socketOpen() {
    int connectId = _wio.SocketOpen("harvest.soracom.io", 8514, WIOLTE_UDP);
    if (connectId < 0) {
        DEBUG_MSG_LN("### ERROR! ###");
    }
    return connectId;
}

/**
 * Socket クローズ
 */
bool Cellular::socketClose(int connectId) {
    if (!_wio.SocketClose(connectId)) {
        DEBUG_MSG_LN("### ERROR! ###");
        return false;
    }
    return true;
}

bool Cellular::connectMqttServer() {
    DEBUG_MSG_LN("### Connecting to MQTT server \"" MQTT_SERVER_HOST "\"");
    _mqttClient.setServer(MQTT_SERVER_HOST, MQTT_SERVER_PORT);
    _mqttClient.setCallback(subScribeCallback);
    _mqttClient.setClient(_wioClient);
    if (!_mqttClient.connect(MQTT_CLIENT_ID)) {
        DEBUG_MSG_LN("### ERROR! ###");
        return false;
    }
    return true;
}

bool Cellular::disconnectMqttServer() {}

bool Cellular::publish(const char *topic, const char *message) {}
bool Cellular::subscribe() {}

void Cellular::subScribeCallback(char *topic, byte *payload, unsigned int length) {}
