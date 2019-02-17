#include "moduleConfig.h"

/***********************************
 * module method
 **********************************/
/**
 * モジュール情報を取得
 */
void ModuleConfig::collectModuleInfo(painlessMesh &mesh, JsonObject &moduleInfo) {
    DEBUG_MSG_LN("collectModuleInfo");
    moduleInfo[KEY_NODE_ID] = mesh.getNodeId();
    moduleInfo[KEY_TRAP_MODE] = _trapMode;
    moduleInfo[KEY_TRAP_FIRE] = _trapFire;
    moduleInfo[KEY_GPS_LAT] = _lat;
    moduleInfo[KEY_GPS_LON] = _lon;
    moduleInfo[KEY_ACTIVE_START] = _activeStart;
    moduleInfo[KEY_ACTIVE_END] = _activeEnd;
    moduleInfo[KEY_CAMERA_ENABLE] = _cameraEnable;
    moduleInfo[KEY_PARENT_NODE_ID] = _parentNodeId;
    moduleInfo[KEY_CURRENT_TIME] = now();
    // モジュールリスト
    JsonArray &nodeList = moduleInfo.createNestedArray(KEY_NODE_LIST);
    SimpleList<uint32_t> nodes = mesh.getNodeList();
    for (SimpleList<uint32_t>::iterator it = nodes.begin(); it != nodes.end(); ++it) {
        nodeList.add(*it);
    }
}

/**
 * モジュール設定値を取得
 * ただし、引数のmoduleConfigに予め設定されている値は上書きしない
 */
void ModuleConfig::collectModuleConfig(JsonObject &moduleConfig) {
    moduleConfig[KEY_ACTIVE_START] = _activeStart;
    moduleConfig[KEY_ACTIVE_END] = _activeEnd;
    moduleConfig[KEY_PARENT_NODE_ID] = _parentNodeId;
    moduleConfig[KEY_GPS_LAT] = _lat;
    moduleConfig[KEY_GPS_LON] = _lon;
    moduleConfig[KEY_WAKE_TIME] = _wakeTime;
    moduleConfig[KEY_TRAP_MODE] = _trapMode;
}

/**
 * モジュール状態を取得
 */
void ModuleConfig::collectModuleState(JsonObject &state) {
    state[KEY_MODULE_STATE] = true;
    state[KEY_TRAP_FIRE] = _trapFire;
    state[KEY_CAMERA_ENABLE] = _cameraEnable;
    state[KEY_BATTERY_DEAD] = _isBatteryDead;
    double realBattery = REAL_BATTERY_VALUE(analogRead(A0));
    state[KEY_CURRENT_BATTERY] = realBattery;
}

/**
 * デフォルト設定を書き込む
 */
void ModuleConfig::setDefaultModuleConfig() {
    DEBUG_MSG_LN("Set Default Module Config");
    _activeStart = DEF_ACTIVE_START;
    _activeEnd = DEF_ACTIVE_END;
    _trapMode = DEF_TRAP_MODE;
    _trapFire = DEF_TRAP_FIRE;
    memset(_lat, '\0', GPS_STR_LEN);
    memset(_lon, '\0', GPS_STR_LEN);
    _parentNodeId = DEF_NODEID;
    _nodeNum = DEF_NODE_NUM;
    _wakeTime = DEF_WAKE_TIME;
}

/**
 * モジュールに保存してある設定を読み出す
 * ファイルが存在しない場合（初回起動時）はデフォルト値の設定ファイルを作成する
 * 設定値はjson形式のファイルとする
 * 設置モード強制起動の場合は親モジュール情報をクリアする
 */
bool ModuleConfig::loadModuleConfigFile() {
    DEBUG_MSG_LN("loadModuleConfigFile");
    File file = SPIFFS.open("/config.json", "r");
    // 初回起動
    if (!file) {
        DEBUG_MSG_LN("file not found...\nmaybe this is first using of this module");
        setDefaultModuleConfig();
        return false;
    }
    size_t size = file.size();
    if (size == 0) {
        file.close();
        setDefaultModuleConfig();
        return false;
    }
    // 読み取ったデータを設定値に反映
    std::unique_ptr<char[]> buf(new char[size]);
    file.readBytes(buf.get(), size);
    DynamicJsonBuffer jsonBuffer(JSON_BUF_NUM);
    JsonObject &config = jsonBuffer.parseObject(buf.get());
    if (!config.success()) {
        DEBUG_MSG_LN("json parse failed");
        file.close();
        setDefaultModuleConfig();
        return false;
    }
    // 強制設置モード起動
    if (digitalRead(FORCE_SETTING_MODE_PIN) == HIGH) {
        config[KEY_TRAP_MODE] = false;
    }
    // 設置モードでの起動時にロードしない内容はここで除外する
    if (!config.containsKey(KEY_TRAP_MODE) || !config[KEY_TRAP_MODE]) {
        config.remove(KEY_PARENT_NODE_ID);
        config.remove(KEY_TRAP_FIRE);
        config.remove(KEY_WAKE_TIME);
    }
    // 罠モードで起動した場合は現在時刻を起動時刻と同時刻にセット
    if (config.containsKey(KEY_TRAP_MODE) && config[KEY_TRAP_MODE]) {
        config[KEY_CURRENT_TIME] = config[KEY_WAKE_TIME];
    }
    updateModuleConfig(config);
    // 罠モードから強制設置モードで起動しても、設定値更新せず電源を切ると
    // 再度罠モードで起動してしまうのでここで一旦設定値を保存する
    if (digitalRead(FORCE_SETTING_MODE_PIN)) {
        saveCurrentModuleConfig();
    }
    // 罠起動モード移行フラグは、設定値読み込み(loadModuleConfig)後に罠モード変更があった場合に変化する
    _isTrapStart = false;
    file.close();
#ifdef DEBUG_ESP_PORT
    String param;
    config.printTo(param);
    DEBUG_MSG_LN(param);
#endif
    return true;
}

