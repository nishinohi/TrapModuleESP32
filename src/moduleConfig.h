#ifndef INCLUDE_GUARD_MODULECONFIG
#define INCLUDE_GUARD_MODULECONFIG

#include "trapCommon.h"
#include <TimeLib.h>
#include <painlessMesh.h>

// モジュール状態構
struct ModuleState {
    uint32_t nodeId;
    uint16_t batery;
    bool batteryDead;
    bool trapFire;
    bool cameraEnable;
};

class ModuleConfig {
  public:
    uint32_t _nodeId = DEF_NODEID;           // 自身のNodeId
    uint8_t _activeStart = DEF_ACTIVE_START; // 稼働開始時刻
    uint8_t _activeEnd = DEF_ACTIVE_END;     // 稼働停止時刻
    bool _trapMode = DEF_TRAP_MODE;          // 罠モード
    bool _trapFire = DEF_TRAP_FIRE;          // 罠作動状況
    char _lat[GPS_STR_LEN] = DEF_GPS_LAT;    // GPS 緯度
    char _lon[GPS_STR_LEN] = DEF_GPS_LON;    // GPS 経度
    uint32_t _parentNodeId = DEF_NODEID;     // 親モジュール ID
    uint8_t _nodeNum = DEF_NODE_NUM;         // 前回起動時のノード数
    time_t _wakeTime = DEF_WAKE_TIME;        // 次回起動時刻
    // フラグ関連
    bool _isTrapStart = false;       // 罠起動モード移行フラグ
    bool _ledOnFlag = false;         // LED点滅フラグ
    bool _isBatteryDead = false;     // バッテリー切れフラグ
    bool _isSendModuleState = false; // モジュール状態送信済みフラグ
    bool _isSleep = false;           // スリープ状態遷移フラグ
    // カメラモジュール関連
    bool _cameraEnable = false;
    // 時間誤差修正
    time_t _realTime = 0;
    unsigned long _realTimeDiff = 0;
    // 親限定
    bool _isParent = true;                  // 親モジュールとして振る舞うかどうか
    bool _isStarted = false;                // 罠起動モード開始済みフラグ
    SimpleList<uint32_t> _parentNodeIdList; // 親モジュール ID リスト（一時格納用）
    SimpleList<ModuleState> _moduleStateList; // モジュール機能状態保存リスト

  public:
    ModuleConfig(){};

    void setWakeTime();
    void collectModuleInfo(painlessMesh &mesh, JsonObject &moduleInfo);
    void collectModuleState(JsonObject &state);
    void collectModuleConfig(JsonObject &moduleConfig);
    void updateModuleConfig(const JsonObject &config);
    bool saveCurrentModuleConfig();
    void initGps() {
        memset(_lat, '\0', GPS_STR_LEN);
        memset(_lon, '\0', GPS_STR_LEN);
    };
    time_t calcSleepTime(const time_t &tNow, const time_t &nextWakeTime);
    void pushNoDuplicateNodeId(const uint32_t &nodeId, SimpleList<uint32_t> &list);
    bool loadModuleConfigFile();
    // 親機用
    void createModulesInfo(String &modulesInfoStr, bool isStart);
    void collectTrapUpdateInfo(painlessMesh &mesh, JsonObject &obj);
    void updateParentState();
    void updateNodeNum(SimpleList<uint32_t> nodeList);
    void pushNoDuplicateModuleState(const uint32_t &nodeId, JsonObject &stateJson);

  private:
    void setDefaultModuleConfig();
    template <class T>
    void setParameter(T &targetParam, const T &setParam, const int maxV, const int minV);
    bool saveModuleConfig(const JsonObject &config);
    void updateGpsInfo(const char *lat, const char *lon);
};

#endif // INCLUDE_GUARD_MODULECONFIG