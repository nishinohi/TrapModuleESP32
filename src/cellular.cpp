#include "cellular.h"

/**
 * FONA 起動
 */
bool Cellular::fonaSetup() {
    if (_fonaStart) {
        return true;
    }
    _fonaStart = false;
    if (!_fona.begin(*_fonaSerial)) {
        DEBUG_MSG_LN(F("Couldn't find FONA"));
        return false;
    }
    uint8_t len = _fona.getSIMIMSI(_imsi);
    if (len <= 0) {
        DEBUG_MSG_LN("failed to read sim");
        return false;
    }
    DEBUG_MSG_LN(F("FONA is OK"));
    _fonaStart = true;
    return true;
}

/**
 * FONA 終了
 */
void Cellular::fonaShutdown() {
    if (!_fona.shutdown()) {
        _fona.shutdown(true);
    }
    _fonaStart = false;
}

/**
 * ネットワークオープン
 */
bool Cellular::fonaOpenNetwork(uint8_t tryCount) {
    _fona.setGPRSNetworkSettings(F(APN), F(USERNAME), F(PASSWORD));
    while (tryCount > 0) {
        if (_fona.enableGPRS(true)) {
            return true;
        }
        DEBUG_MSG_LN(F("Failed to enable 3G"));
        TASK_DELAY(100);
        --tryCount;
    }
    return false;
}

/**
 * 罠モジュール情報を送信する
 */
void Cellular::sendTrapModuleInfo(String &contents, const SendType sendType) {
    // 作動した罠の通知を送信する
    _fonaStart = fonaSetup();
    if (!_fonaStart) {
        return;
    }
    // 罠モジュール情報送信
    if (fonaOpenNetwork(5) && connectMqttServer(5)) {
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
        }
        topic.concat(_imsi);
        topic.concat("/");
        pushMqtt(topic.c_str(), contents.c_str());
    }
    // shutdown _fona
    if (!_fona.shutdown()) {
        _fona.shutdown(true);
        _fonaStart = false;
    }
}

/**
 * GPS データ取得コールバック
 **/
void Cellular::getGpsData() {
    DEBUG_MSG_LN("getGpsData");
    // GPS 情報初期化
    memset(_lat, '\0', GPS_STR_LEN);
    memset(_lon, '\0', GPS_STR_LEN);
    // 3Gモジュールが起動していなければ起動する
    if (!fonaSetup()) {
        return;
    }
    if (!_fona.enableGPS(true)) {
        DEBUG_MSG_LN("Failed to enable GPS");
        return;
    }
    digitalWrite(GPS_ANTENA, HIGH);

    double lat = 0.0f;
    double lon = 0.0f;
    int tDay = 0;
    int tMonth = 0;
    int tYear = 0;
    int tHour = 0;
    int tMinute = 0;
    int tSecond = 0;
    if (!_fona.getGPS(&lat, &lon, NULL, NULL, NULL, &tDay, &tMonth, &tYear, &tHour, &tMinute,
                      &tSecond)) {
        if (_getGPSDataTask.isLastIteration()) {
            DEBUG_MSG_LN("failed to GPS Data...");
            digitalWrite(GPS_ANTENA, LOW);
            fonaShutdown();
            _getGPSDataTask.disable();
            return;
        }
        DEBUG_MSG_LN("getting GPS Data...");
        return;
    }
    // set time
    setTime(tHour, tMinute, tSecond, tDay, tMonth, tYear);
    // UTC -> JTC
    adjustTime(9 * SECS_PER_HOUR);
    DEBUG_MSG_F("get time:%d/%d/%d %d:%d:%d\n", year(), month(), day(), hour(), minute(), second());
    // gps
    char latStr[16] = "\0";
    char lonStr[16] = "\0";
    dtostrf(lat, 1, 7, latStr);
    dtostrf(lon, 1, 7, lonStr);
    strncpy(_lat, latStr, strlen(latStr));
    strncpy(_lon, lonStr, strlen(lonStr));
    DEBUG_MSG_F("get GPS data: (lat, lon) = (%s, %s)\n", _lat, _lon);
    _getGPSDataTask.disable();
    fonaShutdown();
    digitalWrite(GPS_ANTENA, LOW);
}

/**
 * SORACOM Beam サーバーに接続する
 */
bool Cellular::connectMqttServer(uint8_t tryCount) {
    if (_mqtt.connected()) {
        return true;
    }
    Serial.print("Connecting to MQTT... ");
    while (tryCount > 0) {
        TASK_DELAY(1);
        yield();
        // connect will return 0 for connected
        if (_mqtt.connect() == 0) {
            DEBUG_MSG_LN("MQTT Connected!");
            return true;
        }
        int8_t ret;
        DEBUG_MSG_LN(_mqtt.connectErrorString(ret));
        DEBUG_MSG_LN("Retrying MQTT connection in 2 seconds...");
        // _mqtt.disconnect();
        TASK_DELAY(2000);
        --tryCount;
    }
    return false;
}

/**
 * MQTT メッセージ送信
 */
bool Cellular::pushMqtt(const char *topic, const char *contents) {
    DEBUG_MSG_F("topic:%s\n", topic);
    DEBUG_MSG_F("contents:%s\n", contents);
    Adafruit_MQTT_Publish pushPublisher = Adafruit_MQTT_Publish(&_mqtt, topic);
    return pushPublisher.publish(contents);
}