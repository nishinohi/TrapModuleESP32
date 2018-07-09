#ifndef INCLUDE_GUARD_TRAPMODULE
#define INCLUDE_GUARD_TRAPMODULE

#include "camera.h"
#include "moduleConfig.h"
#include "trapCommon.h"
#include <ArduinoBase64.h>
#include <TimeLib.h>
#include <painlessMesh.h>

class TrapModule {
  private:
    ModuleConfig _config;
    Camera _camera;
    painlessMesh _mesh;

    // タスク関連
    Task _deepSleepTask;    // DeepSleep以降タスク
    Task _checkTrapTask;    // 罠作動チェック
    Task _blinkNodesTask;   // LED タスク
    Task _checkBatteryTask; // バッテリーチェック
    Task _sendPictureTask;  // 写真撮影フラグ

  public:
    TrapModule(){};
    ~TrapModule(){};
    // setup
    void setupMesh(const uint16_t types);
    void setupTask();
    void setupCamera() { _config._cameraEnable = _camera.initialize(); };
    bool loadModuleConfig() { return _config.loadModuleConfigFile(); };
    void checkStart() {
        if (_config._isBatteryDead) {
            shiftDeepSleep();
        }
        if (_config._trapMode && _config._wakeTime - now() >= 0) {
            shiftDeepSleep();
        }
    }
    // loop
    void update();
    // モジュール設定値操作
    bool setConfig(JsonObject &config);
    String getMeshGraph() { return _mesh.subConnectionJson(); };
    JsonObject &getModuleInfo() { return _config.getModuleInfo(_mesh); };
    bool setCurrentTime(int hour, int minute, int second, int day, int month,
                        int year);
    bool setCurrentTime(time_t current);
    bool initGps();
    bool sendGetGps();
    // カメラ機能
    bool snapCamera(int picFmt = -1);
    // debug 機能
    bool sendDebugMesage(String msg, uint32_t nodeId = 0);

  private:
    // メッセージ送信
    bool syncAllModuleConfigs(const JsonObject &config);
    bool syncCurrentTime();
    bool sendBatteryDead();
    bool sendTrapFire();
    void sendPicture();
    // config
    void updateModuleConfig(const JsonObject &config) {
        _config.updateModuleConfig(config);
    };
    bool saveCurrentModuleConfig() {
        return _config.saveCurrentModuleConfig();
    };

    // ハードウェア機能
    void shiftDeepSleep();
    // mesh
    void receivedCallback(uint32_t from, String &msg);
    void newConnectionCallback(uint32_t nodeId);
    void changedConnectionCallback();
    void nodeTimeAdjustedCallback(int32_t offset);
    // task
    void blinkLed();
    void checkTrap();
    void checkBattery();
    void moduleCheckStart();
};

#endif // INCLUDE_GUARD_TRAPMODULE