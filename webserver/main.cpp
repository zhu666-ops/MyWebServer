#include "./conf/conf.h"
#include "./webserver.h"
using namespace std;

int main(){
    //加载配置文件
    Conf config;

    WebServer server;
    //初始化服务器
    server.init(config.m_actorModel,config.m_resourcesPath,config.m_mode,config.m_port,config.m_threadPoolMinNum,config.m_threadPoolMaxNum);

    server.threadPoolInit();

    server.eventListen();

    server.eventLoop();

    return 0;
}