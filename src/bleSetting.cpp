#include "bleSetting.h"

void BleSetting::init(ModuleConfig **ppModuleConfig) {
    _pModuleConfig = *ppModuleConfig;
    BLEDevice::init("TrapModuleSetting");
    _pServer = BLEDevice::createServer();
    _pService = _pServer->createService(TRAP_MODULE_SERVICE_UUID);
    createTrapModuleCharacteristic();
    updateModuleInfoCharacteristic();
}

// 罠モジュールBLEキャラクタリスティックを生成
void BleSetting::createTrapModuleCharacteristic() {
    _trapModuleCharactaristicMap[MODULE_INFO_UUID] =
        _pService->createCharacteristic(MODULE_INFO_UUID, BLECharacteristic::PROPERTY_READ);
    _trapModuleCharactaristicMap[MODULE_SETTING_UUID] = _pService->createCharacteristic(
        MODULE_SETTING_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    _trapModuleCharactaristicMap[MODULE_SETTING_UUID]->setCallbacks(new SettingCallbacks());
}

// モジュール設定値を更新
void BleSetting::updateModuleInfoCharacteristic() {
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &info = jsonBuf.createObject();
    _pModuleConfig->collectModuleSetting(info);
    String infoStr;
    info.printTo(infoStr);
    _trapModuleCharactaristicMap[MODULE_INFO_UUID]->setValue((uint8_t *)infoStr.c_str(),
                                                             infoStr.length());
}

template <class T> String BleSetting::convertConfigToString(T value, String key) {
    DynamicJsonBuffer jsonBuf(JSON_BUF_NUM);
    JsonObject &obj = jsonBuf.createObject();
    obj[key] = value;
    String configStr;
    obj.printTo(configStr);
    return configStr;
}
