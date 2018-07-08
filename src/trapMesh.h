#ifndef INCLUDE_GUARD_TRAPMESH
#define INCLUDE_GUARD_TRAPMESH

#include "camera.h"
#include "moduleConfig.h"
#include "trapCommon.h"
#include <ESPAsyncWebServer.h>
#include <TimeLib.h>
#include <painlessMesh.h>
#include <ArduinoBase64.h>

class TrapMesh {
  private:
    ModuleConfig _config;
    Camera _camera;
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
    bool syncAllModuleConfigs(const JsonObject &config);
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
    // config
    bool isCameraEnable() { return _config._cameraEnable; };
    // camera
    bool initCamera() {
        _config._cameraEnable = _camera.initialize();
        return _config._cameraEnable;
    };
    bool snapCamera(int picFmt = -1);

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

#endif // INCLUDE_GUARD_TRAPMESH