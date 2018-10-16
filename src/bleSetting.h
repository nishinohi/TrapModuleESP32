
#ifndef BLE_SETTING
#define BLE_SETTING

#include "moduleConfig.h"
#include "trapCommon.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <map>
#include <painlessMesh.h>

#define BLE_DEVICE_NAME "TrapModuleSetting"
// Module Info Service
#define TRAP_MODULE_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// Module Setting Charactaristic
#define MODULE_SETTING_UUID "a131c37c-6acb-421d-9640-53bdcd818898"
// Module Info Charactaristic
#define MODULE_INFO_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class BleSetting {
  private:
    BLEServer *_pServer;
    BLEService *_pService;
    BLEAdvertising *_pAdvertising;

    std::map<std::string, BLECharacteristic *> _trapModuleCharactaristicMap;

  public:
    ModuleConfig *_pModuleConfig;

  private:
    void createTrapModuleCharacteristic();
    void updateModuleInfoCharacteristic();
    template <class T> String convertConfigToString(T value, String key);

  public:
    BleSetting() {}

    void init(ModuleConfig **ppModuleConfig);
    void start() {
        _pService->start();
        BLEAdvertising *pAdvertising = _pServer->getAdvertising();
        pAdvertising->start();
    }
};

class SettingCallbacks : public BLECharacteristicCallbacks {
  private:
    ModuleConfig *_pModuleConfig;

  public:
    SettingCallbacks(ModuleConfig **ppModuleConfig) { _pModuleConfig = *ppModuleConfig; }

    void onWrite(BLECharacteristic *pChara);
};

#endif // BLE_SETTING