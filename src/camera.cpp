#include "camera.h"

// singleton
Camera *Camera::_pCamera = NULL;

/**
 * 画像を撮影し指定したファイル名を含むパスに保存
 * パスを指定しないとデフォルトのパスに保存
 */
bool Camera::saveCameraData(String path) {
    DEBUG_MSG_LN("saveCameraData");
    if (SPIFFS.exists(path)) {
        DEBUG_MSG_LN("delete old image");
        SPIFFS.remove(path);
    }
    _resolution == NON_SET ? preCapture(OV528_SIZE_QVGA) : preCapture(_resolution);
    unsigned long dataLen = capture();
    return readAndSaveCaptureData(path, dataLen);
}

/**
 * バッファクリア
 */
void Camera::clearRxBuf() {
    while (_camSerial.available()) {
        TASK_DELAY(1);
        _camSerial.read();
    }
}

/**
 * カメラモジュールに任意のコマンドを送信
 */
void Camera::sendCmd(char cmd[], int cmd_len) {
    for (int i = 0; i < cmd_len; i++) {
        TASK_DELAY(1);
        _camSerial.write(cmd[i]);
    }
}

/**
 * UART のバッファから読み出し
 */
uint16_t Camera::readBytes(uint8_t buf[], uint16_t len, uint16_t timeout_ms) {
    unsigned long current = millis();
    int bufIndex = 0;
    for (bufIndex = 0; bufIndex < len; bufIndex++) {
        while (!_camSerial.available()) {
            if (millis() - current > timeout_ms) {
                DEBUG_MSG_LN("read Buffer timeout.");
                return bufIndex;
            }
            TASK_DELAY(1);
        }
        buf[bufIndex] = _camSerial.read();
        current = millis();
    }
    return bufIndex;
}

/**
 * 初期化
 */
bool Camera::initialize() {
    DEBUG_MSG_LN("initializing camera");
    clearRxBuf();
    char cmd[] = {0xaa, 0x0d | _cameraAddr, 0x00, 0x00, 0x00, 0x00};
    unsigned char resp[6];

    unsigned long current = millis();
    while (millis() - current < INITIALIZE_TIMEOUT) {
        TASK_DELAY(1);
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 500) != 6) {
            DEBUG_MSG(".");
            continue;
        }
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) && resp[2] == 0x0d && resp[4] == 0 &&
            resp[5] == 0) {
            if (readBytes((uint8_t *)resp, 6, 500) != 6) {
                continue;
            }
            if (resp[0] == 0xaa && resp[1] == (0x0d | _cameraAddr) && resp[2] == 0 &&
                resp[3] == 0 && resp[4] == 0 && resp[5] == 0) {
                cmd[1] = 0x0e | _cameraAddr;
                cmd[2] = 0x0d;
                sendCmd(cmd, 6);
                DEBUG_MSG_LN("\nCamera initialization done.");
                return true;
            }
        }
    }
    DEBUG_MSG_LN("\nCamera Not found.");
    return false;
}

/**
 * カメラ解像度設定
 */
void Camera::setResolution(int resolution) {
    DEBUG_MSG("Camera Resolution:");
    switch (resolution) {
    case OV528_SIZE_80_60:
        DEBUG_MSG_LN("OV528_SIZE_80_60");
        resolution = 1;
        break;
    case OV528_SIZE_QQVGA:
        DEBUG_MSG_LN("OV528_SIZE_QQVGA");
        resolution = 3;
        break;
    case OV528_SIZE_QVGA:
        DEBUG_MSG_LN("OV528_SIZE_QVGA");
        resolution = 5;
        break;
    case OV528_SIZE_VGA:
        DEBUG_MSG_LN("OV528_SIZE_VGA");
        resolution = 7;
        break;
    default:
        DEBUG_MSG_LN("OV528_SIZE_QQVGA");
        resolution = 3;
        break;
    }
    _resolution = resolution;
}

/**
 * キャプチャ準備
 */
