#include "moduleConfig.h"
#include "trapModule.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <TimeLib.h>
#include <painlessMesh.h>

class TrapMesh {
  private:
    ModuleConfig _config;
    // タスク関連
    Task deepSleepTask;    // DeepSleep以降タスク
    Task checkTrapTask;    // 罠作動チェック
    Task blinkNodesTask;   // LED タスク
    Task checkBatteryTask; // バッテリーチェック
    Task sendPictureTask;  // 写真撮影フラグ

  public:
    painlessMesh _mesh;

  public:
    TrapMesh(){};
    ~TrapMesh(){};
    void update();

    // module method
    JsonObject &getModuleInfo() { return _config.getModuleInfo(_mesh); };
    bool updateAllModuleConfigs(const JsonObject &config);
    void updateModuleConfig(const JsonObject &config) {
        _config.updateModuleConfig(config);
    };
    bool saveCurrentModuleConfig() { _config.saveCurrentModuleConfig(); };
    bool sendGetGps();
    void initGps() { _config.initGps(); };
    void receivedCallback(uint32_t from, String &msg);
    bool loadModuleConfig() { return _config.loadModuleConfigFile(); };
    void checkWakeTime() {
        if (_config._trapMode && _config._wakeTime - now() >= 0) {
            shiftDeepSleep();
        }
    }
    void setupMesh();
    void setupTask();

  private:
    // mesh
    void shiftDeepSleep();
    void newConnectionCallback(uint32_t nodeId);
    void changedConnectionCallback();
    void nodeTimeAdjustedCallback(int32_t offset);
    // task
    void blinkLedCallBack();
    void trapCheckCallBack();
    void checkBatteryCallBack();
    void sendPicture();
    bool sendBatteryDead();
    bool sendTrapFire();
    void moduleCheckStart();
};