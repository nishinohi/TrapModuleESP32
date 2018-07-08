#include "trapModule.h"
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
#define GPS_ANTENA 15
// 罠作動モジュール保存最大数
#define CHILDREN_MAX 32
// GPS 取得試行回数
#define GPS_TRY_COUNT 20
// GPS データ取得間隔[msec]
#define GPS_GET_INTERVAL 10000 // 10秒
// GPS 送信試行回数
#define GPS_SEND_COUNT 5
// GPS データ送信間隔[msec]
#define GPS_SEND_INTERVAL 5000 // 5秒
// 親モジュール情報送信間隔
#define SEND_PARENT_INTERVAL 3000 // 3秒
// 同期停止メッセージ送信感覚
#define SEND_SYNC_SLEEP_INTERVAL 20000 // 20病
// ISMI(15桁の数字 + 1(終端文字))
#define IMSI_LEN 16
// JSON KEY
#define KEY_FIRED_MODULES "FiredModules"

class Cellular {
  public:
    // UART1 デフォルトピンは使用できないのでピンを変更 (RX=32, TX=33)
    HardwareSerial _celllarSerial = HardwareSerial(1);
    HardwareSerial *fonaSerial = &_celllarSerial;
    // fona
    Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);
    Adafruit_MQTT_FONA mqtt =
        Adafruit_MQTT_FONA(&fona, MQTT_SERVER_URL, SERVER_PORT,
                           __TIME__ CLIENT_ID, "sora", "sora");

    char _imsi[IMSI_LEN];
    bool _fonaStart = false;

  public:
    Cellular() { _celllarSerial.begin(115200, SERIAL_8N1, FONA_RX, FONA_TX); };

    bool fonaSetup();
    bool fonaOpenNetwork(uint8_t tryCount);
    void sendTrapModuleInfo();
    void pushTrapFireInfo();
    bool pushGPSData();
    bool connectMqttServer(uint8_t tryCount);
    bool pushMqtt(const char *topic, const char *message);
};