#include "cellular.h"
/************************************************
                    helper
*************************************************/

String createRandomId() {
    DEBUG_MSG_LN("### createRandomId");
    randomSeed(millis());
    String clientId = "MqttClient-";
    clientId += String(random(0xffff), HEX);
    return clientId;
}

/************************************************
                    public
*************************************************/
/**
 * モジュール起動
 */
bool Cellular::startModule() {
    DEBUG_MSG_LN("### startModule");
    // wio seyup
    if (!wakeup()) {
        DEBUG_MSG_LN("### wakeup failed");
        return false;
    }
    if (!activate(APN, USERNAME, PASSWORD)) {
        DEBUG_MSG_LN("### activate failed");
        return false;
    }
    if (!readImsi(_imsi, IMSI_LEN)) {
        DEBUG_MSG_LN("### readImsi failed");
        return false;
    }
    return true;
}

/**
 * モジュール停止
 */
bool Cellular::stopModule() {
    DEBUG_MSG_LN("### stopModule");
    if (!shutdown()) {
        DEBUG_MSG_LN("### forced shutdown");
    }
}

/**
 * 現在時刻取得
 * tm_yearは1900年からの経過年数
 */
time_t Cellular::getTime() {
    DEBUG_MSG_LN("### getTime");
    struct tm currentTime;
    if (!_wio.SyncTime(NTP_SERVER)) {
        DEBUG_MSG_LN("### SyncTime failed");
        return 0;
    }
    if (!_wio.GetTime(&currentTime)) {
        DEBUG_MSG_LN("### GetTime failed");
        return 0;
    }
    DEBUG_MSG_LN(asctime(&currentTime));
    return mktime(&currentTime);
}

/**
 * MQTT 初回設定
 */
void Cellular::initMqttSetting() {
    // mqtt setting
    setSubscribeCallback(subScribeCallback);
    setClient(_wioClient);
}

/**
 * MQTT サーバー接続
 */
bool Cellular::connectMqttServer(const char *hostAddress, const uint16_t port,
                                 const char *clientId) {
    DEBUG_MSG_F("### Connecting to MQTT server \"%s\"", hostAddress);
    _mqttClient.setServer(hostAddress, port);
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
 * 罠検知情報通知
 */
void Cellular::sendTrapModuleInfo(const String &contents, const SendType sendType) {
    DEBUG_MSG_LN("sendTrapModuleInfo");
    String topic;
    switch (sendType) {
    case TEST:
        topic = TEST_TOPIC;
        break;
    case SETTING:
        topic = SETTING_TOPIC;
        break;
    case PERIOD:
        topic = PERIOD_TOPIC;
        break;
    default:
        topic = TEST_TOPIC;
    }
    topic.concat(_imsi);
    DEBUG_MSG_F("topic:\n%s\n", topic.c_str());
    DEBUG_MSG_F("period:\n%s\n", contents.c_str());
    publish(topic.c_str(), contents.c_str());
}

/************************************************
                     private
*************************************************/

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

bool Cellular::readImsi(char *imsi, int imsiSize) {
    DEBUG_MSG_LN("### readImsi");
    bool succes = _wio.GetIMSI(imsi, imsiSize) == -1 ? false : true;
    DEBUG_MSG_LN(imsi);
    return succes;
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
