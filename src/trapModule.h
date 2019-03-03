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
    static TrapModule *_pTrapModule;

    ModuleConfig *_pConfig;
    Camera *_pCamera;
    painlessMesh _mesh;

    // タスク関連
    Task _blinkNodesTask;      // LED タスク
    Task _sendPictureTask;     // 写真撮影フラグ
    Task _sendModuleStateTask; // モジュール状態送信タスク
    Task _checkBatteryLimitTask; // バッテリー残量チェックタスク（設置モードで使用する）

    TaskHandle_t _taskHandle[1];

  public:
    static TrapModule *getInstance() {
        if (_pTrapModule == NULL) {
            _pTrapModule = new TrapModule();
        }
        return _pTrapModule;
    }
    static void deleteInstance() {
        if (_pTrapModule == NULL) {
            return;
        }
        delete _pTrapModule;
        _pTrapModule = NULL;
    }

    // setup
    void setupModule();
    // loop
    void update();
    // モジュール設定同期
    bool syncConfig(JsonObject &config);
    bool syncCurrentTime(time_t current);
    bool initGps();
    // モジュール情報取得
    String getMeshGraph() { return _mesh.subConnectionJson(); };
    void collectModuleInfo(JsonObject &moduleInfo) {
        _pConfig->collectModuleInfo(_mesh, moduleInfo);
    };
    // カメラ機能
    bool snapCamera(int resolution = -1);
    static void snapCameraTask(void *arg);
    // debug 機能
    bool sendDebugMesage(String msg, uint32_t nodeId = 0);
    // deepSleep
    void shiftDeepSleep();

  private:
    TrapModule() {
        _pConfig = ModuleConfig::getInstance();
        _pCamera = Camera::getInstance();
    };
    // setup
    void setupMesh(const uint16_t types);
    void setupTask();
    void setupCamera();
    bool loadModuleConfig() { return _pConfig->loadModuleConfigFile(); };
    bool checkBeforeStart();
    // メッセージ送信
    bool sendCurrentTime();
    void sendPicture();
    void sendModuleState();
    // モジュール情報取得
    uint32_t getNodeId() { return _pConfig->_nodeId != 0 ? _pConfig->_nodeId : _mesh.getNodeId(); };
    // センサ情報
    void updateBattery();
    void updateTrapFire();
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
        return _mesh.sendSingle(_pConfig->_parentNodeId, msg);
    }
    void refreshMeshDetail();
};

#endif // INCLUDE_GUARD_TRAPMODULE