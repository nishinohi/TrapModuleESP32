#include "trapModule.h"

// singleton
TrapModule *TrapModule::_pTrapModule = NULL;

/**************************************
 * setup
 * ***********************************/
void TrapModule::setupModule() {
    pinMode(TRAP_CHECK_PIN, INPUT);
    pinMode(FORCE_SETTING_MODE_PIN, INPUT);
    pinMode(LED, OUTPUT);
    // モジュール読み込み
    loadModuleConfig();
    // // 起動前チェック
    if (!checkBeforeStart()) {
        shiftDeepSleep();
    }
    DEBUG_MSG_LN("camera setup");
    setupCamera();
    DEBUG_MSG_LN("mesh setup");
    setupMesh(CONNECTION | SYNC); // painlessmesh 1.3v error
    setupTask();
    // 現在時刻調整
    if (isTrapMode()) {
        adjustCurrentTimeFromNTP();
    }
}

/**
 * メッシュネットワークセットアップ
 */
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
    _pConfig->_nodeId = _mesh.getNodeId();
}

/**
 * カメラセットアップ
 */
void TrapModule::setupCamera() {
    _pCamera->cameraSerialBegin();
    delay(10);
    _pConfig->_cameraEnable = _pCamera->initialize();
    if (_pConfig->_cameraEnable) {
        xTaskCreatePinnedToCore(TrapModule::snapCameraTask, CAMERA_TASK_NAME, TASK_MEMORY, NULL, 2,
                                &_taskHandle[0], 0);
    }
}

/**
 * 実施タスクセットアップ
 */
void TrapModule::setupTask() {
    // LED Setting
    setTask(_blinkNodesTask, BLINK_PERIOD, (_mesh.getNodeList().size() + 1) * 2,
            std::bind(&TrapModule::blinkLed, this), true);
    // module state 送信が輻輳しないよう送信間隔をランダムにする
    randomSeed(now());
    setTask(_sendModuleStateTask, random(DEF_INTERVAL, MODULE_STATE_INTERVAL), TASK_FOREVER,
            std::bind(&TrapModule::sendModuleState, this), false);
    // picture
    setTask(_sendPictureTask, DEF_INTERVAL, DEF_ITERATION,
            std::bind(&TrapModule::sendPicture, this), false);
    // battery check
    // 設置モード時はバッテリーチェックを有効にする
    setTask(_checkBatteryLimitTask, BATTERY_CHECK_INTERVAL, TASK_FOREVER,
            std::bind(&TrapModule::checkBatteryLimit, this), !_pConfig->_trapMode);
    // request Module State task
    setTask(_requestModuleStateTask, MODULE_STATE_INTERVAL, TASK_FOREVER,
            std::bind(&TrapModule::sendRequestModuleState, this), false);
    // send Sync Sleep task
    setTask(_sendSyncSleepTask, DEF_INTERVAL, TASK_FOREVER,
            std::bind(&TrapModule::sendSyncSleep, this), false);
    // send Parent ModuleInfo Task
    setTask(_sendParentInfoTask, SEND_PARENT_INTERVAL, DEF_ITERATION,
            std::bind(&TrapModule::sendParentModuleInfo, this), false);
}

/**
 * 起動前チェック
 * バッテリー残量がない場合終了
 * 起動時刻が稼働時刻範囲内じゃない場合終了
 */
bool TrapModule::checkBeforeStart() {
    DEBUG_MSG_LN("checkBeforeStart.");
    // バッテリー残量チェック
    updateBattery();
    if (_pConfig->_isBatteryDead) {
        DEBUG_MSG_LN("cannot start because battery already dead.");
        return false;
    }
    // 設置モードの場合はバッテリー残量チェックのみ
    if (!_pConfig->_trapMode) {
        return true;
    }
    // 起動時刻チェック
    tmElements_t activeStart;
    breakTime(_pConfig->_wakeTime, activeStart);
    activeStart.Hour = _pConfig->_activeStart;
    activeStart.Minute = 0;
    activeStart.Second = 0;
    tmElements_t activeEnd = activeStart;
    activeEnd.Hour = _pConfig->_activeEnd;
    if (_pConfig->_wakeTime < makeTime(activeStart) || _pConfig->_wakeTime > makeTime(activeEnd)) {
        DEBUG_MSG_LN("cannot start because current time is not active time.");
        return false;
    }
    return true;
}

