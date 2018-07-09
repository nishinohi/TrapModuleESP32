#include "trapModule.h"

/**************************************
 * setup
 * ***********************************/
// メッシュネットワークセットアップ
void TrapModule::setupMesh(const uint16_t types) {
    // set before init() so that you can see startup messages
    _mesh.setDebugMsgTypes(types);
    _mesh.init(MESH_SSID, MESH_PASSWORD, MESH_PORT);
    _mesh.onReceive(std::bind(&TrapModule::receivedCallback, this,
                              std::placeholders::_1, std::placeholders::_2));
    _mesh.onNewConnection(std::bind(&TrapModule::newConnectionCallback, this,
                                    std::placeholders::_1));
    _mesh.onChangedConnections(
        std::bind(&TrapModule::changedConnectionCallback, this));
    _mesh.onNodeTimeAdjusted(std::bind(&TrapModule::nodeTimeAdjustedCallback,
                                       this, std::placeholders::_1));
}

// 実施タスクセットアップ
void TrapModule::setupTask() {
    // LED Setting
    _blinkNodesTask.set(BLINK_PERIOD, (_mesh.getNodeList().size() + 1) * 2,
                        std::bind(&TrapModule::blinkLed, this));
    _mesh.scheduler.addTask(_blinkNodesTask);
    _blinkNodesTask.enable();
    // battery check interval
    _checkBatteryTask.set(CHECK_INTERVAL, TASK_FOREVER,
                          std::bind(&TrapModule::checkBattery, this));
    _mesh.scheduler.addTask(_checkBatteryTask);
    if (!_config._trapMode) {
        _checkBatteryTask.enable();
    }
    // trap check
    _checkTrapTask.set(CHECK_INTERVAL, TASK_FOREVER,
                       std::bind(&TrapModule::checkTrap, this));
    _mesh.scheduler.addTask(_checkTrapTask);
    // picture
    _sendPictureTask.set(5000, 5, std::bind(&TrapModule::sendPicture, this));
    _mesh.scheduler.addTask(_sendPictureTask);
    // DeepSleep
    _deepSleepTask.set(SYNC_SLEEP_INTERVAL, TASK_FOREVER,
                       std::bind(&TrapModule::shiftDeepSleep, this));
    _mesh.scheduler.addTask(_deepSleepTask);
}

/********************************************
 * Camera メソッド
 *******************************************/
bool TrapModule::snapCamera(int picFmt) {
    // _mesh.stop();
    DEBUG_MSG_LN("snapCamera");
    bool isSnap = _camera.saveCameraData("/image.jpg", picFmt);
    // setupMesh();
    _sendPictureTask.setIterations(5);
    _sendPictureTask.enableDelayed(5000);
    return isSnap;
}

/********************************************
 * loop メソッド
 *******************************************/
/**
 * 監視機能はタスクで管理したほうがいいかもしれない
 **/
void TrapModule::update() {
    // 罠モード始動
    if (_config._isTrapStart) {
        DEBUG_MSG_LN("Trap start");
        // 罠モード開始直後に deepSleepを開始すると http
        // 接続が未完のまま落ちるので少し待つ
        _deepSleepTask.enableDelayed(SYNC_SLEEP_INTERVAL);
        _config._isTrapStart = false;
    }
    // メッシュ待機限界時間が経過したら罠とバッテリーチェック開始
    if (_config._trapMode &&
        now() - _config._wakeTime > _config._workTime - MESH_WAIT_LIMIT &&
        !_checkTrapTask.isEnabled() && !_checkBatteryTask.isEnabled()) {
        DEBUG_MSG_LN("mesh wait limit.");
        moduleCheckStart();
    }
    // 稼働時間超過により強制 DeepSleep
    if (_config._trapMode && now() - _config._wakeTime > _config._workTime &&
        !_deepSleepTask.isEnabled()) {
        DEBUG_MSG_LN("work time limit.");
        _deepSleepTask.enableDelayed(SYNC_SLEEP_INTERVAL);
        _config._nodeNum = _mesh.getNodeList().size();
    }
    // 確認用 LED
    if (_config._trapMode) {
        digitalWrite(LED, HIGH);
    } else {
        digitalWrite(LED, !_config._ledOnFlag);
    }
    _mesh.update();
}

/********************************************
 * モジュール設定値操作
 *******************************************/