/**
 * モジュール設定値を更新(設定可能範囲を考慮)
 */
void ModuleConfig::updateModuleConfig(const JsonObject &config) {
    DEBUG_MSG_LN("updateModuleConfig");
    if (!config.success()) {
        DEBUG_MSG_LN("json parse failed");
        setDefaultModuleConfig();
        return;
    }
    // 稼働開始時刻
    if (config.containsKey(KEY_ACTIVE_START)) {
        setParameter(_activeStart, static_cast<uint8_t>(config[KEY_ACTIVE_START]), 24, 0);
    }
    // 稼働終了時刻
    if (config.containsKey(KEY_ACTIVE_END)) {
        setParameter(_activeEnd, static_cast<uint8_t>(config[KEY_ACTIVE_END]), 24, 0);
    }
    // 親モジュール ID
    if (config.containsKey(KEY_PARENT_NODE_ID)) {
        _parentNodeId = config[KEY_PARENT_NODE_ID];
    }
    // ノードサイズ
    if (config.containsKey(KEY_NODE_NUM)) {
        _nodeNum = config[KEY_NODE_NUM];
    }
    // GPS 情報
    if (config.containsKey(KEY_GPS_LAT) && config.containsKey(KEY_GPS_LON)) {
        updateGpsInfo(config[KEY_GPS_LAT], config[KEY_GPS_LON]);
    }
    // GPS 初期化
    if (config.containsKey(KEY_INIT_GPS)) {
        initGps();
    }
    // 次回起動時刻情報
    if (config.containsKey(KEY_WAKE_TIME)) {
        _wakeTime = config[KEY_WAKE_TIME];
    }
    // 現在時刻情報
    if (config.containsKey(KEY_CURRENT_TIME)) {
        setTime(config[KEY_CURRENT_TIME]);
    }
    // 真時刻メッセージ
    if (config.containsKey(KEY_REAL_TIME)) {
        _realTime = config[KEY_REAL_TIME];
        _realTimeDiff = millis();
    }
    // 罠作動
    if (config.containsKey(KEY_TRAP_FIRE)) {
        _trapFire = config[KEY_TRAP_FIRE];
    }
    // 罠モード
    if (config.containsKey(KEY_TRAP_MODE)) {
        bool preTrapMode = _trapMode;
        _trapMode = config[KEY_TRAP_MODE];
        if (!preTrapMode && _trapMode) {
            DEBUG_MSG_LN("Trap start!");
            _isTrapStart = true;
        }
    }
}

/**
 * 最大値と最小値を考慮した値設定
 */
template <class T>
void ModuleConfig::setParameter(T &targetParam, const T &setParam, const int maxV, const int minV) {
    if (setParam >= minV && setParam <= maxV) {
        targetParam = setParam;
    } else {
        targetParam = setParam < minV ? minV : maxV;
    }
}

/**
 * 設定ファイルを保存
 */
bool ModuleConfig::saveModuleConfig(const JsonObject &config) {
    File file = SPIFFS.open("/config.json", "w");
    if (!file) {
        DEBUG_MSG_LN("File Open Error");
        return false;
    }
    config.printTo(file);
    file.close();
    return true;
}

/**
 * 現在の設定値を保存
 **/
bool ModuleConfig::saveCurrentModuleConfig() {
    DEBUG_MSG_LN("saveCurrentModuleConfig");
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &config = jsonBuf.createObject();
    config[KEY_TRAP_MODE] = _trapMode;
    config[KEY_TRAP_FIRE] = _trapFire;
    config[KEY_GPS_LAT] = _lat;
    config[KEY_GPS_LON] = _lon;
    config[KEY_ACTIVE_START] = _activeStart;
    config[KEY_ACTIVE_END] = _activeEnd;
    config[KEY_PARENT_NODE_ID] = _parentNodeId;
    config[KEY_WAKE_TIME] = _wakeTime;
    config[KEY_NODE_NUM] = _nodeNum;
#ifdef DEBUG_ESP_PORT
    String configStr;
    config.printTo(configStr);
    DEBUG_MSG_LN(configStr);
#endif
    return saveModuleConfig(config);
}

