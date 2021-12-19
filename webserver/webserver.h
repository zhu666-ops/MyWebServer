#pragma once 
#include <exception>
#include <string>
#include "./threadpool/threadpool.h"
#include "./conf/conf.h"
#include "./httpconnect/http_connect.h"
#include "./utiltimer/util_timer.h"
#include "./epoll/epoll.h"
using namespace std;

#define MAX_FD 65535   //最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000  //最多能监听事件的数量
#define TIMESLOT 5   //定时器触发时间，每5s触发一次

class WebServer{
public:
    WebServer();
    ~WebServer();

    void init(bool actorModel,string root,bool mode,int port,int threadPoolMinNum,int threadPoolMaxNum);   //初始化各种参数

    void threadPoolInit();  //线程池的初始化

    void eventListen();  //事件的监听函数，也就是有关网络的一些监听

    void eventLoop();

    void timerInit(int connectFd,struct sockaddr_in clientAddr);

    void adjustTimer(int sockFd);

    void deleteTimer(int sockFd);   //删除定时器

    bool handleClientData();  //处理与客户端连接

    bool handleSignal(bool &timeout,bool &stopServer);
    
    void handleRead(int sockFd);

    void handleWrite(int sockFd);

    //有关信号相关的函数
    

private:
    //基础配置
    bool m_actorModel;   //默认是proactor模型，true为reactor模型
    string m_root;   //访问资源目录的默认路径
    bool m_mode;   //默认是LT模式，true为ET模式
    int m_port;  //默认端口号是9999
    int m_threadPoolMinNum;  //线程池中默认线程的最小值
    int m_threadPoolMaxNum;  //线程池中默认线程的最大值

    
    HttpConnect *m_users;

    //socket相关的事件相关的
    int m_epollFd;
    int m_listenFd;
    //创建epoll对象，事件的数组
    epoll_event m_events[MAX_EVENT_NUMBER];

    //有关epoll相关的add等操作
    Epoll m_epollEvent;

    //线程池相关
    ThreadPool<HttpConnect> *m_pool;

    //定时器相关的
    UtilTimer m_timer;
};

//定时器回调函数，它删除非活动连接socket上的注册事件，并关闭
void cb_func(HttpConnect *userData);
void addSignal(int signal,void(*handler)(int));
static void sigHeadler(int sig);
void addSig(int sig);
void timerHeadler();