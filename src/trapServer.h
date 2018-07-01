#include "trapMesh.h"
#include "trapModule.h"

class TrapServer {
  private:
    AsyncWebServer server = AsyncWebServer(80);
    TrapMesh *_trapMesh;

  public:
    TrapServer(TrapMesh *trapMesh) { _trapMesh = trapMesh; };
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