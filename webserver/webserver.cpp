#include <iostream>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include "webserver.h"
#include "./threadpool/threadpool.h"
#include "./conf/conf.h"
#include "./httpconnect/http_connect.h"
#include "./utiltimer/util_timer.h"
#include "./threadpool/threadpool.cpp"
#include "./threadpool/taskqueue.h"
#include "./threadpool/taskqueue.cpp"
using namespace std;

int m_pipeFd[2];
SortTimerList m_timerList;

 WebServer::WebServer(){
     //创建一个数组用于保存所有客户端信息
     m_users=new HttpConnect[MAX_FD];
 }
    
 WebServer::~WebServer(){
     close(m_pipeFd[0]);
     close(m_pipeFd[1]);
     close(m_listenFd);
     if(m_users)
        delete[] m_users;
    if(m_pool)
        delete m_pool;
 }

//初始化各种参数
void WebServer::init(bool actorModel,string root,bool mode,int port,int threadPoolMinNum,int threadPoolMaxNum){
    m_actorModel=actorModel;
    m_root=root;
    m_mode=mode;
    m_port=port;
    m_threadPoolMaxNum=threadPoolMaxNum;
    m_threadPoolMinNum=threadPoolMinNum;
}   

 //线程池的初始化
void  WebServer::threadPoolInit(){
    //创建线程池
    try{                                 //定义异常
        m_pool=new ThreadPool<HttpConnect>(m_threadPoolMinNum,m_threadPoolMaxNum);
    }
    catch(...){                     //捕获异常并处理异常，退出程序
        exit(-1);
    }
} 

//事件的监听函数，也就是有关网络的一些监听
void  WebServer::eventListen(){

    //对SIGPIE信号进行处理
    addSignal(SIGPIPE,SIG_IGN);

    m_listenFd=socket(AF_INET,SOCK_STREAM,0);
    if(m_listenFd==-1){
        perror("socket error:");
        exit(1);
    }

    //设置端口复用
    int flag=1;
    setsockopt(m_listenFd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));

    struct sockaddr_in serverAddr;
    serverAddr.sin_port=htons(m_port);
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=INADDR_ANY;

    //绑定端口
    int ret=bind(m_listenFd,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
    if(ret!=0){
        perror("bind error:");
        exit(1);
    }

    //监听
    ret=listen(m_listenFd,128);
    assert(ret >= 0);

    //创建监听红黑树
    int epollFd=epoll_create(MAX_EVENT_NUMBER);
    assert(epollFd != -1);

    //把创建的epollFd赋值给m_epollFd
    m_epollFd=epollFd;
    m_epollEvent.getEpollFd(m_epollFd);

    //将监听的文件描述符添加到epollFd中
    m_epollEvent.addFd(m_listenFd,false,m_mode);

    HttpConnect::m_epollFd=epollFd;

    //创建管道
    ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_pipeFd);
    assert(ret!=-1);

    m_epollEvent.setNonblocking(m_pipeFd[1]);
    m_epollEvent.addFd(m_pipeFd[0],0,m_mode);  //把管道的读端注册到红黑树上进行监听

    //设置信号处理函数
    addSig(SIGALRM);
    addSig(SIGTERM);

    alarm(TIMESLOT);  //定时，5s后产生SIGALARM信号
}  