bool Camera::preCapture(int picFmt) {
    DEBUG_MSG_LN("preCapture");
    char cmd[] = {0xaa, 0x01 | _cameraAddr, 0x00, 0x07, 0x00, picFmt};
    unsigned char resp[6];

    unsigned long timeout = 3000;
    unsigned long current = millis();
    while (millis() - current < timeout) {
        TASK_DELAY(1);
        clearRxBuf();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 500) != 6) {
            continue;
        }
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) && resp[2] == 0x01 && resp[4] == 0 &&
            resp[5] == 0) {
            return true;
        }
    }
    return false;
}

/**
 * キャプチャ
 */
unsigned long Camera::capture() {
    DEBUG_MSG_LN("capture");
    char cmd[] = {0xaa, 0x06 | _cameraAddr, 0x08, PIC_PKT_LEN & 0xff, (PIC_PKT_LEN >> 8) & 0xff, 0};
    unsigned char resp[6];

    while (1) {
        TASK_DELAY(1);
        clearRxBuf();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 500) != 6)
            continue;
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) && resp[2] == 0x06 && resp[4] == 0 &&
            resp[5] == 0)
            break;
    }
    cmd[1] = 0x05 | _cameraAddr;
    cmd[2] = 0;
    cmd[3] = 0;
    cmd[4] = 0;
    cmd[5] = 0;
    while (1) {
        TASK_DELAY(1);
        clearRxBuf();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 500) != 6)
            continue;
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) && resp[2] == 0x05 && resp[4] == 0 &&
            resp[5] == 0)
            break;
    }
    cmd[1] = 0x04 | _cameraAddr;
    cmd[2] = 0x1;
    unsigned long dataLen = 0;
    while (1) {
        clearRxBuf();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 500) != 6)
            continue;
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) && resp[2] == 0x04 && resp[4] == 0 &&
            resp[5] == 0) {
            if (readBytes((uint8_t *)resp, 6, 1000) != 6) {
                continue;
            }
            if (resp[0] == 0xaa && resp[1] == (0x0a | _cameraAddr) && resp[2] == 0x01) {
                dataLen = (resp[3]) | (resp[4] << 8) | (resp[5] << 16);
                DEBUG_MSG("DataLen:");
                DEBUG_MSG_LN(dataLen);
                break;
            }
        }
    }
    return dataLen;
}

/**
 * キャプチャしたデータをバッファから読み出し
 * 指定したパスへ保存する
 */
bool Camera::readAndSaveCaptureData(String fileName, unsigned long dataLen) {
    DEBUG_MSG_LN("readAndSaveCaptureData");
    File myFile = SPIFFS.open(fileName, "w");
    if (!myFile) {
        DEBUG_MSG_LN("myFile open fail...");
        return false;
    }

    char cmd[] = {0xaa, 0x0e | _cameraAddr, 0x00, 0x00, 0x00, 0x00};
    unsigned char pkt[PIC_PKT_LEN];
    unsigned long readLen = 0;
    for (int ii = 0; readLen < dataLen; ++ii) {
        cmd[4] = ii & 0xff;
        cmd[5] = (ii >> 8) & 0xff;

        // delay(10);
        TASK_DELAY(1);
        clearRxBuf();
        sendCmd(cmd, 6);
        uint16_t cnt = readBytes((uint8_t *)pkt, PIC_PKT_LEN, 500);
        // 読み込んだデータが正常かチェック
        if (cnt == 0) {
            break;
        }
        unsigned char sum = 0;
        for (int y = 0; y < cnt - 2; y++) {
            sum += pkt[y];
        }
        if (sum != pkt[cnt - 2]) {
            break;
        }
        readLen += cnt - 6;
        myFile.write((uint8_t *)&pkt[4], cnt - 6);
    }
    cmd[4] = 0xf0;
    cmd[5] = 0xf0;
    sendCmd(cmd, 6);
    myFile.close();
    return readLen == dataLen;
}