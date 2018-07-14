#ifndef INCLUDE_GUARD_MODULECONFIG
#define INCLUDE_GUARD_MODULECONFIG

#include "trapCommon.h"
#include <TimeLib.h>
#include <painlessMesh.h>

class ModuleConfig {
  public:
    unsigned long _sleepInterval = DEF_SLEEP_INTERVAL;
    unsigned long _workTime = DEF_WORK_TIME;
    uint8_t _activeStart = DEF_ACTIVE_START;
    uint8_t _activeEnd = DEF_ACTIVE_END;
    bool _trapMode = DEF_TRAP_MODE;
    bool _trapFire = DEF_TRAP_FIRE;
    char _lat[GPS_STR_LEN] = DEF_GPS_LAT;
    char _lon[GPS_STR_LEN] = DEF_GPS_LON;
    SimpleList<uint32_t> _parentModules;    // 親モジュールリスト
    uint8_t _nodeNum = DEF_NODE_NUM;        // 前回起動時のノード数
    time_t _wakeTime = DEF_WAKE_TIME;       // 次回起動時刻
    time_t _currentTime = DEF_CURRENT_TIME; // 現在時刻

    // フラグ関連
    bool _isTrapStart = false;   // 罠起動モード移行フラグ
    bool _ledOnFlag = false;     // LED点滅フラグ
    bool _isBatteryDead = false; // バッテリー切れフラグ

    // カメラモジュール関連
    bool _cameraEnable = false;
    // 時間誤差修正
    time_t _realTime = 0;
    unsigned long _realTimeDiff = 0;

  public:
    ModuleConfig(){};

    void setWakeTime();
    JsonObject &getModuleInfo(painlessMesh &mesh);
    void updateModuleConfig(const JsonObject &config);
    bool saveCurrentModuleConfig();
    void initGps() {
        memset(_lat, '\0', GPS_STR_LEN);
        memset(_lon, '\0', GPS_STR_LEN);
    };
    time_t calcSleepTime(const time_t &tNow, const time_t &nextWakeTime);
    void addParentModule(uint32_t nodeId);
    void updateParentModuleList(painlessMesh &mesh);
    bool loadModuleConfigFile();

  private:
    void setDefaultModuleConfig();
    template <class T>
    void setParameter(T &targetParam, const T &setParam, const int maxV, const int minV);
    bool saveModuleConfig(const JsonObject &config);
    void updateGpsInfo(const char *lat, const char *lon);
};

#endif // INCLUDE_GUARD_MODULECONFIG