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
    bool success = true;
    if (!_wio.TurnOff()) {
        DEBUG_MSG_LN("### ERROR! ###");
        success = false;
    }
    _wio.PowerSupplyLTE(false);
    return success;
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

String createRandomId() {
    DEBUG_MSG_LN("### createRandomId");
    randomSeed(millis());
    String clientId = "MqttClient-";
    clientId += String(random(0xffff), HEX);
    return clientId;
}

bool Cellular::connectMqttServer(const char *clientId) {
    DEBUG_MSG_LN("### Connecting to MQTT server \"" MQTT_SERVER_HOST "\"");
    _mqttClient.setServer(MQTT_SERVER_HOST, MQTT_SERVER_PORT);
    _mqttClient.setCallback(subScribeCallback);
    _mqttClient.setClient(_wioClient);
    // MQTT クライアントIDが未指定の場合はランダムIDを生成して接続
    if (!clientId) {
        String randomId = createRandomId();
        clientId = randomId.c_str();
    }
    if (!_mqttClient.connect(clientId)) {
        DEBUG_MSG_LN("### ERROR! ###");
        return false;
    }
    return true;
}

/**
 * MQTT サーバー再接続
 */
bool Cellular::reconnectMqttServer(unsigned long timeout, const char *clientId) {
    // clientIdが未指定の場合は接続試行の度にIDを変更する
    bool randomIdTry = !clientId;
    unsigned long current = millis();
    while (timeout > millis() - current) {
        DEBUG_MSG("### Attempting MQTT connection...");
        // MQTT クライアントIDが未指定の場合はランダムIDを生成して接続
        if (randomIdTry) {
            String randomId = createRandomId();
            clientId = randomId.c_str();
        }
        // Attempt to connect
        if (_mqttClient.connect(clientId)) {
            DEBUG_MSG_LN("### connected");
            return true;
        }
        DEBUG_MSG("### failed, rc=");
        DEBUG_MSG(_mqttClient.state());
        DEBUG_MSG_LN("### try again in 1 seconds");
        delay(1000);
    }
    DEBUG_MSG_LN("### recoonect timeout!");
    return false;
}

void Cellular::subScribeCallback(char *topic, byte *payload, unsigned int length) {
    DEBUG_MSG_LN("### subScribeCallback");
    DEBUG_MSG_LN("### TOPIC");
    DEBUG_MSG_LN(topic);
    DEBUG_MSG_LN("### payload");
    DEBUG_MSG_LN((char *)payload);
}