// モジュールパラメータ設定
bool TrapModule::setConfig(const JsonObject &config) {
    if (syncAllModuleConfigs(config)) {
        updateModuleConfig(config);
        return saveCurrentModuleConfig();
    }
    return false;
}

// 現在時刻設定
bool TrapModule::setCurrentTime(int hour, int minute, int second, int day,
                                int month, int year) {
    setTime(hour, minute, second, day, month, year);
    return syncCurrentTime();
}

// 現在時刻設定
bool TrapModule::setCurrentTime(time_t current) {
    setTime(current);
    return syncCurrentTime();
}

// GPS 初期化
bool TrapModule::initGps() {
    _config.initGps();
    if (_mesh.getNodeList().size() == 0) {
        return true;
    }
    String message = "{";
    message += KEY_INIT_GPS;
    message += ":\"InitGps\"}";
    return _mesh.sendBroadcast(message);
}

/**
 * GPS 取得要求を親モジュールに送信
 **/
bool TrapModule::sendGetGps() {
    if (_mesh.getNodeList().size() == 0) {
        return true;
    }
    String message = "{";
    message += KEY_GET_GPS;
    message += ":\"GetGps\"}";
    for (SimpleList<uint32_t>::iterator it = _config._parentModules.begin();
         it != _config._parentModules.end(); ++it) {
        if (_mesh.sendSingle(*it, message)) {
            return true;
        }
    }
    return false;
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
    // バッテリー切れメッセージ
    if (msgJson.containsKey(KEY_BATTERY_DEAD_MESSAGE)) {
        DEBUG_MSG_LN("battery dead message");
    }
    // 罠検知メッセージ
    if (msgJson.containsKey(KEY_TRAP_FIRE_MESSAGE)) {
        DEBUG_MSG_LN("trap fire message");
    }
    // 画像保存
    if (msgJson.containsKey(KEY_PICTURE)) {
        DEBUG_MSG_LN("image receive");
        const char *data = (const char *)msgJson[KEY_PICTURE];
        int inputLen = strlen(data);
        int decLen = base64_dec_len((char *)data, inputLen);
        char *dec = (char *)malloc(decLen + 1);
        base64_decode(dec, (char *)data, inputLen);
        File img = SPIFFS.open("/image.jpg", "w");
        img.write((const uint8_t *)dec, decLen + 1);
        img.close();
        free(dec);
    }
    // GPS 初期化
    if (msgJson.containsKey(KEY_INIT_GPS)) {
        DEBUG_MSG_LN("Init GPS");
        _config.initGps();
        saveCurrentModuleConfig();
    }
    // GPS 取得
    if (msgJson.containsKey(KEY_GET_GPS)) {
        DEBUG_MSG_LN("Get GPS");
    }
    // 親モジュールリスト追加
    if (msgJson.containsKey(KEY_PARENT_MODULE_LIST)) {
        DEBUG_MSG_LN("parentModuleList");
        _config.addParentModule(msgJson[KEY_PARENT_MODULE_LIST]);
        saveCurrentModuleConfig();
    }
    // モジュール同期 DeepSleep メッセージ
    if (msgJson.containsKey(KEY_SYNC_SLEEP)) {
        DEBUG_MSG_LN("SyncSleep start");
        _deepSleepTask.enableDelayed(SYNC_SLEEP_INTERVAL);
        // 現時点でのメッシュノード数を保存する(deepSleep直前はメッシュネットワークが不安定のため)
        _config._nodeNum = _mesh.getNodeList().size();
    }
    // 真時刻メッセージ
    if (msgJson.containsKey(KEY_REAL_TIME)) {
        DEBUG_MSG_LN("realTime get");
        _config._realTime = msgJson[KEY_REAL_TIME];
        _config._realTimeDiff = millis();
    }
    // モジュール設定更新メッセージ受信
    if (msgJson.containsKey(KEY_SLEEP_INTERVAL) ||
        msgJson.containsKey(KEY_WORK_TIME) ||
        msgJson.containsKey(KEY_TRAP_MODE) ||
        msgJson.containsKey(KEY_ACTIVE_START) ||
        msgJson.containsKey(KEY_ACTIVE_END) ||
        msgJson.containsKey(KEY_GPS_LAT) || msgJson.containsKey(KEY_GPS_LON) ||
        msgJson.containsKey(KEY_WAKE_TIME) ||
        msgJson.containsKey(KEY_CURRENT_TIME)) {
        DEBUG_MSG_LN("Module config changed");
        // 罠検知済みフラグは自身の設定値を反映
        updateModuleConfig(msgJson);
        saveCurrentModuleConfig();
    }
}