void  WebServer::eventLoop(){
    bool stopServer=false;  //判断是否关闭服务器
    bool timeout=false;

    while (!stopServer){
        int num=epoll_wait(m_epollFd,m_events,MAX_EVENT_NUMBER,-1);
        if((num<0)&&(errno !=EINTR)){   //epoll失败的时候，EINTR说明是被中断的时候
            cout<<"epoll failure"<<endl;
            break;
        }

        //遍历数组
        for(int i=0;i<num;i++){
            int sockFd=m_events[i].data.fd;
            if(sockFd==m_listenFd){
                bool flag=handleClientData();
                if(flag==false)
                    continue;    
            }else if((sockFd==m_pipeFd[0])&&(m_events[i].events&EPOLLIN)){
                bool flag1=handleSignal(timeout,stopServer);
                if(flag1==false)
                    continue;
            }else if(m_events[i].events&(EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                //对方异常断开或者错误等事件
                deleteTimer(sockFd);
            }else if(m_events[i].events & EPOLLIN){
                handleRead(sockFd);
            }else if(m_events[i].events & EPOLLOUT){
                handleWrite(sockFd);
            }
        }
        // 最后处理定时事件，因为I/O事件有更高的优先级。当然，这样做将导致定时任务不能精准的按照预定的时间执行。
        if( timeout ) {
            timerHeadler();
            timeout = false;
        }
    }
}

void  WebServer::timerInit(int connectFd,struct sockaddr_in clientAddr){
    //将新的客户端的数据初始化，放到数组中
    m_users[connectFd].init(connectFd,clientAddr,m_mode,m_root);
    
    //创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表TimerList中
    UtilTimer *timer=new UtilTimer;
    timer->m_userData=&m_users[connectFd];
    timer->cb_func=cb_func;
    time_t cur=time(NULL);
    timer->m_expire=cur+3*TIMESLOT;  //设置超时时间
    m_users[connectFd].m_timer=timer;
    m_timerList.addTimer(timer);
}

void  WebServer::adjustTimer(int sockFd){
    time_t timeCur=time(NULL);
    if(sockFd!=-1){
        m_users[sockFd].m_timer->m_expire=timeCur+5*TIMESLOT;
        cout<<"adjus timer once"<<endl;
        m_timerList.adjustTimer(m_users[sockFd].m_timer);
    }
}


//处理定时器
void  WebServer::deleteTimer(int sockFd){
    if(m_users[sockFd].m_timer)
        m_timerList.deleteTimer(m_users[sockFd].m_timer);
    m_users[sockFd].closeConnect();
} 

//处理与客户端连接
bool  WebServer::handleClientData(){
    struct sockaddr_in clientAddr;
    socklen_t lenClientAddr=sizeof(clientAddr);

    //有客户端连接进来
    int connectFd=accept(m_listenFd,(struct sockaddr*)&clientAddr,&lenClientAddr);
    if(connectFd==-1){
        perror("accept error:");
        exit(1);
    }

    if(HttpConnect::m_userCount>=MAX_FD){
        //目前连接数满了
        //给客户端写一个信息，服务的内部正忙
        close(connectFd);
        return false;
    }
    timerInit(connectFd,clientAddr);
    return true;
}  

bool WebServer::handleSignal(bool &timeout,bool &stopServer){
    //处理信号
    int sig;
    char signals[1024];
    int ret=recv(m_pipeFd[0],signals,sizeof(signals),0);
    if(ret==-1){
        return false;
    }
    else if(ret==0){
        return false;
    }else{
        for(int i=0;i<ret;i++){
            switch (signals[i])
            {
            case SIGALRM:
                //用timeout变量标记有定时器任务需要处理，但不立即处理定时器任务
                //这是因为定时器任务的优先级不是很高，我们优先处理其他更重要的任务
                timeout=true;
                break;
            case SIGTERM:
                stopServer=true;
            default:
                break;
            }
        }
    }
    return true;
}
    
void  WebServer::handleRead(int sockFd){
    //reactor
    if(m_actorModel){
        
        m_users[sockFd].m_actorModel=m_actorModel;
        m_users[sockFd].m_reactorState=true;
        m_users[sockFd].m_reactorErrorStatus=true;
        m_users[sockFd].m_reactorEnd=false;

        //直接把事件放入队列中
        m_pool->addTask(Task<HttpConnect>(m_users+sockFd));
        while(true){
            if(m_users[sockFd].m_reactorEnd)
                break;
        }
        if(!m_users[sockFd].m_reactorErrorStatus){
            //读数据发生错误
            deleteTimer(sockFd);
        }else{
             // 如果某个客户端上有数据可读，则我们要调整该连接对应的定时器，以延迟该连接被关闭的时间。
            if(m_users[sockFd].m_timer){
                adjustTimer(sockFd);
            }
        }
    }
    else{    //proactor  
        if(m_users[sockFd].read()){
            //一次性把所有数据都读完
            m_pool->addTask(Task<HttpConnect>(m_users+sockFd));
             // 如果某个客户端上有数据可读，则我们要调整该连接对应的定时器，以延迟该连接被关闭的时间。
            if(m_users[sockFd].m_timer){
                adjustTimer(sockFd);
            }
        }
        else{
            //读数据失败了
            deleteTimer(sockFd);
        }
    }
    
}

void WebServer::handleWrite(int sockFd){
    //reactor
    if(m_actorModel){
        
        m_users[sockFd].m_actorModel=m_actorModel;
        m_users[sockFd].m_reactorState=false;
        m_users[sockFd].m_reactorErrorStatus=true;
        m_users[sockFd].m_reactorEnd=false;

        //直接把事件放入队列中
        m_pool->addTask(Task<HttpConnect>(m_users+sockFd));
        while(true){
            if(m_users[sockFd].m_reactorEnd)
                break;
        }
        if(!m_users[sockFd].m_reactorErrorStatus){
            //写数据发生错误
            deleteTimer(sockFd);
        }else{
             //重新调整定时器
            if(m_users[sockFd].m_timer){
                adjustTimer(sockFd);
            }
        }
    }else{
        if(!m_users[sockFd].write()){   
            deleteTimer(sockFd); //一次性写完所有数据失败
        }else{
             //重新调整定时器
            if(m_users[sockFd].m_timer){
                adjustTimer(sockFd);
            }
        }
    }
    
}

//添加信号捕捉函数  为什么这样，可以看这里的说明https://www.cnblogs.com/lit10050528/p/5116566.html
void addSignal(int signal,void(*handler)(int)){
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler=handler;
    sigfillset(&sa.sa_mask);
    sigaction(signal,&sa,NULL);

}

//有关于定时器超时的处理函数
void sigHeadler(int sig){
    int saveErrno=errno;
    int msg=sig;
    //定时器超时的时候信号捕捉的处理函数，通过管道发送信号通知处理
    send(m_pipeFd[1],(char*)&msg,1,0);
    errno=saveErrno;
}

//用注册定时器捕捉信号
void addSig(int sig){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=sigHeadler;
    sa.sa_flags|=SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}

//定时处理任务函数
void timerHeadler(){
    //定时处理任务，实际上就是调用tick函数
    m_timerList.tick();
    //因为一次alarm调用只会引起一次SIGALARM信号，所以我们要重新定时，以不断触发SIGALARM信号
    alarm(TIMESLOT);
}

//定时器回调函数，它删除非活动连接socket上的注册事件，并关闭
void cb_func(HttpConnect *userData){
    epoll_ctl(HttpConnect::m_epollFd,EPOLL_CTL_DEL,userData->m_socketFd,0);
    assert(userData);
    close(userData->m_socketFd);
    HttpConnect::m_userCount--;
    cout<<"close fd:"<<userData->m_socketFd<<endl;
}