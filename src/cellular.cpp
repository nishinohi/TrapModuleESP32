#include "cellular.h"

bool Cellular::fonaSetup() {
    if (!fona.begin(*fonaSerial)) {
        DEBUG_MSG_LN(F("Couldn't find FONA"));
        return false;
    }
    uint8_t len = fona.getSIMIMSI(_imsi);
    if (len <= 0) {
        DEBUG_MSG_LN("failed to read sim");
        return false;
    }
    DEBUG_MSG_LN(F("FONA is OK"));
    return true;
}

/**
 * ネットワークオープン
 */
bool Cellular::fonaOpenNetwork(uint8_t tryCount) {
    fona.setGPRSNetworkSettings(F(APN), F(USERNAME), F(PASSWORD));
    while (tryCount > 0) {
        if (fona.enableGPRS(true)) {
            return true;
        }
        DEBUG_MSG_LN(F("Failed to enable 3G"));
        delay(100);
        --tryCount;
    }
    return false;
}

/**
 * 罠モジュール情報を送信する
 */
void Cellular::sendTrapModuleInfo() {
    // if (_firedModules.size() == 0 && !_trapFire) {
    //     return;
    // }
    // // 作動した罠の通知を送信する
    // _fonaStart = fonaSetup();
    // if (!_fonaStart) {
    //     return;
    // }
    // // 罠モジュール情報送信
    // if (fonaOpenNetwork(5) && connectMqttServer(5)) {
    //     pushTrapFireInfo();
    //     pushGPSData();
    // }
    // // shutdown fona
    // if (!fona.shutdown()) {
    //     fona.shutdown(true);
    //     _fonaStart = false;
    // }
}

/**
 * 罠が作動したモジュールの chipId とセットでメッセージを送信する
 * topic：/test/mqtt、message：任意
 */
void Cellular::pushTrapFireInfo() {
    // SimpleList<uint32_t>::iterator firedModule = _firedModules.begin();
    // while (firedModule != _firedModules.end()) {
    //     String message = String(*firedModule);
    //     message += ":えものがかかりました";
    //     if (pushMqtt("/test/mqtt", message.c_str())) {
    //         firedModule = _firedModules.erase(firedModule);
    //     } else {
    //         ++firedModule;
    //     }
    //     DEBUG_MSG_LN("罠検知情報送信");
    //     delay(100);
    // }
    // return;
}

/**
 * GPSデータをpushする
 * topic：/test/mqtt/gps/(15桁のIMSI)、message：123.435553,22.131234
 */
bool Cellular::pushGPSData() {
    // char gps[32] = "\0";
    // strncat(gps, _lat, strlen(_lat) - 1);
    // strncat(gps, ",", 1);
    // strncat(gps, _lon, strlen(_lon) - 1);
    // char topic[64] = "/test/mqtt/gps/";
    // strncat(topic, _imsi, strlen(_imsi));
    // DEBUG_MSG_F("GPS情報送信:(lat, lon) = (%s)\n", gps);
    // return pushMqtt(topic, gps);
}

bool Cellular::connectMqttServer(uint8_t tryCount) {
    int8_t ret;
    if (mqtt.connected()) {
        return true;
    }
    Serial.print("Connecting to MQTT... ");
    while (tryCount > 0) {
        // connect will return 0 for connected
        if (mqtt.connect() == 0) {
            DEBUG_MSG_LN("MQTT Connected!");
            return true;
        }
        DEBUG_MSG_LN(mqtt.connectErrorString(ret));
        DEBUG_MSG_LN("Retrying MQTT connection in 2 seconds...");
        mqtt.disconnect();
        delay(2000);
        --tryCount;
    }
    return false;
}

/**
 * MQTT メッセージ送信
 */
bool Cellular::pushMqtt(const char *topic, const char *message) {
    bool ret;
    Adafruit_MQTT_Publish pushPublisher = Adafruit_MQTT_Publish(&mqtt, topic);
    return pushPublisher.publish(message);
}