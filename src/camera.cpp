#include "camera.h"

/**
 * save picture by default name
 */
bool Camera::saveCameraData() {
    if (SPIFFS.exists("/image.jpg")) {
        DEBUG_MSG_LN("delete old image");
        SPIFFS.remove("/image.jpg");
    }
    if (!_isCaptured) {
        preCapture();
        _isCaptured = true;
    }
    Capture();
    return GetData();
}

/*********************************************************************/
void Camera::clearRxBuf() {
    while (_camSerial.available()) {
        _camSerial.read();
    }
}
/*********************************************************************/
void Camera::sendCmd(char cmd[], int cmd_len) {
    for (char i = 0; i < cmd_len; i++)
        _camSerial.write(cmd[i]);
}
/*********************************************************************/
uint16_t Camera::readBytes(uint8_t buf[], uint16_t len, uint16_t timeout_ms) {
    uint16_t i;
    uint8_t subms = 0;
    for (i = 0; i < len; i++) {
        while (_camSerial.available() == 0) {
            delayMicroseconds(10);
            if (++subms >= 100) {
                if (timeout_ms == 0) {
                    return i;
                }
                subms = 0;
                timeout_ms--;
            }
        }
        buf[i] = _camSerial.read();
    }
    return i;
}
/*********************************************************************/
bool Camera::initialize() {
    char cmd[] = {0xaa, 0x0d | _cameraAddr, 0x00, 0x00, 0x00, 0x00};
    unsigned char resp[6];
    Serial.print("initializing camera...");

    unsigned long timeout = 3000;
    unsigned long current = millis();
    while (millis() - current < timeout) {
        yield();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 1000) != 6) {
            Serial.print(".");
            continue;
        }
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) &&
            resp[2] == 0x0d && resp[4] == 0 && resp[5] == 0) {
            if (readBytes((uint8_t *)resp, 6, 500) != 6) {
                continue;
            }
            if (resp[0] == 0xaa && resp[1] == (0x0d | _cameraAddr) &&
                resp[2] == 0 && resp[3] == 0 && resp[4] == 0 && resp[5] == 0) {
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
/*********************************************************************/
void Camera::preCapture() {
    char cmd[] = {0xaa, 0x01 | _cameraAddr, 0x00, 0x07, 0x00, PIC_FMT};
    unsigned char resp[6];

    while (1) {
        clearRxBuf();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 100) != 6)
            continue;
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) &&
            resp[2] == 0x01 && resp[4] == 0 && resp[5] == 0)
            break;
    }
}
void Camera::Capture() {
    char cmd[] = {0xaa,
                  0x06 | _cameraAddr,
                  0x08,
                  PIC_PKT_LEN & 0xff,
                  (PIC_PKT_LEN >> 8) & 0xff,
                  0};
    unsigned char resp[6];

    while (1) {
        clearRxBuf();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 100) != 6)
            continue;
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) &&
            resp[2] == 0x06 && resp[4] == 0 && resp[5] == 0)
            break;
    }
    cmd[1] = 0x05 | _cameraAddr;
    cmd[2] = 0;
    cmd[3] = 0;
    cmd[4] = 0;
    cmd[5] = 0;
    while (1) {
        clearRxBuf();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 100) != 6)
            continue;
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) &&
            resp[2] == 0x05 && resp[4] == 0 && resp[5] == 0)
            break;
    }
    cmd[1] = 0x04 | _cameraAddr;
    cmd[2] = 0x1;
    while (1) {
        clearRxBuf();
        sendCmd(cmd, 6);
        if (readBytes((uint8_t *)resp, 6, 100) != 6)
            continue;
        if (resp[0] == 0xaa && resp[1] == (0x0e | _cameraAddr) &&
            resp[2] == 0x04 && resp[4] == 0 && resp[5] == 0) {
            if (readBytes((uint8_t *)resp, 6, 1000) != 6) {
                continue;
            }
            if (resp[0] == 0xaa && resp[1] == (0x0a | _cameraAddr) &&
                resp[2] == 0x01) {
                _picTotalLen = (resp[3]) | (resp[4] << 8) | (resp[5] << 16);
                DEBUG_MSG("_picTotalLen:");
                DEBUG_MSG_LN(_picTotalLen);
                break;
            }
        }
    }
}
/*********************************************************************/
bool Camera::GetData() {
    unsigned int pktCnt = (_picTotalLen) / (PIC_PKT_LEN - 6);
    if ((_picTotalLen % (PIC_PKT_LEN - 6)) != 0)
        pktCnt += 1;

    char cmd[] = {0xaa, 0x0e | _cameraAddr, 0x00, 0x00, 0x00, 0x00};
    unsigned char pkt[PIC_PKT_LEN];

    char picName[] = "/image.jpg";
    File myFile = SPIFFS.open(picName, "w");
    if (!myFile) {
        DEBUG_MSG_LN("myFile open fail...");
        return false;
    } else {
        for (unsigned int i = 0; i < pktCnt; i++) {
            cmd[4] = i & 0xff;
            cmd[5] = (i >> 8) & 0xff;

            int retry_cnt = 0;
        retry:
            delay(10);
            clearRxBuf();
            sendCmd(cmd, 6);
            uint16_t cnt = readBytes((uint8_t *)pkt, PIC_PKT_LEN, 200);

            unsigned char sum = 0;
            for (int y = 0; y < cnt - 2; y++) {
                sum += pkt[y];
            }
            if (sum != pkt[cnt - 2]) {
                if (++retry_cnt < 100)
                    goto retry;
                else
                    break;
            }
            myFile.write((uint8_t *)&pkt[4], cnt - 6);

            //   DEBUG_MSG_LN("img:");
            //   for (int ii = 0; ii < cnt - 6; ++ii) {
            // 	  Serial.write(pkt[4 + ii]);
            //   }
            //   DEBUG_MSG_LN("");
            //   int encLen = base64_enc_len(cnt - 6);
            //   char enc[encLen];
            //   base64_encode(enc, (char *)&pkt[4], cnt - 6);
            //   int decLen = base64_dec_len(enc, sizeof(enc));
            //   char dec[decLen];
            //   base64_decode(dec, enc, sizeof(enc));
            //   DEBUG_MSG_LN("dec:");
            //   for (int ii = 0; ii < sizeof(dec); ++ii) {
            // 	  Serial.write(dec[ii]);
            //   }
            //   DEBUG_MSG_LN("");

            //   DEBUG_MSG_F("FreeHeepMem:%lu\n", ESP.getFreeHeap());
            //   int encLen = base64_enc_len(cnt - 6);
            //   char* enc = (char*)malloc(encLen + 1);
            //   base64_encode(enc, (char *)&pkt[4], cnt - 6);
            //   DEBUG_MSG_LN("encChar:");
            //   DEBUG_MSG_LN(enc);
            //   DEBUG_MSG_LN("encString:");
            //   String temp(enc);
            //   DEBUG_MSG_LN(temp);
            //   int decLen = base64_dec_len((char*)temp.c_str(),
            //   temp.length()); char* dec = (char*)malloc(decLen + 1);
            //   base64_decode(dec, (char*)temp.c_str(), temp.length());
            //   DEBUG_MSG_LN("dec:");
            //   for (int ii = 0; ii < decLen; ++ii) {
            // 	  Serial.write(dec[ii]);
            //   }
            //   DEBUG_MSG_LN("");
            //   DEBUG_MSG_F("FreeHeepMem:%lu\n", ESP.getFreeHeap());
            //   free(enc);
            //   free(dec);
            //   DEBUG_MSG_LN("free");
            //   DEBUG_MSG_F("FreeHeepMem:%lu\n", ESP.getFreeHeap());
        }
        cmd[4] = 0xf0;
        cmd[5] = 0xf0;
        sendCmd(cmd, 6);
    }
    myFile.close();
    return true;
}