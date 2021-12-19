#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fcntl.h>
#include "http_connect.h"
#include "../utiltimer/util_timer.h"
using namespace std;

int HttpConnect::m_epollFd=-1;  //所有的socket上的事件都被注册到同一个epoll对象中
int HttpConnect::m_userCount=0;  //统计用户的数量

//定义HTTP响应的一些状态信息
const char * ok_200_title="OK";
const char * error_400_title="Bad Request";
const char * error_400_form="Your request has bad syntax or is inherently impossible";
const char * error_403_title="Forbidden";
const char * error_403_form="You do not have permission to get file from this server";
const char * error_404_title="Not Found";
const char * error_404_form="The requested file was not found on this server.";
const char * error_500_title="Internale Error";
const char * error_500_form="There was an unusual problem serving the requested file";





//添加文件描述符到epoll中
extern void addFd(int epollFd,int fd,bool oneShot,bool mode);

//修改文件描述符
extern void modFd(int epollFd,int fd,int ev);

//从epoll中删除文件描述符
extern void removeFd(int epollFd,int fd);

//设置文件描述符为非阻塞
extern void setNonblocking(int fd);

HttpConnect::HttpConnect(){

}

//由线程池中的工作线程调用，这是处理Http请求的入口函数
void HttpConnect::process(){
    //解析HTTP请求
    HTTP_CODE readRet=processRead();
    if(readRet==NO_REQUEST){
        if(m_mode)
            modFd(m_epollFd,m_socketFd,EPOLLIN | EPOLLET);
        else 
             modFd(m_epollFd,m_socketFd,EPOLLIN);
        return ;
    }
    //生成响应
    bool writeRet=processWrite(readRet);
    if(!writeRet){
        //如果相应没有成功，则关闭连接
        closeConnect();
    }
    //把写事件开启监听，并添加到红黑树
    modFd(m_epollFd,m_socketFd,EPOLLOUT);
}

 //初始化连接函数
void HttpConnect::init(int socketFd,const struct sockaddr_in &addr,bool mode,string root){
    m_socketFd=socketFd;
    m_address=addr;
    m_mode=mode;
    
    for(int i=1;i<root.size();i++){
        docRoot[i-1]=root[i];
    }
    docRoot[root.size()-2]='\0';

    //设置端口复用
    int flag=1;
    setsockopt(m_socketFd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));

    //添加到epoll对象中
    addFd(m_epollFd,m_socketFd,true,m_mode);
    m_userCount++;

    init();
}  


//初始化其余连接相关的信息
void HttpConnect::init(){
    m_checkState=CHECK_STATE_REQUESTLINE;   //初始化状态为解析请求首行
    m_checkedIndex=0;    
    m_startLine=0;
    //初始化m_readIdx
    m_readIdx=0;

    m_method=GET;
    m_url=nullptr;
    m_version=nullptr;
    m_host=nullptr;
    m_linger=false;
    m_contentLength=0;

    m_startLine=0;
    m_writeIdx=0;
    m_timer=nullptr;

    bzero(m_readBuf,READ_BUFFER_SIZE);  //把缓冲区数据清零
    bzero(m_writeBuf,WRITE_BUFFER_SIZE);
    bzero(m_realFile,FILENAME_LEN);
}

//关闭连接
void HttpConnect::closeConnect(){
    if(m_socketFd!=-1){
        removeFd(m_epollFd,m_socketFd);
        m_socketFd=-1;
        m_userCount--;
    }
}

//非阻塞的读数据
//循环的读取客户端发送过来的数据，直到无数据可以读或者对方关闭连接
bool HttpConnect::read(){
    //当读缓冲区的最后一个位置的下一个位置的大小比读缓冲区的数组长度还要大，则返回失败
    if(m_readIdx>=READ_BUFFER_SIZE)
        return false;
    
    //读取到的字节
    int readBytes=0;
    while(true){
        readBytes=recv(m_socketFd,m_readBuf+m_readIdx,READ_BUFFER_SIZE-m_readIdx,0);
        if(readBytes==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                //没有数据
                break;
            }
            //否则发生错误，返回失败
            return false;
        }
        else if(readBytes==0){
            //对端关闭
            return false;
        }
        //标识读缓冲区中以及读入的客户端数据的最后一个字节的下一个数据的位置
        m_readIdx+=readBytes;
    }
    cout<<m_readBuf;
    cout<<"读取到了数据:"<<m_readIdx<<endl;
    return true;
}


