#ifndef INCLUDE_GUARD_CAMERA
#define INCLUDE_GUARD_CAMERA

#include "trapCommon.h"
#include <HardwareSerial.h>

#define PIC_PKT_LEN 500
#define CAM_ADDR 0
// camera swSerial UART2 (default GPIO RX=16, TX=17)
#define CAMEARA_RX 16
#define CAMERA_TX 17
// 初期化タイムアウト[msec]
#define INITIALIZE_TIMEOUT 1500
#define DEF_CAMERA_TIMEOUT 500

enum CameraResolution {
    NON_SET = 0,
    OV528_SIZE_80_60 = 1, // 80x60
    OV528_SIZE_QQVGA = 3, // 160x120
    OV528_SIZE_QVGA = 5,  // 320x240
    OV528_SIZE_VGA = 7    // 640x480
};

class Camera {
  private:
    HardwareSerial _camSerial = HardwareSerial(2);
    static Camera *_pCamera;

    int _resolution = NON_SET;
    byte _cameraAddr = (CAM_ADDR << 5); // addr

  public:
    static Camera *getInstance() {
        if (_pCamera == NULL) {
            _pCamera = new Camera();
        }
        return _pCamera;
    }
    static void deleteInstance() {
        if (_pCamera == NULL) {
            return;
        }
        delete _pCamera;
        _pCamera = NULL;
    }

    bool cameraSerialBegin() {
        DEBUG_MSG_LN("cameraSerialBegin");
        _camSerial.begin(115200, SERIAL_8N1, CAMEARA_RX, CAMERA_TX);
    }
    bool initialize();
    bool saveCameraData(String path = DEF_IMG_PATH);
    bool isSetResolution() {
        _resolution == NON_SET ? DEBUG_MSG_LN("resolution no set") : DEBUG_MSG_LN("resolution set");
        return _resolution != NON_SET;
    }
    void setResolution(int resolution);

  private:
    Camera(){};
    void clearRxBuf();
    void sendCmd(char cmd[], int cmd_len);
    uint16_t readBytes(uint8_t buf[], uint16_t len, uint16_t timeout_ms = DEF_CAMERA_TIMEOUT);
    bool preCapture(int picFmt);
    unsigned long capture();
    bool readAndSaveCaptureData(String fileName, unsigned long dataLen);
};

#endif // INCLUDE_GUARD_CAMERA