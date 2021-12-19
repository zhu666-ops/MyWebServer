#include "epoll.h"
using namespace std;

Epoll::Epoll(){
    epollFd=-1;
}

Epoll::Epoll(int fd){
    this->epollFd=fd;
}

//取得epollFd
void Epoll::getEpollFd(int fd){
    this->epollFd=fd;
}

//设置文件描述符为非阻塞
void Epoll::setNonblocking(int fd){
    int flag=fcntl(fd,F_GETFL);
    flag|=O_NONBLOCK;
    fcntl(fd,F_SETFL,flag);
}

//用来向epoll中添加需要监听的文件描述符
void Epoll::addFd(int fd,bool oneShot,bool mode){
    epoll_event event;
    event.data.fd=fd;
    //EPOLLRDHUP事件底层触发读端是否断开，然后处理断开请求
    //默认使用水平触发，也可以使用边沿触发
    if(mode)
        event.events=EPOLLIN | EPOLLET | EPOLLRDHUP;  //边沿触发
    else 
        event.events=EPOLLIN | EPOLLRDHUP;
  

    if(oneShot){
        event.events|=EPOLLONESHOT;
    }

    epoll_ctl(epollFd,EPOLL_CTL_ADD,fd,&event);

    //设置文件描述符为非阻塞
    setNonblocking(fd);
}

//用来向epoll中删除监听的文件描述符
void Epoll::removeFd(int fd){
    epoll_ctl(epollFd,EPOLL_CTL_DEL,fd,NULL);
    close(fd);
}

//修改文件描述符,重置socket上的EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发
void Epoll::modFd(int fd,int ev){
    epoll_event event;
    event.data.fd=fd;
    event.events=ev | EPOLLONESHOT |EPOLLRDHUP;
    epoll_ctl(epollFd,EPOLL_CTL_MOD,fd,&event);
}
Epoll::~Epoll(){

}