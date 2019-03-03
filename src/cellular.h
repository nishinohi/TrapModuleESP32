#ifndef INCLUDE_GUARD_CELLULAR
#define INCLUDE_GUARD_CELLULAR

#include "moduleConfig.h"
#include "trapCommon.h"
#include <PubSubClient.h>
#include <TimeLib.h>
#include <WioLTEClient.h>
#include <WioLTEforArduino.h>
#include <painlessMesh.h>
/************* 親モジュール *************/
// SIM 設定
#define APN "soracom.io"
#define USERNAME "sora"
#define PASSWORD "sora"
// MQTT サーバー接続設定（SORACOM Beam 利用）
#define MQTT_SERVER_HOST "beam.soracom.io"
#define MQTT_SERVER_PORT 1883
// 親機用 GPIO 設定
#define CELLULAR_RX 26
#define CELLULAR_TX 27
// mqtt topic
#define SUBSCRIBE_TOPIC "/tm/subscribe/"
#define TEST_TOPIC "/tm/test/"
#define SETTING_TOPIC "/tm/network/mosules/setting/"
#define PERIOD_TOPIC "/tm/network/mosules/period/"
// NTP
#define NTP_SERVER "ntp.nict.jp"
// timeout
#define DEFAULT_TIMEOUT 5000
// MQTT Client ID Length
#define MQTT_CLIENT_ID_LENGTH 23

/**
 * MQTT の送信タイプ
 */
enum SendType { TEST = 0, SETTING, PERIOD };

class Cellular {
  private:
    static Cellular *_pCellular;

    WioLTE _wio = WioLTE();
    WioLTEClient _wioClient = WioLTEClient(&_wio);
    PubSubClient _mqttClient;

  public:
    char _imsi[IMSI_LEN];
    char _lat[GPS_STR_LEN] = "\0";
    char _lon[GPS_STR_LEN] = "\0";

  public:
    static Cellular *getInstance() {
        if (_pCellular == NULL) {
            _pCellular = new Cellular();
        }
        return _pCellular;
    }
    static void deleteInstance() {
        if (_pCellular == NULL) {
            return;
        }
        delete _pCellular;
        _pCellular = NULL;
    }

    bool startModule();
    bool stopModule();

    time_t getTime();

    void initMqttSetting();
    bool connectMqttServer(const char *hostAddress, const uint16_t port,
                           const char *clientId = NULL);
    void disconnectMqttServer() { _mqttClient.disconnect(); }
    void sendTrapModuleInfo(const String &contents, const SendType sendType = TEST);

  private:
    Cellular(){};

    bool wakeup();
    bool shutdown();
    bool activate(const char *apn, const char *useName, const char *password);
    bool deactivate();
    bool readImsi(char *imsi, int imsiSize);

    int socketOpen();
    bool socketClose(int connectId);

    bool reconnectMqttServer(unsigned long timeout = DEFAULT_TIMEOUT, const char *clientId = NULL);
    void setSubscribeCallback(void (*callback)(char *, uint8_t *, unsigned int)) {
        _mqttClient.setCallback(callback);
    }
    void setClient(Client &client) { _mqttClient.setClient(client); }

    bool publish(const char *topic, const char *payload) {
        return _mqttClient.publish(topic, payload);
    }
    bool subscribe(const char *topic) { return _mqttClient.subscribe(topic); }
    bool unsubscribe(const char *topic) { return _mqttClient.unsubscribe(topic); }
    static void subScribeCallback(char *topic, byte *payload, unsigned int length);
};

#endif // INCLUDE_GUARD_CELLULAR