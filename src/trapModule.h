#ifndef INCLUDE_GUARD_TRAPMODULE
#define INCLUDE_GUARD_TRAPMODULE

#include "camera.h"
#include "cellular.h"
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
    Cellular _cellular;

    // タスク関連
    Task _deepSleepTask;       // DeepSleep以降タスク
    Task _blinkNodesTask;      // LED タスク
    Task _sendPictureTask;     // 写真撮影フラグ
    Task _sendModuleStateTask; // モジュール状態送信タスク
    Task _checkBatteryLimitTask; // バッテリー残量チェックタスク（設置モードで使用する）
    // 親モジュール機能
    Task _sendGPSDataTask;           // GPS 送信タスク
    Task _sendSyncSleepTask;         // 同期停止メッセージ送信タスク
    Task _sendParentInfoTask;        // 親モジュール情報送信タスク
    Task _requestModuleStateTask; // モジュール状態送信要求タスク
    // マルチタスクハンドラ
    TaskHandle_t _taskHandle[2]; // 0:カメラタスク、1:Cellular タスク

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
    bool setCurrentTime(time_t current);
    bool initGps();
    // モジュール情報取得
    String getMeshGraph() { return _mesh.subConnectionJson(); };
    void collectModuleInfo(JsonObject &moduleInfo) {
        _config.collectModuleInfo(_mesh, moduleInfo);
    };
    bool getGps();
    static void getGpsTask(void *arg);
    // カメラ機能
    bool snapCamera(int resolution = -1);
    static void snapCameraTask(void *arg);
    // debug 機能
    bool sendDebugMesage(String msg, uint32_t nodeId = 0);

  private:
    // 罠モード開始処理
    void startTrapMode();
    // mqtt server に情報送信
    void sendModulesInfo();
    // メッセージ送信
    bool syncAllModuleConfigs(JsonObject &config);
    bool syncCurrentTime();
    void sendPicture();
    bool sendGetGps();
    void sendModuleState();
    // 親限定メッセージ送信
    void sendSyncSleep();
    void sendGpsData();
    void sendParentModuleInfo();
    void sendRequestModuleState();
    // モジュール情報取得
    uint32_t getNodeId() { return _config._nodeId != 0 ? _config._nodeId : _mesh.getNodeId(); };
    // センサ情報
    void updateBattery();
    void updateTrapFire();
    // deepSleep
    void shiftDeepSleep();
    // mesh
    void receivedCallback(uint32_t from, String &msg);
    void newConnectionCallback(uint32_t nodeId);
    void changedConnectionCallback();
    void nodeTimeAdjustedCallback(int32_t offset);
    // task
    void blinkLed();
    void setTask(Task &task, const unsigned long interval, const long iteration,
                 TaskCallback aCallback, const bool isEnable);
    void checkBatteryLimit();
    void taskStart(Task &task, unsigned long duration = 0, long iteration = -1);
    void taskStop(Task &task) {
        if (task.isEnabled()) {
            task.disable();
        }
    }
    void startSendModuleState();
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
    void refreshMeshDetail();
    bool beginMultiTask(const char *taskName, TaskFunction_t func, TaskHandle_t taskHandle,
                        void *arg, const uint8_t priority, const uint8_t core = 0);
};

#endif // INCLUDE_GUARD_TRAPMODULE