/**
 * 罠モード開始
 * 自身が親モジュールとして動作するかチェックした後
 * このジュールに状態情報送信要求を送信する
 */
void TrapModule::startTrapMode() {
    DEBUG_MSG_LN("startTrapMode");
    // 罠モード開始
    _pConfig->updateParentState();
    if (!_pConfig->_isParent) {
        return;
    }
    _pConfig->updateNodeNum(_mesh.getNodeList());
    taskStart(_requestModuleStateTask);
}

/**
 * 現在時刻をNTPサーバーから取得した時刻に設定する
 */
bool TrapModule::adjustCurrentTimeFromNTP() {
    if (!startModule()) {
        return false;
    }
    time_t ntpTime = _pCellular->getTime();
    if (ntpTime == 0) {
        return false;
    }
    setTime(ntpTime);
    stopModule();
    return true;
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
    if (_pConfig->_trapMode) {
        digitalWrite(LED, HIGH);
    } else {
        digitalWrite(LED, !_pConfig->_ledOnFlag);
    }
    // 罠モード開始時に一度だけ実行する
    if (_pConfig->_isTrapStart && !_pConfig->_isStarted) {
        _pConfig->_isStarted = true;
        startTrapMode();
    }
    // DeepSleep 開始
    if (_pConfig->_isSleep) {
        delay(SYNC_SLEEP_INTERVAL);
        shiftDeepSleep();
    }
    // 設置モードの場合は以降の処理は無視
    if (!_pConfig->_trapMode) {
        return;
    }
    // 単独稼働の場合はそのまま終了
    if (_pConfig->_nodeNum == 0) {
        startSyncSleeptask();
        return;
    }
    // 罠モード開始時は稼働時間超過は無視
    if (_pConfig->_isTrapStart) {
        return;
    }
    // 稼働時間超過により強制 DeepSleep
    if (millis() > WORK_TIME) {
        DEBUG_MSG_LN("work time limit.");
        _pConfig->_isSleep = true;
    }
}

/********************************************
 * モジュール設定値操作
 *******************************************/
/**
 * モジュール設定値同期
 * 同期に成功して初めて自身の設定値を更新する
 */
bool TrapModule::syncConfig(JsonObject &config) {
    config[KEY_CONFIG_UPDATE] = true;
    if (_mesh.getNodeList().size() > 0) {
        if (!sendBroadcast(config)) {
            return false;
        }
    }
    _pConfig->updateModuleConfig(config);
    return true;
}

/**
 * 現在時刻設定
 */
bool TrapModule::syncCurrentTime(time_t current) {
    setTime(current);
    DEBUG_MSG_F("current time:%d/%d/%d %d:%d:%d\n", year(), month(), day(), hour(), minute(),
                second());
    return sendCurrentTime();
}

/**
 * GPS 初期化
 */
bool TrapModule::initGps() {
    _pConfig->initGps();
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
        _pConfig->updateModuleConfig(msgJson);
    }
    // モジュール状態送信要求が来た場合は送信済みか否かにかかわらず送信する
    if (msgJson.containsKey(KEY_REQUEST_MODULE_STATE)) {
        DEBUG_MSG_LN("request module state");
        taskStart(_sendModuleStateTask);
    }
    // 親モジュール情報
    if (msgJson.containsKey(KEY_PARENT_INFO)) {
        DEBUG_MSG_LN("parent module info");
        _pConfig->pushNoDuplicateNodeId(msgJson[KEY_PARENT_NODE_ID], _pConfig->_parentNodeIdList);
    }
    // モジュール状態受信(設置モード時は無視)
    if (msgJson.containsKey(KEY_MODULE_STATE) && _pConfig->_trapMode) {
        DEBUG_MSG_LN("get module state");
        _pConfig->pushNoDuplicateModuleState(from, msgJson);
        // 全てのモジュールからモジュール状態を受信したら次回起動時刻を決定しDeepSleep要求送信
        if (_pConfig->_moduleStateList.size() >= _pConfig->_nodeNum) {
            taskStop(_requestModuleStateTask);
            startSyncSleeptask();
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
        _pConfig->_isSleep = true;
    }
}

