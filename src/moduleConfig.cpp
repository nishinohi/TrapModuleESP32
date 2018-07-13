#include "moduleConfig.h"

/***********************************
 * module method
 **********************************/
// モジュール情報を取得
JsonObject &ModuleConfig::getModuleInfo(painlessMesh &mesh) {
    DEBUG_MSG_LN("getModuleInfo");
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &moduleInfo = jsonBuf.createObject();
    moduleInfo[KEY_NODE_ID] = mesh.getNodeId();
    moduleInfo[KEY_TRAP_MODE] = _trapMode;
    moduleInfo[KEY_SLEEP_INTERVAL] = _sleepInterval;
    moduleInfo[KEY_WORK_TIME] = _workTime;
    moduleInfo[KEY_TRAP_FIRE] = _trapFire;
    moduleInfo[KEY_GPS_LAT] = _lat;
    moduleInfo[KEY_GPS_LON] = _lon;
    moduleInfo[KEY_ACTIVE_START] = _activeStart;
    moduleInfo[KEY_ACTIVE_END] = _activeEnd;
    moduleInfo[KEY_CAMERA_ENABLE] = _cameraEnable;
    bool isTimeSet = timeStatus() != timeStatus_t::timeNotSet;
    moduleInfo[KEY_CURRENT_TIME] = isTimeSet ? now() : DEF_CURRENT_TIME;
    JsonArray &nodeList = moduleInfo.createNestedArray(KEY_NODE_LIST);
    SimpleList<uint32_t> nodes = mesh.getNodeList();
    for (SimpleList<uint32_t>::iterator it = nodes.begin(); it != nodes.end();
         ++it) {
        nodeList.add(*it);
    }
    JsonArray &parentModule =
        moduleInfo.createNestedArray(KEY_PARENT_MODULE_LIST);
    for (SimpleList<uint32_t>::iterator it = _parentModules.begin();
         it != _parentModules.end(); ++it) {
        parentModule.add(*it);
    }
    return moduleInfo;
}

