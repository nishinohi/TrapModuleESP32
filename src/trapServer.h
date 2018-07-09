#ifndef INCLUDE_GUARD_SERVER
#define INCLUDE_GUARD_SERVER

#include "trapModule.h"
#include "trapCommon.h"
#include <ESPAsyncWebServer.h>

class TrapServer {
  private:
    AsyncWebServer server = AsyncWebServer(80);
    TrapModule *_trapModule;

  public:
    TrapServer(TrapModule *trapMesh) { _trapModule = trapMesh; };
    ~TrapServer(){};
    void beginServer() { server.begin(); };
    void setupServer();

  private:
    // server call back
    void onSetConfig(AsyncWebServerRequest *request);
    void onGetModuleInfo(AsyncWebServerRequest *request);
    void onGetMeshGraph(AsyncWebServerRequest *request);
    void onSetCurrentTime(AsyncWebServerRequest *request);
    void onSnapShot(AsyncWebServerRequest *request);
    void onSendMessage(AsyncWebServerRequest *request);
    void onInitGps(AsyncWebServerRequest *request);
    void onGetGps(AsyncWebServerRequest *request);
};

#endif // INCLUDE_GUARD_SERVER