//非阻塞的写数据
bool HttpConnect::write(){
    int temp=0;
    int bytesHaveSend=0;   //已经发送的字节
    int bytesToSend=m_writeIdx;  //将要发送的字节，(m_writeIdx)写缓冲区中待发送的字节数

    if(bytesToSend==0){
        //将要发送的字节为0，这一次响应结束
        if(m_mode)
            modFd(m_epollFd,m_socketFd,EPOLLIN | EPOLLET);
        else 
             modFd(m_epollFd,m_socketFd,EPOLLIN);
        init();
        return true;
    }

    while(true){
        //分散写，一部分是写缓冲区中的内存，就是响应头部分的内存，一部分是客户端请求文件的内存，而请求文件的内存被映射到了内存映射区，所以就内存映射区的内存
        temp=writev(m_socketFd,m_iv,m_ivCount);
        if(temp<=-1){
            //如果TCP写缓冲区没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间
            //服务器无法立即接受到同一客户的下一个请求，但是可以保证连接的完整性
            if(errno==EAGAIN){
                modFd(m_epollFd,m_socketFd,EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytesToSend-=temp;
        bytesHaveSend+=temp;
        if(bytesToSend<=bytesHaveSend){
            //发送HTTP响应成功，根据HTTP请求中的Connection字段决定是长连接还是短连接，如果是短连接，则直接关闭
            unmap();
            if(m_linger){  //如果是长连接，则继续监听该连接
                init();
                if(m_mode)
                    modFd(m_epollFd,m_socketFd,EPOLLIN | EPOLLET);
                else 
                    modFd(m_epollFd,m_socketFd,EPOLLIN);
                return true;
            }       //如果是短连接，则返回false，再关闭连接
            else{
                if(m_mode)
                    modFd(m_epollFd,m_socketFd,EPOLLIN | EPOLLET);
                else 
                    modFd(m_epollFd,m_socketFd,EPOLLIN);
                return false;
            }
        }
    }

    return true;
}

 //获取一行数据
char* HttpConnect::getLine(){
    return m_readBuf+m_startLine;
}

//解析http请求 
//主状态机，解析请求
 HttpConnect::HTTP_CODE HttpConnect::processRead(){
     LINE_STATUS lineStatus=LINE_OK;
     HTTP_CODE ret=NO_REQUEST;

     char *text=NULL;
    //假如解析的这一行是正确的，则继续循环的解析
    //(m_checkState==CHECK_STATE_CONTENT)&&(lineStatus==LINE_OK)表示当请求行解析完后，解析请求体的时候需要满足的状态
    //解析到了一行完整的数据，或者解析到了请求体，也是完整的数据
     while(((m_checkState==CHECK_STATE_CONTENT)&&(lineStatus==LINE_OK))||(lineStatus=parseLine())==LINE_OK){
         //获取一行数据
         text=getLine();

         m_startLine=m_checkedIndex;
         cout<<"get 1 http line:"<<text<<endl;

         switch (m_checkState)
         {
            case CHECK_STATE_REQUESTLINE:
            {
                ret=parseRequestLine(text);
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret=parseHeaders(text);
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }
                else if(ret==GET_REQUEST){
                    return doRequest();   //解析具体请求的信息
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret=parseContent(text);
                if(ret==GET_REQUEST){
                    return doRequest();
                }
                //失败的时候
                lineStatus=LINE_OPEN;
                break;
            }
            default:{
                return INTERNAL_ERROR;  //返回错误
            }
         }
     }
     return NO_REQUEST;
 }   
    

//根据服务器处理HTTP请求的结果，处理相应的请求
bool HttpConnect::processWrite(HTTP_CODE ret){
    switch (ret)
    {
    case INTERNAL_ERROR:
        addStatusLine(500,error_500_title);
        addHeaders(strlen(error_500_form));
        if(!addContent(error_500_form)){
            return false;
        }
        break;
    case BAD_REQUEST:
        addStatusLine(400,error_400_title);
        addHeaders(strlen(error_500_form));
        if(!addContent(error_500_form)){
            return false;
        }
        break;
    case NO_RESOURCE:
        addStatusLine(404,error_404_title);
        addHeaders(strlen(error_404_form));
        if(!addContent(error_404_form)){
            return false;
        }
        break;
    case FORBIDDEN_REQUEST:
        addStatusLine(403,error_403_title);
        addHeaders(strlen(error_403_form));
        if(!addContent(error_403_form)){
            return false;
        }
        break;
    case FILE_REQUEST:
        addStatusLine(200,ok_200_title);
        addHeaders(m_fileStat.st_size);
        m_iv[0].iov_base=m_writeBuf;
        m_iv[0].iov_len=m_writeIdx;
        m_iv[1].iov_base=m_fileAddress;
        m_iv[1].iov_len=m_fileStat.st_size;
        m_ivCount=2;
        return true;
        break;
    default:
        return false;
        break;
    }
    m_iv[0].iov_base=m_writeBuf;
    m_iv[0].iov_len=m_writeIdx;
    m_ivCount=1;
    return true;
}

 //解析http请求行,获得请求方法，目标URL，HTTP版本
HttpConnect::HTTP_CODE HttpConnect::parseRequestLine(char *text){
    //Get /index.html Http/1.1
    m_url=strpbrk(text," \t");  //这里使用的是正则表达式,找到第一个空格或者\t的位置的指针赋值给m_url

    //GET\0 /index.html HTTP/1.1
    *m_url++='\0';

    char *method=text;  //因为第一个变成了\0，所以可以直接把GET取出来了
    if(strcasecmp(method,"GET")==0){
        m_method=GET;
    }
    else{
        return BAD_REQUEST;
    }

    //  /index.html HTTP/1.1
    m_version=strpbrk(m_url," \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    //  /index.html/0HTTP/1.1
    *m_version++='\0';
    if(strcasecmp(m_version,"HTTP/1.1")!=0){
        return BAD_REQUEST;
    }

    //http://192.168.1.1:10000/index.html
    if(strncasecmp(m_url,"http://",7)==0){
        m_url+=7;   //192.168.1.1:10000/index.html
        m_url=strchr(m_url,'/');   // /index.html  这里的意思是把192.168.1.1:10000的部分切掉,保留/index.html的部分
    }

    if(!m_url || m_url[0]!='/'){
        return BAD_REQUEST;
    }

    m_checkState=CHECK_STATE_HEADER;  //主状态机检查状态变成检查请求头

    return NO_REQUEST; 
} 

//解析请求头
//解析HTTP请求的一个头部信息
HttpConnect::HTTP_CODE HttpConnect::parseHeaders(char *text){
    //当遇到空行，表示头部字段解析完毕
    if(text[0]=='\0'){
        //如果HTTP请求有消息体，则还需要读取m_contentLenght字节的消息体
        //状态机转移到CHECK_STATE_CONTENT状态
        if(m_contentLength!=0){
            m_checkState=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"Connection:",11)==0){
        //处理Connection头部字段  Connection: keep-alive
        text +=11;
        text +=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0){
            m_linger=true;
        }
    }
    else if(strncasecmp(text,"Content-Length:",15)==0){
        //处理Content-Length头部字段
        text+=15;
        text+=strspn(text," \t");
        m_contentLength=atol(text);
    }
    else if(strncasecmp(text,"Host:",5)==0){
        //处理Host头部字段
        text+=5;
        text+=strspn(text," \r");
        m_host=text;
    }
    else{
        cout<<"oop! unknow header "<<text<<endl;
    }
    return NO_REQUEST;
}

 //解析请求体
 //我们没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
HttpConnect::HTTP_CODE HttpConnect::parseContent(char *text){
    if(m_readIdx>=(m_contentLength+m_checkedIndex)){
        text[m_contentLength]='\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}   

 //解析一行,判断的依据是\r\n，这里就是判断一行的数据怎么样的状态机
HttpConnect::LINE_STATUS HttpConnect::parseLine(){
    char temp;
    for(;m_checkedIndex<m_readIdx;m_checkedIndex++){
        temp=m_readBuf[m_checkedIndex];
        if(temp=='\r'){
            if(m_checkedIndex+1==m_readIdx)  //m_readIdx是最后一位的，假如m_checkedIndex+1=m_readIdx,则说明m_checkIndex还差一位
                return LINE_OPEN;
            else if(m_readBuf[m_checkedIndex+1]=='\n'){
                m_readBuf[m_checkedIndex++]='\0';  //这里就是把'\r''\n',替换成'\0'，让它结束成一个具有'\0'的字符串
                m_readBuf[m_checkedIndex++]='\0';
                return LINE_OK;
            }
            else 
                return LINE_BAD;   //其他情况，则说明改行出错
                
        }
        else if(temp=='\n'){
            if(m_checkedIndex>1&&(m_readBuf[m_checkedIndex-1]=='\r')){   //当读到\n的时候，则需要判断\n前面那个字符是否为\r，m_checkedIndex-1>0的判断是为了防止数组越界
                m_readBuf[m_checkedIndex-1]='\0';
                m_readBuf[m_checkedIndex++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}    

//处理信息
//当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性
//如果目标文件存在，对所有用户可读，且不是目录，则使用mmap将其映射到内存地址m_file_address处，并告诉调用者获取文件成功
HttpConnect::HTTP_CODE HttpConnect::doRequest(){
    
    strcpy(m_realFile,docRoot);
    int len=strlen(docRoot);
    strncpy(m_realFile+len,m_url,FILENAME_LEN-len-1);
    //获取m_realFile文件的相关的状态信息，-1失败，0成功
    if(stat(m_realFile,&m_fileStat)<0){
        return NO_REQUEST;
    }
    //判断文件的访问权限
    if(!(m_fileStat.st_mode&S_IROTH)){
        return FORBIDDEN_REQUEST;
    }

    //判断是否是目录
    if(S_ISDIR(m_fileStat.st_mode)){
        return BAD_REQUEST;
    }

    //以只读方式打开文件
    int fd=open(m_realFile,O_RDONLY);
    //创建内存映射区
    m_fileAddress=(char *)mmap(0,m_fileStat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}


//对内存映射区执行munmap操作，释放内存映射区的资源
void HttpConnect::unmap(){
    if(m_fileAddress){
        munmap(m_fileAddress,m_fileStat.st_size);
        m_fileAddress=nullptr;
    }
}

//往写缓冲区里面写入待发送的数据
bool HttpConnect::addResponse(const char *format,...){
    //写缓冲区的大小不够的时候，返回false
    if(m_writeIdx>=WRITE_BUFFER_SIZE){
        return false;
    }
    va_list argList;
    va_start(argList,format);
    int len=vsnprintf(m_writeBuf+m_writeIdx,WRITE_BUFFER_SIZE-m_writeIdx-1,format,argList);
    //写缓冲区写不下了
    if(len>=(WRITE_BUFFER_SIZE-m_writeIdx-1)){
        return false;
    }
    m_writeIdx+=len;
    va_end(argList);
    return true;
}

//添加响应行
bool HttpConnect::addStatusLine(int status,const char* title){
    return addResponse("%s %d %s\r\n","HTTP/1.1",status,title);
}

//有关响应头的添加
bool HttpConnect::addHeaders(int contentLen){
    addContentLength(contentLen);
    addContentType();
    addLinger();
    addBlankLine();
}

//响应头文件的长度
bool HttpConnect::addContentLength(int contentLen){
    return addResponse("Content-Length: %d\r\n",contentLen);
}

//响应头文件的类型"text/html"
bool HttpConnect::addContentType(){
    return addResponse("Content-Type:%s\r\n","text/html");
}

//响应头的Connection状态，比如是keep-alive还是close
bool HttpConnect::addLinger(){
    return addResponse("Connection: %s\r\n",(m_linger==true)?"keep-alive":"close");
}

//响应头的空行
bool HttpConnect::addBlankLine(){
    return addResponse("%s","\r\n");
}

//显示回馈信息的函数
bool HttpConnect::addContent(const char *content){
    return addResponse("%s",content);
}

HttpConnect::~HttpConnect(){
    
}

//设置文件描述符为非阻塞
void setNonblocking(int fd){
    int flag=fcntl(fd,F_GETFL);
    flag|=O_NONBLOCK;
    fcntl(fd,F_SETFL,flag);
}


//用来向epoll中添加需要监听的文件描述符
void addFd(int epollFd,int fd,bool oneShot,bool mode){
    epoll_event event;
    event.data.fd=fd;
    //EPOLLRDHUP事件底层触发读端是否断开，然后处理断开请求
    //默认使用水平触发，也可以使用边沿触发
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
void removeFd(int epollFd,int fd){
    epoll_ctl(epollFd,EPOLL_CTL_DEL,fd,NULL);
    close(fd);
}

//修改文件描述符,重置socket上的EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发
void modFd(int epollFd,int fd,int ev){
    epoll_event event;
    event.data.fd=fd;
    event.events=ev | EPOLLONESHOT |EPOLLRDHUP;
    epoll_ctl(epollFd,EPOLL_CTL_MOD,fd,&event);
}