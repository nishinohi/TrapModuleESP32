#include "trapServer.h"

/**
 * Server 設定
 */
void TrapServer::setupServer() {
    server.on("/setConfig", HTTP_POST,
              std::bind(&TrapServer::onSetConfig, this, std::placeholders::_1));
    server.on(
        "/getModuleInfo", HTTP_GET,
        std::bind(&TrapServer::onGetModuleInfo, this, std::placeholders::_1));
    server.on(
        "/getMeshGraph", HTTP_GET,
        std::bind(&TrapServer::onGetMeshGraph, this, std::placeholders::_1));
    server.on(
        "/setCurrentTime", HTTP_POST,
        std::bind(&TrapServer::onSetCurrentTime, this, std::placeholders::_1));
    server.on("/snapShot", HTTP_POST,
              std::bind(&TrapServer::onSnapShot, this, std::placeholders::_1));
    server.on(
        "/sendMessage", HTTP_POST,
        std::bind(&TrapServer::onSendMessage, this, std::placeholders::_1));
    server.on("/initGps", HTTP_POST,
              std::bind(&TrapServer::onInitGps, this, std::placeholders::_1));
    server.on("/getGps", HTTP_GET,
              std::bind(&TrapServer::onGetGps, this, std::placeholders::_1));
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    server.onNotFound(
        [](AsyncWebServerRequest *request) { request->send(404); });
}

/**********************************
 * Server call back
 *********************************/
/**
 * モジュール設定コールバック
 */
void TrapServer::onSetConfig(AsyncWebServerRequest *request) {
    DEBUG_MSG_LN("onSetConfig");
    // 設定値反映
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &config = jsonBuf.createObject();
    // 起動間隔
    String temp = request->arg(KEY_SLEEP_INTERVAL);
    if (temp != NULL && temp.length() != 0) {
        config[KEY_SLEEP_INTERVAL] = temp.toInt();
    }
    // 稼働時間
    temp = request->arg(KEY_WORK_TIME);
    if (temp != NULL && temp.length() != 0) {
        config[KEY_WORK_TIME] = temp.toInt();
    }
    // 罠モード
    temp = request->arg(KEY_TRAP_MODE);
    if (temp != NULL && temp.length() != 0) {
        config[KEY_TRAP_MODE] = temp.toInt() == 1;
    }
    // 稼働開始時間
    temp = request->arg(KEY_ACTIVE_START);
    if (temp != NULL && temp.length() != 0) {
        config[KEY_ACTIVE_START] = temp.toInt();
    }
    // 稼働終了時間
    temp = request->arg(KEY_ACTIVE_END);
    if (temp != NULL && temp.length() != 0) {
        config[KEY_ACTIVE_END] = temp.toInt();
    }
    // 設定された変更値で全モジュールの設定値を更新
    if (_trapMesh->syncAllModuleConfigs(config)) {
        _trapMesh->updateModuleConfig(config);
        _trapMesh->saveCurrentModuleConfig();
        String cfg;
        config.printTo(cfg);
        request->send(200, "application/json", cfg);
    } else {
        request->send(500);
        return;
    }
}

/**
 * モジュール情報取得
 */
void TrapServer::onGetModuleInfo(AsyncWebServerRequest *request) {
    DEBUG_MSG_F("FreeHeepMem:%lu\n", ESP.getFreeHeap());
    DEBUG_MSG_LN("onGetModuleInfo");
    JsonObject &moduleInfo = _trapMesh->getModuleInfo();
    String response;
    moduleInfo.printTo(response);
    DEBUG_MSG_LN(response);
    request->send(200, "application/json", response);
}

/**
 * メッシュネットワーク状態取得
 */
void TrapServer::onGetMeshGraph(AsyncWebServerRequest *request) {
    DEBUG_MSG_LN("onGetMeshGraph");
    String response = _trapMesh->_mesh.subConnectionJson();
    DEBUG_MSG_LN(response);
    request->send(200, "application/json", response);
}

/**
 * 現在時刻設定
 */
