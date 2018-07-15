#ifndef INCLUDE_GUARD_CELLULAR
#define INCLUDE_GUARD_CELLULAR

#include "trapCommon.h"
#include "moduleConfig.h"
#include <TimeLib.h>
#include <painlessMesh.h>
#include <Adafruit_FONA.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_MQTT_FONA.h>

/************* 親モジュール *************/
// SIM 設定
#define APN "soracom.io"
#define USERNAME "sora"
#define PASSWORD "sora"
// MQTT サーバー接続設定（SORACOM Beam 利用）
#define MQTT_SERVER_URL "beam.soracom.io"
#define CLIENT_ID "maxj066rj51"
#define SERVER_PORT 1883
// 親機用 GPIO 設定
#define FONA_RX 32
#define FONA_TX 33
#define FONA_RST 25
#define GPS_ANTENA 2

class Cellular {
  private:
    Adafruit_FONA_3G _fona = Adafruit_FONA_3G(FONA_RST);
    // UART1 デフォルトピンは使用できないのでピンを変更 (RX=32, TX=33)
    HardwareSerial _celllarSerial = HardwareSerial(1);
    HardwareSerial *_fonaSerial;

  public:
    // fona
    Task _getGPSDataTask; // GPS 取得タスク
    Adafruit_MQTT_FONA _mqtt = Adafruit_MQTT_FONA(&_fona, MQTT_SERVER_URL, SERVER_PORT,
                                                  __TIME__ CLIENT_ID, "sora", "sora");

    char _imsi[IMSI_LEN];
    bool _fonaStart = false;
    char _lat[GPS_STR_LEN] = "\0";
    char _lon[GPS_STR_LEN] = "\0";

  public:
    Cellular() {
        _fonaSerial = &_celllarSerial;
        _celllarSerial.begin(115200, SERIAL_8N1, FONA_RX, FONA_TX);
        pinMode(GPS_ANTENA, OUTPUT);
        digitalWrite(GPS_ANTENA, LOW);
        _getGPSDataTask.set(GPS_GET_INTERVAL, GPS_TRY_COUNT, std::bind(&Cellular::getGpsData, this));
    };
    ~Cellular(){};

    void sendTrapModuleInfo();

  private:
    bool fonaSetup();
    void fonaShutdown();
    bool fonaOpenNetwork(uint8_t tryCount);
    bool connectMqttServer(uint8_t tryCount);
    void getGpsData();
    bool pushMqtt(const char *topic, const char *message);
};

#endif // INCLUDE_GUARD_CELLULAR