/**
 * メッシュネットワークに新規のモジュールが追加されたとき
 */
void TrapModule::newConnectionCallback(uint32_t nodeId) {
    // Reset blink task
    _config._ledOnFlag = false;
    _blinkNodesTask.setIterations((_mesh.getNodeList().size() + 1) * 2);
    _blinkNodesTask.enableDelayed(
        BLINK_PERIOD - (_mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);
    DEBUG_MSG_F("--> startHere: New Connection, nodeId = %u\n", nodeId);

    SimpleList<uint32_t> nodes = _mesh.getNodeList();
    // 罠モード時に前回起動時のメッシュ数になれば罠検知とバッテリーチェック開始
    if (_config._trapMode && nodes.size() >= _config._nodeNum) {
        moduleCheckStart();
    }
    // 設置モードの場合メッシュノード数更新
    if (!_config._trapMode) {
        _config._nodeNum = nodes.size();
    }
    DEBUG_MSG_F("Num nodes: %d\n", nodes.size());
    DEBUG_MSG_F("Connection list:");
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
        DEBUG_MSG_F(" %u", *node);
        node++;
    }
    DEBUG_MSG_LN();
}

/**
 * ネットワーク（トポロジ）状態変化
 */
void TrapModule::changedConnectionCallback() {
    DEBUG_MSG_F("Changed connections %s\n", _mesh.subConnectionJson().c_str());
    // Reset blink task
    _config._ledOnFlag = false;
    _blinkNodesTask.setIterations((_mesh.getNodeList().size() + 1) * 2);
    _blinkNodesTask.enableDelayed(
        BLINK_PERIOD - (_mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);

    SimpleList<uint32_t> nodes = _mesh.getNodeList();
    // 罠モード時に前回起動時のメッシュ数になれば罠検知とバッテリーチェック開始
    if (_config._trapMode && nodes.size() >= _config._nodeNum) {
        moduleCheckStart();
    }
    // 設置モードの場合メッシュノード数更新
    if (!_config._trapMode) {
        _config._nodeNum = nodes.size();
    }
    // 親モジュールリストを更新
    _config.updateParentModuleList(_mesh);
    // 接続情報表示
    DEBUG_MSG_F("Num nodes: %d\n", nodes.size());
    DEBUG_MSG_F("Connection list:");
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
        DEBUG_MSG_F(" %u", *node);
        node++;
    }
    DEBUG_MSG_LN();
}

void TrapModule::nodeTimeAdjustedCallback(int32_t offset) {
    DEBUG_MSG_F("Adjusted time %u. Offset = %d\n", _mesh.getNodeTime(), offset);
}

/*******************************************************
 * メッセージング
 ******************************************************/
/**
 * 設定値を全モジュールに同期する
 **/
bool TrapModule::syncAllModuleConfigs(const JsonObject &config) {
    DEBUG_MSG_LN("syncAllModuleConfigs");
    if (!config.success()) {
        DEBUG_MSG_LN("json parse failed");
        return false;
    }
    if (_mesh.getNodeList().size() == 0) {
        return true;
    }
    String message;
    config.printTo(message);
    return _mesh.sendBroadcast(message);
}