/**
 * メッシュネットワークに新規のモジュールが追加されたとき
 * 罠モード時に親モジュールがメッシュ内に存在していてかつモジュール状態が未送信なら送信する
 */
void TrapModule::newConnectionCallback(uint32_t nodeId) {
    DEBUG_MSG_F("--> startHere: New Connection, nodeId = %u\n", nodeId);
    refreshMeshDetail();
    if (_pConfig->_isParent && !_pConfig->_trapMode) {
        // 何度も broadcast する必要はないので遅延送信
        taskStart(_sendParentInfoTask, SEND_PARENT_INTERVAL);
        return;
    }
    // 以降は子モジュールと同じ
    startSendModuleState();
}

/**
 * ネットワーク（トポロジ）状態変化
 * 罠モード時に親モジュールがメッシュ内に存在していてかつモジュール状態が未送信なら送信する
 */
void TrapModule::changedConnectionCallback() {
    DEBUG_MSG_F("Changed connections %s\n", _mesh.subConnectionJson().c_str());
    refreshMeshDetail();
    if (_pConfig->_isParent && !_pConfig->_trapMode) {
        // 何度も broadcast する必要はないので遅延送信
        taskStart(_sendParentInfoTask, SEND_PARENT_INTERVAL);
        return;
    }
    // 以降は子モジュールと同じ
    startSendModuleState();
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
    DEBUG_MSG("updateBattery: ");
#ifdef BATTERY_CHECK_ACTIVE
    uint16_t battery = analogRead(A0);
    double realBattery = REAL_BATTERY_VALUE(battery);
    DEBUG_MSG_F("%f\n", realBattery);
    battery = battery * VOLTAGE_DIVIDE;
    _pConfig->_isBatteryDead = battery < BATTERY_LIMIT;
#else
    _pConfig->_isBatteryDead = false;
    DEBUG_MSG_LN("unavailable");
#endif
}

/**
 * 罠作動状態更新
 */
void TrapModule::updateTrapFire() {
    DEBUG_MSG_LN("updateTrapFire");
#ifdef TRAP_CHECK_ACTIVE
    _pConfig->_trapFire = digitalRead(TRAP_CHECK_PIN) == HIGH;
#else
    _pConfig->_trapFire = false;
#endif
}

/*******************************************************
 * メッセージング
 ******************************************************/
/**
 * 現在時刻同期
 */
