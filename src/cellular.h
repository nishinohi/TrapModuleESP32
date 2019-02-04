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
#define MQTT_CLIENT_ID "maxj066rj51" // なんでもOK
#define MQTT_SERVER_PORT 1883
// 親機用 GPIO 設定
#define CELLULAR_RX 26
#define CELLULAR_TX 27
// mqtt topic
#define TEST_TOPIC "/tm/test/"
#define SETTING_TOPIC "/tm/network/mosules/setting/"
#define PERIOD_TOPIC "/tm/network/mosules/period/"

/**
 * MQTT の送信タイプ
 */
enum SendType { TEST = 0, SETTING, PERIOD };

class Cellular {
  private:
    WioLTE _wio = WioLTE();
    WioLTEClient _wioClient = WioLTEClient(&_wio);
    PubSubClient _mqttClient;

  public:
    char _imsi[IMSI_LEN];
    bool _fonaStart = false;
    char _lat[GPS_STR_LEN] = "\0";
    char _lon[GPS_STR_LEN] = "\0";

  public:
    Cellular(){};
    ~Cellular(){};

    bool startModule();
    bool stopModule();
    void sendTrapModuleInfo(String &contents, const SendType sendType = TEST);

  private:
    bool wakeup();
    bool shutdown();
    bool activate(const char *apn, const char *useName, const char *password);
    bool deactivate();
    int socketOpen();
    bool socketClose(int connectId);
    bool connectMqttServer();
    bool disconnectMqttServer();
    bool publish(const char *topic, const char *message);
    bool subscribe();
    static void subScribeCallback(char *topic, byte *payload, unsigned int length);
};

#endif // INCLUDE_GUARD_CELLULAR