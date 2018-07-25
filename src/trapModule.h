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
    Task _deepSleepTask;       // DeepSleep以降タスク
    Task _blinkNodesTask;      // LED タスク
    Task _sendPictureTask;     // 写真撮影フラグ
    Task _sendModuleStateTask; // モジュール状態送信タスク

    TaskHandle_t _taskHandle[1];

  public:
    TrapModule(){};
    ~TrapModule(){};
    // setup
    void setupMesh(const uint16_t types);
    void setupTask();
    void setupCamera() { _config._cameraEnable = _camera.initialize(); };
    bool loadModuleConfig() { return _config.loadModuleConfigFile(); };
    void checkStart();
    // loop
    void update();
    // モジュール設定値操作
    bool setConfig(JsonObject &config);
    String getMeshGraph() { return _mesh.subConnectionJson(); };
    JsonObject &getModuleInfo() { return _config.getModuleInfo(_mesh); };
    bool setCurrentTime(time_t current);
    bool initGps();
    bool getGps();
    // カメラ機能
    bool snapCamera(int resolution = -1);
    static void snapCameraTask(void *arg);
    // debug 機能
    bool sendDebugMesage(String msg, uint32_t nodeId = 0);

  private:
    // メッセージ送信
    bool syncAllModuleConfigs(JsonObject &config);
    bool syncCurrentTime();
    void sendPicture();
    bool sendGetGps();
    void sendModuleState();
    // config
    uint32_t getNodeId() { return _config._nodeId != 0 ? _config._nodeId : _mesh.getNodeId(); };
    void updateModuleState();
    // ハードウェア機能
    void shiftDeepSleep();
    // mesh
    void receivedCallback(uint32_t from, String &msg);
    void newConnectionCallback(uint32_t nodeId);
    void changedConnectionCallback();
    void nodeTimeAdjustedCallback(int32_t offset);
    // task
    void blinkLed();
    void moduleStateTaskStart() {
        _sendModuleStateTask.setIterations(SEND_RETRY);
        if (!_sendModuleStateTask.isEnabled()) {
            _sendModuleStateTask.enable();
        }
    }
    void moduleStateTaskStop() {
        if (_sendModuleStateTask.isEnabled()) {
            _sendModuleStateTask.disable();
        }
    }
    // util
    void saveBase64Image(const char *data, const char *name = NULL);
    bool sendBroadcast(JsonObject &obj) {
        String msg;
        obj.printTo(msg);
        return _mesh.sendBroadcast(msg);
    }
    bool sendParent(JsonObject &obj) {
        String msg;
        obj.printTo(msg);
        return _mesh.sendSingle(_config._parentNodeId, msg);
    }
};

#endif // INCLUDE_GUARD_TRAPMODULE