// デフォルト設定を書き込む
void ModuleConfig::setDefaultModuleConfig() {
    DEBUG_MSG_LN("Set Default Module Config");
    _sleepInterval = DEF_SLEEP_INTERVAL;
    _workTime = DEF_WORK_TIME;
    _trapFire = DEF_TRAP_FIRE;
    _trapMode = DEF_TRAP_MODE;
    memset(_lat, '\0', GPS_STR_LEN);
    memset(_lon, '\0', GPS_STR_LEN);
    _wakeTime = DEF_WAKE_TIME;
    _currentTime = DEF_CURRENT_TIME;
    _activeStart = DEF_ACTIVE_START;
    _activeEnd = DEF_ACTIVE_END;
    _nodeNum = DEF_NODE_NUM;
    _parentModules.clear();
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
        DEBUG_MSG_LN(
            "file not found...\nmaybe this is first using of this module");
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
    // 強制設置モード起動スイッチ
    if (!digitalRead(FORCE_TRAP_MODE_PIN)) {
        config[KEY_TRAP_MODE] = false;
    }
    // 設置モードでの起動時にロードしない内容はここで除外する
    if (!config.containsKey(KEY_TRAP_MODE) || config[KEY_TRAP_MODE] == false) {
        config.remove(KEY_PARENT_MODULE_LIST);
        config.remove(KEY_NODE_LIST);
        config.remove(KEY_TRAP_FIRE);
        config.remove(KEY_WAKE_TIME);
        config.remove(KEY_CURRENT_TIME);
    }
    updateModuleConfig(config);
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
    // 起動間隔
    if (config.containsKey(KEY_SLEEP_INTERVAL)) {
        setParameter(_sleepInterval,
                     static_cast<unsigned long>(config[KEY_SLEEP_INTERVAL]),
                     MAX_SLEEP_INTERVAL, MIN_SLEEP_INTERVAL);
    }
    // 稼働時間
    if (config.containsKey(KEY_WORK_TIME)) {
        setParameter(_workTime,
                     static_cast<unsigned long>(config[KEY_WORK_TIME]),
                     MAX_WORK_TIME, MIN_WORK_TIME);
    }
    // 稼働開始時刻
    if (config.containsKey(KEY_ACTIVE_START)) {
        setParameter(_activeStart,
                     static_cast<uint8_t>(config[KEY_ACTIVE_START]), 24, 0);
    }
    // 稼働終了時刻
    if (config.containsKey(KEY_ACTIVE_END)) {
        setParameter(_activeEnd, static_cast<uint8_t>(config[KEY_ACTIVE_END]),
                     24, 0);
    }
    // 親モジュール
    if (config.containsKey(KEY_PARENT_MODULE_LIST)) {
        _parentModules.clear();
        JsonArray &parentModules = config[KEY_PARENT_MODULE_LIST];
        for (JsonArray::iterator it = parentModules.begin();
             it != parentModules.end(); ++it) {
            _parentModules.push_back(*it);
        }
    }
    // ノードサイズ
    if (config.containsKey(KEY_NODE_NUM)) {
        _nodeNum = config[KEY_NODE_NUM];
    }
    // GPS 情報
    if (config.containsKey(KEY_GPS_LAT) && config.containsKey(KEY_GPS_LON)) {
        updateGpsInfo(config[KEY_GPS_LAT], config[KEY_GPS_LON]);
    }
    // 次回起動時刻情報
    if (config.containsKey(KEY_WAKE_TIME)) {
        _wakeTime = config[KEY_WAKE_TIME];
    }
    // 現在時刻情報
    if (config.containsKey(KEY_CURRENT_TIME)) {
        _currentTime = config[KEY_CURRENT_TIME];
        setTime(config[KEY_CURRENT_TIME]);
    }
    // 罠モード
    if (config.containsKey(KEY_TRAP_MODE)) {
        bool preTrapMode = _trapMode;
        _trapMode = config[KEY_TRAP_MODE];
        // 設置モードから罠モードへ移行
        if (!preTrapMode && _trapMode) {
            _isTrapStart = true;
        }
    }
    // 罠検知済みフラグは自身の値で更新(設置モード時は常に未検知状態)
    _trapFire = _trapMode ? _trapFire : false;
}

/**
 * 最大値と最小値を考慮した値設定
 */
template <class T>
void ModuleConfig::setParameter(T &targetParam, const T &setParam,
                                const int maxV, const int minV) {
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
    config[KEY_SLEEP_INTERVAL] = _sleepInterval;
    config[KEY_WORK_TIME] = _workTime;
    config[KEY_TRAP_FIRE] = _trapFire;
    config[KEY_GPS_LAT] = _lat;
    config[KEY_GPS_LON] = _lon;
    config[KEY_ACTIVE_START] = _activeStart;
    config[KEY_ACTIVE_END] = _activeEnd;
    bool isTimeSet = timeStatus() != timeStatus_t::timeNotSet;
    config[KEY_WAKE_TIME] = isTimeSet && _trapMode ? _wakeTime : DEF_WAKE_TIME;
    config[KEY_CURRENT_TIME] =
        isTimeSet && _trapMode ? _currentTime : DEF_CURRENT_TIME;
    // モジュール数
    config[KEY_NODE_NUM] = _nodeNum;
    // 親機リスト
    JsonArray &parentList = config.createNestedArray(KEY_PARENT_MODULE_LIST);
    for (SimpleList<uint32_t>::iterator it = _parentModules.begin();
         it != _parentModules.end(); ++it) {
        parentList.add(*it);
    }
#ifdef DEBUG_ESP_PORT
    String configStr;
    config.printTo(configStr);
    DEBUG_MSG_LN(configStr);
#endif
    return saveModuleConfig(config);
}