/**
 * 重複なし親モジュール ID 追加処理
 */
void ModuleConfig::pushNoDuplicateNodeId(const uint32_t &nodeId, SimpleList<uint32_t> &list) {
    for (auto &listId : list) {
        if (listId == nodeId) {
            return;
        }
    }
    list.push_back(nodeId);
}

/**
 * GPS情報更新
 */
void ModuleConfig::updateGpsInfo(const char *lat, const char *lon) {
    if (!lat || !lon || strlen(lat) == 0 || strlen(lon) == 0) {
        memset(_lat, '\0', GPS_STR_LEN);
        memset(_lon, '\0', GPS_STR_LEN);
        DEBUG_MSG_F("GPS Clear");
        return;
    }
    strncpy(_lat, lat, strlen(lat));
    strncpy(_lon, lon, strlen(lon));
    DEBUG_MSG_F("GPS data: (lat, lon) = (%s, %s)\n", _lat, _lon);
}

/**
 * 次の起動時刻を設定する
 * もし現在時刻から15分以内(WAKE_TIME_SET_MIN)に次の起動時刻になる場合
 * 最も近い起動時刻を1つ飛ばした次の起動時刻を次回の起動時刻に設定する
 * 例）今が 13:46 で15分以内の 14:00 も稼働時間帯なら次の起動時刻は 14:00 を飛ばして 15:00
 */
void ModuleConfig::setWakeTime() {
    // 現在時刻
    tmElements_t tNow;
    breakTime(now(), tNow);
    int tHour = tNow.Hour;
    int tMinute = tNow.Minute;
    tNow.Minute = 0;
    tNow.Second = 0;
    time_t baseTime = makeTime(tNow);
    // 現在時刻が稼働時間帯
    if (_activeStart <= tHour && tHour < _activeEnd) {
        // 次の起動時刻が15分以上後なら1時間後が次回起動時刻
        if (tMinute < 60 - WAKE_TIME_SET_MIN) {
            _wakeTime = baseTime + 1 * SECS_PER_HOUR;
            return;
        }
        // 2時間後の時刻が稼働時間帯の場合
        if (tHour + 2 <= _activeEnd) {
            _wakeTime = baseTime + 2 * SECS_PER_HOUR;
            return;
        }
        // 2時間後の時刻が稼働時間帯ではない場合は稼働開始時刻をセット
        _wakeTime = baseTime + (24 - tHour + _activeStart) * SECS_PER_HOUR;
        return;
    }
    // 現在時刻が稼働開始時刻より早い
    if (tHour < _activeStart) {
        // 15分以内に稼働開始時刻にならないなら稼働開始時刻まで停止
        if (tHour + 1 != _activeStart || tMinute < 60 - WAKE_TIME_SET_MIN) {
            tNow.Hour = _activeStart;
            _wakeTime = makeTime(tNow);
            return;
        }
        // もし15分以内に稼働開始時刻になるなら2時間後の起動時刻まで飛ばす
        tNow.Hour = _activeStart + 1;
        _wakeTime = makeTime(tNow);
        return;
    }
    // 現在時刻が稼働終了時刻より遅い
    // 15分以内に稼働開始時刻になる場合2時間後まで停止(実質稼働開始時刻が 0 時の場合のみの考慮)
    if (tMinute < 60 - WAKE_TIME_SET_MIN && tHour + 1 - 24 == _activeStart) {
        _wakeTime = baseTime + 2 * SECS_PER_HOUR;
        return;
    }
    _wakeTime = baseTime + (24 - tHour + _activeStart) * SECS_PER_HOUR;
    return;
}

/**
 * DeepSleep 時間を計算する[秒]
 */
time_t ModuleConfig::calcSleepTime(const time_t &tNow, const time_t &nextWakeTime) {
    time_t diff = nextWakeTime - tNow;
    // 時刻誤差修正
    _realTime = _realTime == 0 ? 0 : _realTime + (millis() - _realTimeDiff) / 1000;
    diff = _realTime == 0 ? diff : diff + (tNow - _realTime);
    // 最大Sleep時間以内の時間差
    if (diff <= MAX_SLEEP_INTERVAL) {
        DEBUG_MSG_F("calcSleepTime:%lu\n", diff);
        return diff;
    }
    // 極端に短いSleep時間にならないようにするための処理
    if (diff - MAX_SLEEP_INTERVAL > MAX_SLEEP_INTERVAL / 2) {
        DEBUG_MSG_F("calcSleepTime:%lu\n", MAX_SLEEP_INTERVAL);
        return MAX_SLEEP_INTERVAL;
    }
    DEBUG_MSG_F("calcSleepTime:%lu\n", MAX_SLEEP_INTERVAL / 2);
    return MAX_SLEEP_INTERVAL / 2;
}