bool TrapModule::sendCurrentTime() {
    DEBUG_MSG_LN("sendCurrentTime");
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
 * モジュール状態送信
 * [送信内容]
 * 1:トラップ作動状態、2:バッテリー残量、3:バッテリー切れ、4:カメラ有無
 */
void TrapModule::sendModuleState() {
    DEBUG_MSG_LN("sendModuleState");
    if (_pConfig->_parentNodeId == DEF_NODEID) {
        DEBUG_MSG_LN("parent module not found");
        taskStop(_sendModuleStateTask);
        return;
    }
    // センサ情報更新
    updateBattery();
    updateTrapFire();
    // モジュール状態情報作成
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &state = jsonBuf.createObject();
    _pConfig->collectModuleState(state);
    if (sendParent(state)) {
        _pConfig->_isSendModuleState = true;
        taskStop(_sendModuleStateTask);
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
        taskStop(_sendPictureTask);
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
        taskStop(_sendPictureTask);
        return;
    }
    DEBUG_MSG_LN(_sendPictureTask.isLastIteration() ? "send picture failed"
                                                    : "retry send picture...");
}

/**
 * DeepSleepメッセージ送信
 * 次回起動時刻もここで合わせて送信する
 */
void TrapModule::sendSyncSleep() {
    DEBUG_MSG_LN("sendSyncSleep");
    // 単独起動の場合終了
    if (_mesh.getNodeList().size() == 0) {
        taskStop(_sendSyncSleepTask);
        _pConfig->_isSleep = true;
        return;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &syncObj = jsonBuf.createObject();
    syncObj[KEY_CONFIG_UPDATE] = true;
    syncObj[KEY_CURRENT_TIME] = now();
    syncObj[KEY_WAKE_TIME] = _pConfig->_wakeTime;
    syncObj[KEY_SYNC_SLEEP] = true;
    if (sendBroadcast(syncObj)) {
        taskStop(_sendSyncSleepTask);
        _pConfig->_isSleep = true;
    }
}

/**
 * 複数の親機があった場合のために親機宛に親機情報を送信する
 * モジュール設定値の親機IDを更新するわけではないので注意
 */
void TrapModule::sendParentModuleInfo() {
    DEBUG_MSG_LN("sendParentModuleInfo");
    if (_mesh.getNodeList().size() == 0) {
        taskStop(_sendParentInfoTask);
        return;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &obj = jsonBuf.createObject();
    obj[KEY_PARENT_INFO] = true;
    obj[KEY_PARENT_NODE_ID] = getNodeId();
    if (sendBroadcast(obj)) {
        taskStop(_sendParentInfoTask);
    }
}

/**
 * モジュール状態送信要求を送信
 * 送る対象の親機IDを送信する
 * メッシュノード全ての状態情報を取得したタイミングでタスクを停止するので
 * 情報送信の成功をトリガーにタスクを停止することはない
 */
void TrapModule::sendRequestModuleState() {
    DEBUG_MSG_LN("sendRequestModuleState");
    if (_mesh.getNodeList().size() == 0) {
        taskStop(_requestModuleStateTask);
        return;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &obj = jsonBuf.createObject();
    _pConfig->collectModuleConfig(obj);
    obj[KEY_REQUEST_MODULE_STATE] = true;
    // モジュール状態の送信対象の親モジュール ID を送信
    obj[KEY_CONFIG_UPDATE] = true;
    obj[KEY_PARENT_NODE_ID] = getNodeId();
    sendBroadcast(obj);
}

/*************************************
 * タスク関連
 ************************************/
/**
 * 接続モジュール数 LED 点滅
 */
void TrapModule::blinkLed() {
    if (_pConfig->_ledOnFlag) {
        _pConfig->_ledOnFlag = false;
    } else {
        _pConfig->_ledOnFlag = true;
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
    DEBUG_MSG_LN("Shift Deep Sleep");
    DEBUG_MSG_LN("mesh Stop");
    _mesh.stop();
    // wifi off
    DEBUG_MSG_LN("wifi off");
    WiFi.mode(WIFI_OFF);
    unsigned long dif = millis();
    while (millis() - dif < 5000) {
        delay(100);
        yield();
        if (WiFi.status() == WL_DISCONNECTED) {
            break;
        }
    }
    // 現在の設定値を保存
    _pConfig->saveCurrentModuleConfig();
    // 全モジュールの情報をサーバーに送信
    if (_pConfig->_isParent) {
        sendModulesInfo();
    }
    // バッテリーが限界の場合は手動で起動するまでDeepSleep
    if (_pConfig->_isBatteryDead) {
        DEBUG_MSG_LN("Battery limit!\nshutdown...");
#ifdef ESP32
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);
        esp_deep_sleep_start();
#else
        ESP.deepSleep(0);
#endif
        return;
    }
    time_t currentTime = now();
    DEBUG_MSG_F("currentTime:%s\n", asctime(gmtime(&currentTime)));
    DEBUG_MSG_F("wakeTime:%s\n", asctime(gmtime(&_pConfig->_wakeTime)));
    // calcSleepTime()の返り値をマイクロ秒にするとなぜか変になるので一旦ミリ秒で返してからマイクロ秒にする
    uint64_t deepSleepTime = _pConfig->_wakeTime - now();
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
    if (_pConfig->_isBatteryDead) {
        taskStop(_checkBatteryLimitTask);
        _pConfig->_isSleep = true;
    }
}

/**
 * タスクをスケジューラーにセット
 */
void TrapModule::setTask(Task &task, const unsigned long interval, const long iteration,
                         TaskCallback aCallback, const bool isEnable) {
    task.set(interval, iteration, aCallback);
    _mesh.scheduler.addTask(task);
    if (isEnable) {
        task.enable();
    }
}

/**
 * タスク開始
 */
void TrapModule::taskStart(Task &task, unsigned long duration, long iteration) {
    if (iteration != -1) {
        task.setIterations(iteration);
    }
    if (task.isEnabled()) {
        return;
    }
    if (duration == 0) {
        task.enable();
    } else {
        task.enableDelayed(duration);
    }
}

/**
 * モジュール状態送信開始
 */
void TrapModule::startSendModuleState() {
    // 親モジュールは送信しない
    if (_pConfig->_isParent) {
        return;
    }
    // 設置モードか罠モード開始時は送信しない
    if (!_pConfig->_trapMode || _pConfig->_isTrapStart) {
        return;
    }
    // 一度送信成功したら送らない
    if (_pConfig->_isSendModuleState) {
        return;
    }
    taskStart(_sendModuleStateTask, DEF_INTERVAL);
}

/**
 * deepsleep要求送信タスク開始
 * 開始前に現在のノード数と次回起動時刻を更新する
 */
void TrapModule::startSyncSleeptask() {
    _pConfig->updateNodeNum(_mesh.getNodeList());
    _pConfig->_wakeTime = _pConfig->calcWakeTime(_pConfig->_activeStart, _pConfig->_activeEnd);
    taskStart(_sendSyncSleepTask);
}

/********************************************
 * Camera メソッド
 *******************************************/
/**
 * カメラスナップショット
 */
bool TrapModule::snapCamera(int resolution) {
    DEBUG_MSG_LN("snapCamera");
    if (!_pConfig->_cameraEnable) {
        DEBUG_MSG_LN("camera cannot use");
        return false;
    }
    // 画像転送タスク実行中はカメラ撮影は実行しない
    if (_sendPictureTask.isEnabled()) {
        DEBUG_MSG_LN("camera cannot use because picture send task is running");
        return false;
    }
    if (eTaskGetState(_taskHandle[0]) == eSuspended) {
        Camera::getInstance()->setResolution(resolution);
        vTaskResume(_taskHandle[0]);
        return true;
    }
    DEBUG_MSG_LN("camera task is running");
    return false;
}

/**
 * カメラ撮影タスク
 */
void TrapModule::snapCameraTask(void *arg) {
    TrapModule *pTrapModule = TrapModule::getInstance();
    Camera *pCamera = Camera::getInstance();
    DEBUG_MSG_LN("snapCameraTask");
    while (1) {
        if (pCamera->isSetResolution()) {
            if (pCamera->saveCameraData()) {
                DEBUG_MSG_LN("snap success");
                // メッセージ送信タスク実行中でなければ送信タスク開始
                pTrapModule->_sendPictureTask.setIterations(DEF_ITERATION);
                pTrapModule->_sendPictureTask.enable();
            } else {
                DEBUG_MSG_LN("snap failed");
            }
        }
        DEBUG_MSG_LN("camera task suspend");
        vTaskSuspend(pTrapModule->_taskHandle[0]);
        TASK_DELAY(1);
    }
}

/*************************************
 * MQTT
 ************************************/
/**
 * モジュール状態情報をサーバーに送信
 */
void TrapModule::sendModulesInfo() {
    DEBUG_MSG_LN("sendModulesInfo");
    // 自身のセンサ情報更新
    updateBattery();
    updateTrapFire();
    if (!_pCellular->startModule()) {
        return;
    }
    _pCellular->initMqttSetting();
    if (!_pCellular->connectMqttServer(MQTT_SERVER_HOST, MQTT_SERVER_PORT)) {
        return;
    }
    String trapStartInfo;
    _pConfig->createModulesInfo(trapStartInfo, _pConfig->_isTrapStart);
    SendType sendType = _pConfig->_isTrapStart ? SETTING : PERIOD;
    _pCellular->sendTrapModuleInfo(trapStartInfo, sendType);
    _pCellular->disconnectMqttServer();
    _pCellular->stopModule();
}

/*************************************
 * Debug
 ************************************/
/**
 * Debug メッセージ送信
 */
bool TrapModule::sendDebugMesage(String msg, uint32_t nodeId) {
    if (nodeId != 0) {
        DEBUG_MSG_LN("send debug message single");
        return _mesh.sendSingle(nodeId, msg);
    }
    DEBUG_MSG_LN("send debug message broadcast");
    bool success = _mesh.getNodeList().size() == 0 ? true : _mesh.sendBroadcast(msg);
    receivedCallback(getNodeId(), msg);
    return success;
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
    _pConfig->_ledOnFlag = false;
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