/**
 * 親モジュール追加（重複チェックあり）
 */
void ModuleConfig::addParentModule(uint32_t nodeId) {
    DEBUG_MSG_LN("addParentModule");
    for (SimpleList<uint32_t>::iterator it = _parentModules.begin();
         it != _parentModules.end(); ++it) {
        if (nodeId == *it) {
            DEBUG_MSG_F("parent module has already existed:%lu\n", nodeId);
            return;
        }
    }
    DEBUG_MSG_F("added parent module:%lu\n", nodeId);
    _parentModules.push_back(nodeId);
}

/**
 * 親モジュールリストの更新
 * 接続情報更新時に親モジュールがなくなっていればリストから削除
 */
void ModuleConfig::updateParentModuleList(painlessMesh &mesh) {
    SimpleList<uint32_t> nodes = mesh.getNodeList();
    SimpleList<uint32_t>::iterator node = nodes.begin();
    SimpleList<uint32_t>::iterator parentModule = _parentModules.begin();
    // 接続情報更新時に親モジュールがなくなっていればリストから削除
    while (parentModule != _parentModules.end()) {
        bool found = false;
        while (node != nodes.end()) {
            if (*parentModule == *node) {
                found = true;
                break;
            }
            ++node;
        }
        node = nodes.begin();
        if (!found) {
            _parentModules.erase(parentModule);
        }
        ++parentModule;
    }
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
 * もし1分以内に次の起動時刻になるなら2時間先の起動時刻まで飛ばす
 * 例）今が 13:59:05 で 14:00:00 も稼働時刻内なら次の起動時刻は 15:00:00
 * 移行の最短の起動時刻
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
    // 現在時刻が稼働時刻内
    if (_activeStart <= tHour && tHour < _activeEnd) {
        // 次の起動時刻が 1分以上後
        if (tMinute < 59) {
            _wakeTime = baseTime + 1 * SECS_PER_HOUR;
            return;
        }
        // [次の起動時刻] + 1h が稼働時刻内の場合
        if (tHour + 2 <= _activeEnd) {
            _wakeTime = baseTime + 2 * SECS_PER_HOUR;
            return;
        }
        // [次の起動時刻] + 1h が稼働時刻外の場合は稼働開始時刻をセット
        _wakeTime = baseTime + (24 - tHour + _activeStart) * SECS_PER_HOUR;
        return;
    }
    // 現在時刻が稼働開始時刻より早い
    if (tHour < _activeStart) {
        tNow.Hour = _activeStart;
        baseTime = makeTime(tNow);
        // もし1分以内に稼働開始時刻になるなら [稼働開始時刻] + 1h
        // の起動時刻まで飛ばす
        if (tHour + 1 == _activeStart && tMinute >= 59) {
            _wakeTime = baseTime + 1 * SECS_PER_HOUR;
            return;
        }
        // 1分以内に稼働開始時刻にならないなら稼働開始時刻まで停止
        _wakeTime = baseTime;
        return;
    }
    // 現在時刻が稼働終了時刻より遅い
    // 1分以内に稼働開始時刻になる場合 [稼働開始時刻] + 1h
    // まで停止(実質稼働開始時刻が 0 時の場合のみの考慮)
    if (tMinute < 59 && tHour + 1 - 24 == _activeStart) {
        _wakeTime = baseTime + 2 * SECS_PER_HOUR;
        return;
    }
    _wakeTime = baseTime + (24 - tHour + _activeStart) * SECS_PER_HOUR;
    return;
}

/**
 * DeepSleep 時間を計算する[秒]
 */
time_t ModuleConfig::calcSleepTime(const time_t &tNow,
                                   const time_t &nextWakeTime) {
    time_t diff = nextWakeTime - tNow;
    // 時刻誤差修正
    _realTime =
        _realTime == 0 ? 0 : _realTime + (millis() - _realTimeDiff) / 1000;
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