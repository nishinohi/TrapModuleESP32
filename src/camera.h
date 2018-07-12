#ifndef INCLUDE_GUARD_CAMERA
#define INCLUDE_GUARD_CAMERA

#include "trapCommon.h"
#include <HardwareSerial.h>

// camera
#define CAMERA_ACTIVE
#define OV528_SIZE_80_60 1 // 80x60
#define OV528_SIZE_QQVGA 3 // 160x120
#define OV528_SIZE_QVGA 5  // 320x240
#define OV528_SIZE_VGA 7   // 640x480
// data length of each read, dont set this too big because ram is limited
#define PIC_PKT_LEN 500
#define CAM_ADDR 0
#define PIC_FMT OV528_SIZE_QQVGA
#define cameraRX 5
#define cameraTX 4

class Camera {
  public:
    // camera swSerial UART2 (default GPIO RX=16, TX=17)
    HardwareSerial _camSerial = HardwareSerial(2);

  private:
    unsigned long _picTotalLen = 0;     // picture length
    byte _cameraAddr = (CAM_ADDR << 5); // addr

  public:
    Camera() { _camSerial.begin(115200); };
    bool initialize();
    bool saveCameraData(int pictFmt);

  private:
    void clearRxBuf();
    void sendCmd(char cmd[], int cmd_len);
    uint16_t readBytes(uint8_t buf[], uint16_t len, uint16_t timeout_ms);
    bool preCapture(int picFmt);
    void Capture();
    bool GetData(String fileName);
};

#endif // INCLUDE_GUARD_CAMERA