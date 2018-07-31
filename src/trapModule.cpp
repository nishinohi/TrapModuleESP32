#include "trapModule.h"

/**************************************
 * setup
 * ***********************************/
// メッシュネットワークセットアップ
void TrapModule::setupMesh(const uint16_t types) {
    // set before init() so that you can see startup messages
    _mesh.setDebugMsgTypes(types);
    _mesh.init(MESH_SSID, MESH_PASSWORD, MESH_PORT);
    _mesh.onReceive(std::bind(&TrapModule::receivedCallback, this, std::placeholders::_1,
                              std::placeholders::_2));
    _mesh.onNewConnection(
        std::bind(&TrapModule::newConnectionCallback, this, std::placeholders::_1));
    _mesh.onChangedConnections(std::bind(&TrapModule::changedConnectionCallback, this));
    _mesh.onNodeTimeAdjusted(
        std::bind(&TrapModule::nodeTimeAdjustedCallback, this, std::placeholders::_1));
    _config._nodeId = _mesh.getNodeId();
}

// 実施タスクセットアップ
void TrapModule::setupTask() {
    // LED Setting
    _blinkNodesTask.set(BLINK_PERIOD, (_mesh.getNodeList().size() + 1) * 2,
                        std::bind(&TrapModule::blinkLed, this));
    _mesh.scheduler.addTask(_blinkNodesTask);
    _blinkNodesTask.enable();
    // module state 送信が輻輳しないよう送信間隔をランダムにする
    randomSeed(now());
    _sendModuleStateTask.set(random(DEF_INTERVAL, MODULE_STATE_INTERVAL), TASK_FOREVER,
                             std::bind(&TrapModule::sendModuleState, this));
    _mesh.scheduler.addTask(_sendModuleStateTask);
    // picture
    _sendPictureTask.set(DEF_INTERVAL, DEF_ITERATION,
                         std::bind(&TrapModule::sendPicture, this));
    _mesh.scheduler.addTask(_sendPictureTask);
    // battery check
    _checkBatteryLimitTask.set(BATTERY_CHECK_INTERVAL, TASK_FOREVER,
                               std::bind(&TrapModule::checkBatteryLimit, this));
    _mesh.scheduler.addTask(_checkBatteryLimitTask);
    // 設置モード時はバッテリーチェックを有効にする
    if (!_config._trapMode) {
        _checkBatteryLimitTask.enable();
    }
    // DeepSleep
    _deepSleepTask.set(SYNC_SLEEP_INTERVAL, TASK_FOREVER,
                       std::bind(&TrapModule::shiftDeepSleep, this));
    _mesh.scheduler.addTask(_deepSleepTask);
    // request Module State task
    _requestModuleStateTask.set(DEF_INTERVAL, TASK_FOREVER,
                                std::bind(&TrapModule::sendRequestModuleState, this));
    _mesh.scheduler.addTask(_requestModuleStateTask);
    // send GPS data task
    _sendGPSDataTask.set(DEF_INTERVAL, DEF_ITERATION, std::bind(&TrapModule::sendGpsData, this));
    _mesh.scheduler.addTask(_sendGPSDataTask);
    // send Sync Sleep task
    _sendSyncSleepTask.set(DEF_INTERVAL, TASK_FOREVER,
                           std::bind(&TrapModule::sendSyncSleep, this));
    _mesh.scheduler.addTask(_sendSyncSleepTask);
    // send Parent ModuleInfo Task
    _sendParentInfoTask.set(SEND_PARENT_INTERVAL, DEF_ITERATION,
                            std::bind(&TrapModule::sendParentModuleInfo, this));
    _mesh.scheduler.addTask(_sendParentInfoTask);
}

/**
 * 起動前チェック
 */
void TrapModule::checkStart() {
    updateBattery();
    if (_config._isBatteryDead) {
        DEBUG_MSG_LN("cannot start because battery already dead.");
        shiftDeepSleep();
    }
    if (_config._trapMode && now() < _config._wakeTime) {
        DEBUG_MSG_LN("cannot start because current time is before waketime.");
        shiftDeepSleep();
    }
}

/**
 * 罠モード開始
 */