void TrapServer::onSetCurrentTime(AsyncWebServerRequest *request) {
    DEBUG_MSG_LN("onSetCurrentTime");
    if ((request->arg(KEY_YEAR) == NULL || request->arg(KEY_YEAR).length()) ==
            0 ||
        (request->arg(KEY_MONTH) == NULL || request->arg(KEY_MONTH).length()) ==
            0 ||
        (request->arg(KEY_DAY) == NULL || request->arg(KEY_DAY).length()) ==
            0 ||
        (request->arg(KEY_HOUR) == NULL || request->arg(KEY_HOUR).length()) ==
            0 ||
        (request->arg(KEY_MINUTE) == NULL ||
         request->arg(KEY_MINUTE).length()) == 0 ||
        (request->arg(KEY_SECOND) == NULL ||
         request->arg(KEY_SECOND).length()) == 0) {
        DEBUG_MSG_LN("onSetCurrentTime parse Error");
        request->send(500);
        return;
    }
    setTime(request->arg(KEY_HOUR).toInt(), request->arg(KEY_MINUTE).toInt(),
            request->arg(KEY_SECOND).toInt(), request->arg(KEY_DAY).toInt(),
            request->arg(KEY_MONTH).toInt(), request->arg(KEY_YEAR).toInt());
    if (_trapMesh->_mesh.getNodeList().size() == 0) {
        request->send(200);
        return;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &currentTime = jsonBuf.createObject();
    currentTime[KEY_CURRENT_TIME] = now();
    String msg;
    currentTime.printTo(msg);
    if (_trapMesh->_mesh.sendBroadcast(msg)) {
        request->send(200);
    } else {
        request->send(500);
    }
}

/**
 * 写真撮影
 */
void TrapServer::onSnapShot(AsyncWebServerRequest *request) {
    DEBUG_MSG_LN("onSnapShot");
    String temp = request->arg(KEY_PICTURE_FORMAT);
    int picFmt = -1;
    if (temp != NULL && temp.length() > 0) {
        picFmt = temp.toInt();
    }
    if (_trapMesh->snapCamera(picFmt)) {
        request->send(200, "image/jpeg", "image.jpg");
        return;
    }
    request->send(500);
}

/**
 * デバッグ用メッセージ送信
 */
void TrapServer::onSendMessage(AsyncWebServerRequest *request) {
    DEBUG_MSG_LN("onSendMessage");
    String msg = request->arg("messageContent");
    if (msg == NULL || msg.length() == 0) {
        return;
    }
    String nodeId = request->arg("messageSendNodeId");
    if (nodeId == NULL || nodeId.length() == 0) {
        if (_trapMesh->_mesh.getNodeList().size() == 0) {
            return;
        }
        DEBUG_MSG_LN("send debug message broadcast");
        _trapMesh->_mesh.sendBroadcast(msg);
    } else {
        uint32_t temp = (uint32_t)nodeId.toInt();
        DEBUG_MSG_LN("send debug message single");
        _trapMesh->_mesh.sendSingle(temp, msg);
    }
    // 自身にもメッセージがきたものとして扱う
    _trapMesh->receivedCallback(0, msg);
    DEBUG_MSG_LN("debug message:\n" + msg);
}

/**
 * GPS初期化
 */
void TrapServer::onInitGps(AsyncWebServerRequest *request) {
    DEBUG_MSG_LN("onInitGps");
    _trapMesh->initGps();
    String message = "{";
    message += KEY_INIT_GPS;
    message += ":\"InitGps\"}";
    if (_trapMesh->_mesh.getNodeList().size() == 0 ||
        _trapMesh->_mesh.sendBroadcast(message)) {
        request->send(200);
    } else {
        request->send(500);
    }
}

/**
 * GPS取得要求
 */
void TrapServer::onGetGps(AsyncWebServerRequest *request) {
    DEBUG_MSG_LN("onGetGps");
    if (_trapMesh->sendGetGps()) {
        request->send(200);
    } else {
        request->send(500);
    }
}