// 現在時刻同期
bool TrapModule::syncCurrentTime() {
    if (_mesh.getNodeList().size() == 0) {
        return true;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &currentTime = jsonBuf.createObject();
    currentTime[KEY_CURRENT_TIME] = now();
    String msg;
    currentTime.printTo(msg);
    return _mesh.sendBroadcast(msg);
}

/**
 * 端末電池切れ通知
 * 過放電を防ぐため送信の成否に関係なく電源を落とす(メッセージ送信をタスク化しない)
 **/
bool TrapModule::sendBatteryDead() {
    if (_mesh.getNodeList().size() == 0) {
        return true;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &batteryDead = jsonBuf.createObject();
    batteryDead[KEY_BATTERY_DEAD_MESSAGE] = _mesh.getNodeId();
    String message;
    batteryDead.printTo(message);
    for (SimpleList<uint32_t>::iterator it = _config._parentModules.begin();
         it != _config._parentModules.end(); ++it) {
        if (_mesh.sendSingle(*it, message)) {
            return true;
        }
    }
    return false;
}

/**
 * 罠作動情報を親モジュールへ送信する
 * 送信が失敗した場合、罠作動再チェック時に再度送信する
 **/
bool TrapModule::sendTrapFire() {
    if (_config._parentModules.size() == 0) {
        DEBUG_MSG_LN("no parent module");
        return false;
    }
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &trapFire = jsonBuf.createObject();
    trapFire[KEY_TRAP_FIRE_MESSAGE] = _mesh.getNodeId();
    String message;
    trapFire.printTo(message);
    for (SimpleList<uint32_t>::iterator it = _config._parentModules.begin();
         it != _config._parentModules.end(); ++it) {
        if (_mesh.sendSingle(*it, message)) {
            return true;
        }
    }
    return false;
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
    String path = "/image.jpg";
    if (!SPIFFS.exists(path)) {
        return;
    }
    File file = SPIFFS.open(path, "r");
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
        _blinkNodesTask.enableDelayed(
            BLINK_PERIOD -
            (_mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);
    }
}

/**
 * 罠作動チェック
 * メッシュネットワークが前回起動時のサイズ以上になるか
 * 起動時間が残り半分以下になれば作動状況を通知
 */
void TrapModule::checkTrap() {
#ifdef TRAP_ACTIVE
    if (!_config._trapMode || !digitalRead(TRAP_CHECK_PIN)) {
        return;
    }
    DEBUG_MSG_LN("!****** Trap Fired ******!");
    if (_checkTrapTask.getIterations() == TASK_FOREVER) {
        _checkTrapTask.setIterations(SEND_RETRY);
    }
    if (sendTrapFire()) {
        _config._trapFire = true;
        saveCurrentModuleConfig();
        _checkTrapTask.disable();
    }
#endif
}

/**
 * 1/6 に分圧した電圧が放電終止電圧 1V * 4本 としてバッテリーが
 * 放電終止電圧を下回っていないかチェックする
 * 起動時間が残り半分以下の状態で電池切れになれば通知に失敗していても電源を切る
 **/
void TrapModule::checkBattery() {
#ifdef BATTERY_CHECK_ACTIVE
    int currentBattery = analogRead(A0);
    double realValue = currentBattery * 6;
    realValue = realValue / 1024;
    DEBUG_MSG_F("Current Battery:%.2lf\n", realValue);
    if (currentBattery > DISCHARGE_END_VOLTAGE) {
        return;
    }
    DEBUG_MSG_LN("!****** Battery Dead ******!");
    _config._isBatteryDead = true;
    if (_checkBatteryTask.getIterations() == TASK_FOREVER) {
        _checkBatteryTask.setIterations(SEND_RETRY);
    }
    // 設置モードの場合即時終了
    if (!_config._trapMode) {
        _deepSleepTask.enable();
        return;
    }
    if (sendBatteryDead()) {
        _deepSleepTask.enableDelayed(SYNC_SLEEP_INTERVAL);
    }
#endif
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
    yield();
    unsigned long dif = millis();
    while (millis() - dif < 5000) {
        delay(500);
        yield();
        if (WiFi.status() == WL_DISCONNECTED) {
            break;
        }
    }
    yield();
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
    _config._currentTime =
        tNow + _config.calcSleepTime(tNow, _config._wakeTime);
    saveCurrentModuleConfig();
#ifdef DEBUG_ESP_PORT
    time_t temp = now();
    setTime(_config._wakeTime);
    DEBUG_MSG_F("wakeTime:%d/%d/%d %d:%d:%d\n", year(), month(), day(), hour(),
                minute(), second());
    setTime(_config._currentTime);
    DEBUG_MSG_F("currentTime:%d/%d/%d %d:%d:%d\n", year(), month(), day(),
                hour(), minute(), second());
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
 * 各種チェックメソッド開始
 */
void TrapModule::moduleCheckStart() {
    if (!_config._trapFire && !_checkTrapTask.isEnabled()) {
        DEBUG_MSG_LN("Start Trap Check");
        _checkTrapTask.enableDelayed(TRAP_CHECK_DELAYED);
    }
    if (!_config._isBatteryDead && !_checkBatteryTask.isEnabled()) {
        DEBUG_MSG_LN("Start Battery Check");
        _checkBatteryTask.enableDelayed(BATTERY_CHECK_DELAYED);
    }
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
        return true;
    }
    return _mesh.sendBroadcast(msg, true);
}