void TrapModule::startTrapMode() {
    DEBUG_MSG_LN("startTrapMode");
    // 罠モード開始
    _config.updateParentState();
    if (!_config._isParent) {
        return;
    }
    _config.updateNodeNum(_mesh.getNodeList());
    // 単独稼働の場合はそのまま終了
    if (_config._nodeNum == 0) {
        shiftDeepSleep();
        return;
    }
    _requestModuleStateTask.enableDelayed(2000);
}

/********************************************
 * loop メソッド
 *******************************************/
/**
 * 監視機能はタスクで管理したほうがいいかもしれない
 **/
void TrapModule::update() {
    _mesh.update();
    // 確認用 LED
    if (_config._trapMode) {
        digitalWrite(LED, HIGH);
    } else {
        digitalWrite(LED, !_config._ledOnFlag);
    }
    // 罠モード開始時に一度だけ実行する
    if (_config._isTrapStart && !_config._isStarted) {
        _config._isStarted = true;
        startTrapMode();
    }
    // 設置モードか罠モード作動開始状態の場合は以降の処理は無視
    if (!_config._trapMode || _config._isTrapStart) {
        return;
    }
    // 単独稼働の場合はそのまま終了
    if (_config._nodeNum == 0) {
        shiftDeepSleep();
        return;
    }
    // 稼働時間超過により強制 DeepSleep
    if (now() - _config._wakeTime > _config._workTime) {
        if (_deepSleepTask.isEnabled()) {
            return;
        }
        DEBUG_MSG_LN("work time limit.");
        _sendSyncSleepTask.disable();
        _config.updateNodeNum(_mesh.getNodeList());
        _deepSleepTask.enableDelayed(SYNC_SLEEP_INTERVAL);
    }
}

/********************************************
 * モジュール設定値操作
 *******************************************/
// モジュールパラメータ設定
bool TrapModule::setConfig(JsonObject &config) {
    if (!config.success()) {
        DEBUG_MSG_LN("json parse failed");
        return false;
    }
    config[KEY_CONFIG_UPDATE] = true;
    if (syncAllModuleConfigs(config)) {
        _config.updateModuleConfig(config);
        return _config.saveCurrentModuleConfig();
    }
    return false;
}

// 現在時刻設定
bool TrapModule::setCurrentTime(time_t current) {
    setTime(current);
    DEBUG_MSG_F("current time:%d/%d/%d %d:%d:%d\n", year(), month(), day(), hour(), minute(),
                second());
    return syncCurrentTime();
}

// GPS 初期化
bool TrapModule::initGps() {
    _config.initGps();
    if (_mesh.getNodeList().size() == 0) {
        return true;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &obj = jsonBuf.createObject();
    obj[KEY_CONFIG_UPDATE] = true;
    obj[KEY_INIT_GPS] = KEY_INIT_GPS;
    return sendBroadcast(obj);
}

/********************************************
 * モジュール情報取得１
 *******************************************/
/**
 * GPS取得
 * GPS 取得タスクを開始するだけなので、必ず成功になる
 */
bool TrapModule::getGps() {
    DEBUG_MSG_LN("onGetGps");
    if (_config._isParent) {
        return beginMultiTask(CELLULAR_TASK_NAME, TrapModule::getGpsTask, _taskHandle[1], this, 3);
    }
    return sendGetGps();
}

/**
 * GPS 取得
 */
void TrapModule::getGpsTask(void *arg) {
    TrapModule *pTrapModule = reinterpret_cast<TrapModule *>(arg);
    Scheduler cellularRunner;
    cellularRunner.addTask(pTrapModule->_cellular._getGPSDataTask);
    pTrapModule->_cellular._getGPSDataTask.enable();
    while (1) {
        // 取得メソッド終了
        if (!pTrapModule->_cellular._getGPSDataTask.isEnabled()) {
            size_t latLen = strlen(pTrapModule->_cellular._lat);
            size_t lonLen = strlen(pTrapModule->_cellular._lon);
            if (latLen != 0 && lonLen != 0) {
                pTrapModule->_config.initGps();
                memcpy(pTrapModule->_config._lat, pTrapModule->_cellular._lat, latLen);
                memcpy(pTrapModule->_config._lon, pTrapModule->_cellular._lon, lonLen);
            }
            pTrapModule->_cellular._getGPSDataTask.setIterations(GPS_TRY_COUNT);
            pTrapModule->_cellular._getGPSDataTask.enable();
            vTaskSuspend(pTrapModule->_taskHandle[1]);
        }
        TASK_DELAY(1);
        cellularRunner.execute();
    }
}

/********************************************
 * painlessMesh callback
 *******************************************/
/**
 * モジュールからメッセージがあった場合のコールバック
 */
void TrapModule::receivedCallback(uint32_t from, String &msg) {
    DEBUG_MSG_LN("Received message.\nMessage:" + msg);
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &msgJson = jsonBuf.parseObject(msg);
    if (!msgJson.success()) {
        DEBUG_MSG_LN("json parse failed");
        return;
    }
    // モジュール設定更新メッセージ受信
    if (msgJson.containsKey(KEY_CONFIG_UPDATE)) {
        DEBUG_MSG_LN("Module config update");
        _config.updateModuleConfig(msgJson);
        _config.saveCurrentModuleConfig();
    }
    // 親モジュール情報
    if (msgJson.containsKey(KEY_PARENT_INFO)) {
        DEBUG_MSG_LN("parent module info");
        _config.pushNoDuplicateNodeId(msgJson[KEY_PARENT_NODE_ID], _config._parentNodeIdList);
    }
    // モジュール状態送信要求が来た場合は送信済みか否かにかかわらず送信する
    if (msgJson.containsKey(KEY_REQUEST_MODULE_STATE)) {
        DEBUG_MSG_LN("request module state");
        moduleStateTaskStart();
    }
    // モジュール状態受信
    if (msgJson.containsKey(KEY_MODULE_STATE)) {
        DEBUG_MSG_LN("get module state");
        _config.pushNoDuplicateModuleState(msgJson);
        // 全てのモジュールからモジュール状態を受信したら deepSleep メッセージ送信
        if (_config._moduleStateList.size() >= _config._nodeNum) {
            _config.updateNodeNum(_mesh.getNodeList());
            _sendSyncSleepTask.enable();
        }
    }
    // 画像保存
    if (msgJson.containsKey(KEY_PICTURE)) {
        DEBUG_MSG_LN("image receive");
        saveBase64Image((const char *)msgJson[KEY_PICTURE]);
    }
    // DeepSleepする前に全ノードのバッテリー状態などを取得している必要があるので最後に呼ぶこと
    if (msgJson.containsKey(KEY_SYNC_SLEEP)) {
        DEBUG_MSG_LN("Sync Sleep start");
        moduleStateTaskStop();
        _deepSleepTask.enableDelayed(SYNC_SLEEP_INTERVAL);
    }
}

/**
 * メッシュネットワークに新規のモジュールが追加されたとき
 * 罠モード時に親モジュールがメッシュ内に存在していてかつモジュール状態が未送信なら送信する
 */
void TrapModule::newConnectionCallback(uint32_t nodeId) {
    DEBUG_MSG_F("--> startHere: New Connection, nodeId = %u\n", nodeId);
    refreshMeshDetail();
    if (_config._isParent) {
        // 何度も broadcast する必要はないので遅延送信
        _sendParentInfoTask.enableDelayed(SEND_PARENT_INTERVAL);
        return;
    }
    // 以降は子モジュールの動作
    // 設置モードのときは何もしない
    if (!_config._trapMode || _config._isTrapStart) {
        return;
    }
    if (_config._isSendModuleState) {
        return;
    }
    for (auto &nodeId : _mesh.getNodeList()) {
        if (nodeId == _config._parentNodeId) {
            moduleStateTaskStart();
            break;
        }
    }
}

/**
 * ネットワーク（トポロジ）状態変化
 * 罠モード時に親モジュールがメッシュ内に存在していてかつモジュール状態が未送信なら送信する
 */
void TrapModule::changedConnectionCallback() {
    DEBUG_MSG_F("Changed connections %s\n", _mesh.subConnectionJson().c_str());
    refreshMeshDetail();
    if (_config._isParent) {
        // 何度も broadcast する必要はないので遅延送信
        _sendParentInfoTask.enableDelayed(SEND_PARENT_INTERVAL);
        return;
    }
    // 以降は子モジュールの動作
    // 設置モードのときは何もしない
    if (!_config._trapMode || _config._isTrapStart) {
        return;
    }
    if (_config._isSendModuleState) {
        return;
    }
    for (auto &nodeId : _mesh.getNodeList()) {
        if (nodeId == _config._parentNodeId) {
            moduleStateTaskStart();
            break;
        }
    }
}

void TrapModule::nodeTimeAdjustedCallback(int32_t offset) {
    DEBUG_MSG_F("Adjusted time %u. Offset = %d\n", _mesh.getNodeTime(), offset);
}

/*******************************************************
 * センサ情報
 ******************************************************/
/**
 * バッテリー状態更新
 */
void TrapModule::updateBattery() {
    DEBUG_MSG_LN("updateBattery");
#ifdef BATTERY_CHECK_ACTIVE
    uint16_t battery = analogRead(A0);
    battery = battery * VOLTAGE_DIVIDE;
    _config._isBatteryDead = battery < BATTERY_LIMIT;
#else
    _config._isBatteryDead = false;
#endif
}

/**
 * 罠作動状態更新
 */
void TrapModule::updateTrapFire() {
    DEBUG_MSG_LN("updateTrapFire");
#ifdef TRAP_CHECK_ACTIVE
    _config._trapFire = digitalRead(TRAP_CHECK_PIN) == HIGH;
#else
    _config._trapFire = false;
#endif
}

/*******************************************************
 * メッセージング
 ******************************************************/
/**
 * 設定値を全モジュールに同期する
 **/
bool TrapModule::syncAllModuleConfigs(JsonObject &config) {
    DEBUG_MSG_LN("syncAllModuleConfigs");
    if (_mesh.getNodeList().size() == 0) {
        return true;
    }
    return sendBroadcast(config);
}

// 現在時刻同期
bool TrapModule::syncCurrentTime() {
    DEBUG_MSG_LN("syncCurrentTime");
    if (_mesh.getNodeList().size() == 0) {
        return true;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &currentTime = jsonBuf.createObject();
    currentTime[KEY_CONFIG_UPDATE] = true;
    currentTime[KEY_CURRENT_TIME] = now();
    return sendBroadcast(currentTime);
}

/**
 * GPS 取得要求を親モジュールに送信
 **/
bool TrapModule::sendGetGps() {
    DEBUG_MSG_LN("sendGetGps");
    if (_config._parentNodeId == DEF_NODEID) {
        DEBUG_MSG_LN("parent module not found");
        return false;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &gpsGet = jsonBuf.createObject();
    gpsGet[KEY_GET_GPS] = true;
    return sendParent(gpsGet);
}

/**
 * モジュール状態送信
 * [送信内容]
 * 1:トラップ作動状態、2:バッテリー残量、3:バッテリー切れ、4:カメラ有無
 */
void TrapModule::sendModuleState() {
    DEBUG_MSG_LN("sendModuleState");
    if (_config._parentNodeId == DEF_NODEID) {
        DEBUG_MSG_LN("parent module not found");
        moduleStateTaskStop();
        return;
    }
    // センサ情報更新
    updateBattery();
    updateTrapFire();
    // モジュール状態情報作成
    JsonObject &state = _config.getModuleState();
    if (sendParent(state)) {
        _config._isSendModuleState = true;
        moduleStateTaskStop();
    }
    // 送信に成功しなかった場合輻輳を避けるため送信間隔を変更
    randomSeed(now());
    _sendModuleStateTask.setInterval(random(DEF_INTERVAL, MODULE_STATE_INTERVAL));
}

/**
 * 撮影画像を送信する
 */
void TrapModule::sendPicture() {
    DEBUG_MSG_LN("sendPicture");
    // 送信先がいなければ何もしないz
    if (_mesh.getNodeList().size() == 0) {
        _sendPictureTask.disable();
        return;
    }
    if (!SPIFFS.exists(DEF_IMG_PATH)) {
        return;
    }
    File file = SPIFFS.open(DEF_IMG_PATH, "r");
    size_t size = file.size();
    if (size == 0) {
        file.close();
        return;
    }
    DEBUG_MSG_LN("read picture");
    char *buf = (char *)malloc(size);
    file.readBytes(buf, size);
    file.close();
    int encLen = base64_enc_len(size);
    char *enc = (char *)malloc(encLen + 1);
    base64_encode(enc, buf, size);
    free(buf);
    DEBUG_MSG_LN("encoded");
    // create json message
    DEBUG_MSG_F("FreeHeepMem:%lu\n", ESP.getFreeHeap());
    String temp(enc);
    free(enc);
    String msg = "{\"";
    msg += KEY_PICTURE;
    msg = msg + "\":\"" + temp + "\"}";
    DEBUG_MSG_F("FreeHeepMem:%lu\n", ESP.getFreeHeap());
    DEBUG_MSG_F("msgLength:%d\n", msg.length());
    DEBUG_MSG_LN(msg);
    if (_mesh.sendBroadcast(msg)) {
        DEBUG_MSG_LN("send picture success");
        _sendPictureTask.disable();
    } else {
        if (_sendPictureTask.isLastIteration()) {
            DEBUG_MSG_LN("send picture failed");
        } else {
            DEBUG_MSG_LN("retry send picture...");
        }
    }
}

/**
 * 同期停止メッセージ送信
 */
void TrapModule::sendSyncSleep() {
    DEBUG_MSG_LN("sendSyncSleep");
    if (_mesh.getNodeList().size() == 0) {
        _sendSyncSleepTask.disable();
        _deepSleepTask.enableDelayed(SYNC_SLEEP_INTERVAL);
        return;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &syncObj = jsonBuf.createObject();
    syncObj[KEY_SYNC_SLEEP] = true;
    if (sendBroadcast(syncObj)) {
        _sendSyncSleepTask.disable();
        _deepSleepTask.enableDelayed(SYNC_SLEEP_INTERVAL);
    }
}

/**
 * 親機の nodeId を送信する
 * メッシュネットワークに更新があった場合に送信する
 */
void TrapModule::sendParentModuleInfo() {
    DEBUG_MSG_LN("sendParentModuleInfo");
    if (_mesh.getNodeList().size() == 0) {
        _sendParentInfoTask.disable();
        return;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &obj = jsonBuf.createObject();
    if (_config._trapMode) {
        // 罠モードの場合は親モジュールとの現在時刻同期を実行
        obj[KEY_CONFIG_UPDATE] = true;
        obj[KEY_CURRENT_TIME] = now();
    } else {
        // 設置モード時は親フラグ更新判定で使用する親モジュール ID を送信する
        obj[KEY_PARENT_INFO] = true;
        obj[KEY_PARENT_NODE_ID] = getNodeId();
    }
    if (sendBroadcast(obj)) {
        _sendParentInfoTask.disable();
    }
}

/**
 * モジュール状態送信要求
 */
void TrapModule::sendRequestModuleState() {
    DEBUG_MSG_LN("sendRequestModuleState");
    if (_mesh.getNodeList().size() == 0) {
        _requestModuleStateTask.disable();
        return;
    }
    JsonObject &obj = _config.getModuleConfig();
    obj[KEY_REQUEST_MODULE_STATE] = true;
    // モジュール状態の送信対象の親モジュール ID を送信
    obj[KEY_CONFIG_UPDATE] = true;
    obj[KEY_PARENT_NODE_ID] = getNodeId();
    if (sendBroadcast(obj)) {
        _requestModuleStateTask.disable();
    }
}

/**
 * GPS データ送信
 */
void TrapModule::sendGpsData() {
    DEBUG_MSG_LN("sendGpsData");
    if (_mesh.getNodeList().size() == 0) {
        _sendGPSDataTask.disable();
        return;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &gpsData = jsonBuf.createObject();
    gpsData[KEY_CONFIG_UPDATE] = true;
    gpsData[KEY_GPS_LAT] = _config._lat;
    gpsData[KEY_GPS_LON] = _config._lon;
    gpsData[KEY_CURRENT_TIME] = now();
    if (sendBroadcast(gpsData)) {
        _sendGPSDataTask.disable();
    }
}

/*************************************
 * タスク関連
 ************************************/
// 接続モジュール数 LED 点滅
void TrapModule::blinkLed() {
    if (_config._ledOnFlag) {
        _config._ledOnFlag = false;
    } else {
        _config._ledOnFlag = true;
    }
    _blinkNodesTask.delay(BLINK_DURATION);
    if (_blinkNodesTask.isLastIteration()) {
        _blinkNodesTask.setIterations((_mesh.getNodeList().size() + 1) * 2);
        _blinkNodesTask.enableDelayed(BLINK_PERIOD -
                                      (_mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);
    }
}

/**
 * DeepSleepモードに移行する
 * 次の稼働開始時刻と次の起動時刻を更新し現在の設定値を保存してDeepSleepする
 * バッテリー切れの場合は完全終了
 */
void TrapModule::shiftDeepSleep() {
    DEBUG_MSG_LN("mesh Stop");
    _mesh.stop();
    WiFi.mode(WIFI_OFF);
    unsigned long dif = millis();
    while (millis() - dif < 5000) {
        delay(100);
        yield();
        if (WiFi.status() == WL_DISCONNECTED) {
            break;
        }
    }
    // 検知情報などのモジュール情報を送信
    if (_config._isParent) {
        if (_config._isTrapStart) {
            sendTrapStartInfo();
        } else {
            sendTrapUpdateInfo();
        }
    }
    DEBUG_MSG_LN("Shift Deep Sleep");
    if (_config._isBatteryDead) {
        DEBUG_MSG_LN("Battery limit!\nshutdown...");
#ifdef ESP32
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);
        esp_deep_sleep_start();
#else
        ESP.deepSleep(0);
#endif
        return;
    }
    _config.setWakeTime();
    time_t tNow = now();
    // 次回起動時の現在時刻（現在時刻 + DeepSleep時間）をセットする
    _config._currentTime = tNow + _config.calcSleepTime(tNow, _config._wakeTime);
    _config.saveCurrentModuleConfig();
#ifdef DEBUG_ESP_PORT
    time_t temp = now();
    setTime(_config._wakeTime);
    DEBUG_MSG_F("wakeTime:%d/%d/%d %d:%d:%d\n", year(), month(), day(), hour(), minute(), second());
    setTime(_config._currentTime);
    DEBUG_MSG_F("currentTime:%d/%d/%d %d:%d:%d\n", year(), month(), day(), hour(), minute(),
                second());
    setTime(temp);
#endif
    // calcSleepTime() の返り値をそのままESP.deepSleep()の返り値にすると変になる
    uint64_t deepSleepTime = _config.calcSleepTime(now(), _config._wakeTime);
    deepSleepTime = deepSleepTime * 1000000L;
#ifdef ESP32
    esp_sleep_enable_timer_wakeup(deepSleepTime);
    esp_deep_sleep_start();
    return;
#else
    ESP.deepSleep(deepSleepTime);
#endif
}

/**
 * バッテリー残量が限界値を超えたら矯正シャットダウンさせる
 * 基本的に設置モード時のみ有効にする想定
 */
void TrapModule::checkBatteryLimit() {
    DEBUG_MSG_LN("checkBattery");
    updateBattery();
    if (_config._isBatteryDead) {
        _checkBatteryLimitTask.disable();
        shiftDeepSleep();
    }
}

/********************************************
 * Camera メソッド
 *******************************************/
/**
 * カメラスナップショット要求
 * TODO: タスク完了後の bool 値を判定に使用したい
 */
bool TrapModule::snapCamera(int resolution) {
    DEBUG_MSG_LN("snapCamera");
    if (!_config._cameraEnable) {
        DEBUG_MSG_LN("camera cannot use");
        return false;
    }
    // 画像転送タスク実行中はカメラ撮影は実行しない
    if (_sendPictureTask.isEnabled()) {
        DEBUG_MSG_LN("camera cannot use because picture send task is running");
        return false;
    }
    _camera.setResolution(resolution);
    // タスク作成前の場合はタスクを作成
    if (strncmp(pcTaskGetTaskName(_taskHandle[0]), CAMERA_TASK_NAME, strlen(CAMERA_TASK_NAME)) !=
        0) {
        xTaskCreatePinnedToCore(TrapModule::snapCameraTask, CAMERA_TASK_NAME, TASK_MEMORY, this, 2,
                                &_taskHandle[0], 0);
        return true;
    } else {
        if (eTaskGetState(_taskHandle[0]) == eSuspended) {
            vTaskResume(_taskHandle[0]);
            return true;
        }
    }
    return false;
}

/**
 * カメラ撮影タスク
 */
void TrapModule::snapCameraTask(void *arg) {
    TrapModule *trapModule = reinterpret_cast<TrapModule *>(arg);
    while (1) {
        if (trapModule->_camera.saveCameraData()) {
            DEBUG_MSG_LN("snap success");
            // メッセージ送信タスク実行中でなければ送信タスク開始
            trapModule->_sendPictureTask.setIterations(5);
            trapModule->_sendPictureTask.enable();
        } else {
            DEBUG_MSG_LN("snap failed");
        }
        DEBUG_MSG_LN("camera task suspend");
        vTaskSuspend(trapModule->_taskHandle[0]);
        TASK_DELAY(1);
    }
}

/*************************************
 * MQTT
 ************************************/
/**
 * 罠モード開始時の設定値等を含んだ情報をサーバーに送信
 */
void TrapModule::sendTrapStartInfo() {
    DEBUG_MSG_LN("sendTrapStartInfo");
    JsonObject &info = _config.getTrapStartInfo(_mesh);
    String infoStr;
    info.printTo(infoStr);
    _cellular.sendTrapModuleInfo(infoStr);
}

/**
 * 稼働中の各種情報を送信
 */
void TrapModule::sendTrapUpdateInfo() {
    DEBUG_MSG_LN("sendTrapUpdateInfo");
    JsonObject &info = _config.getTrapStartInfo(_mesh);
    String infoStr;
    info.printTo(infoStr);
    _cellular.sendTrapModuleInfo(infoStr);
}

/*************************************
 * Debug
 ************************************/
// debug メッセージ送信
bool TrapModule::sendDebugMesage(String msg, uint32_t nodeId) {
    if (nodeId != 0) {
        DEBUG_MSG_LN("send debug message single");
        return _mesh.sendSingle(nodeId, msg);
    }
    DEBUG_MSG_LN("send debug message broadcast");
    if (_mesh.getNodeList().size() == 0) {
        receivedCallback(getNodeId(), msg);
        return true;
    }
    return _mesh.sendBroadcast(msg);
}

/*************************************
 * Util
 ************************************/
/**
 * Base64 エンコードされた画像データを保存する
 * ファイル名が NULL ならデフォルト名を使用する
 */
void TrapModule::saveBase64Image(const char *data, const char *name) {
    int inputLen = strlen(data);
    int decLen = base64_dec_len((char *)data, inputLen);
    char *dec = (char *)malloc(decLen + 1);
    base64_decode(dec, (char *)data, inputLen);
    File img = SPIFFS.open(name == NULL ? DEF_IMG_PATH : name, "w");
    img.write((const uint8_t *)dec, decLen + 1);
    img.close();
    free(dec);
}

/**
 * メッシュネットワーク更新時の LED リセットと接続状態表示
 */
void TrapModule::refreshMeshDetail() {
    _config._ledOnFlag = false;
    _blinkNodesTask.setIterations((_mesh.getNodeList().size() + 1) * 2);
    _blinkNodesTask.enableDelayed(BLINK_PERIOD -
                                  (_mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);
    SimpleList<uint32_t> nodes = _mesh.getNodeList();
    // 接続情報表示
    DEBUG_MSG_F("Num nodes: %d\n", nodes.size());
    DEBUG_MSG_F("Connection list:");
    for (auto &node : nodes) {
        DEBUG_MSG_F(" %u", node);
    }
    DEBUG_MSG_LN();
}

/**
 * 任意のタスクを開始
 */
bool TrapModule::beginMultiTask(const char *taskName, TaskFunction_t func, TaskHandle_t taskHandle,
                                void *arg, const uint8_t priority, const uint8_t core) {
    // タスク作成前の場合はタスクを作成
    if (strncmp(pcTaskGetTaskName(taskHandle), taskName, strlen(taskName)) != 0) {
        xTaskCreatePinnedToCore(func, taskName, TASK_MEMORY, arg, priority, &(taskHandle), core);
        return true;
    } else {
        if (eTaskGetState(taskHandle) == eSuspended) {
            vTaskResume(taskHandle);
        }
        return true;
    }
    return false;
}
