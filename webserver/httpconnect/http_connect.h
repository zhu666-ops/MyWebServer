#pragma once 
#include <iostream>
#include <sys/socket.h>
#include <string>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/uio.h>
#include <string.h>
#include "../utiltimer/util_timer.h"
using namespace std;

#define READ_BUFFER_SIZE 2048  //读缓冲区的大小
#define WRITE_BUFFER_SIZE 2048  //写缓冲区的大小
#define FILENAME_LEN 200     //文件的最大长度


class HttpConnect
{
public:
 //使用有限状态机
    //HTTP请求方法，这里只支持GET请求
    enum METHOD {GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT};
    /*
     *  解析客户端请求时，主状态机的状态
     *  CHECK_STATE_REQUESTLINE:当前正在分析请求行
     *  CHECK_STATE_HEADER: 当前正在分析头部字段
     *  CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE=0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT};

    /*
     *  服务器处理http请求的可能结果，报文解析的结果
     *  NO_REQUEST   :请求不完整，需要继续读取客户数据
     *  GET_REQUEST   :表示获得了一个完成的客户请求
     *  BAD_REQUEST   :表示客户端请求语法错误
     *  NO_RESOURCE   :表示客户端没有资源
     *  FORBIDDEN_REQUEST   :表示客户对资源没有足够的访问权限
     *  FILE_REQUEST    :文件请求，获取文件成功
     *  INTERNAL_ERROR  :表示服务器内部错误
     *  CLOSED_CONNECTION   :表示客户端已经关闭连接了
    */
   enum HTTP_CODE {NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,
                    FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,
                    CLOSED_CONNECTION};

    //从状态机的三种可能状态，即行的读取状态，分别表示
    //1.读取到一个完整的行 2.行出错  3.行数据尚不完整（就是还没有读完整行数据）
    enum LINE_STATUS {LINE_OK=0,LINE_BAD,LINE_OPEN};


    HttpConnect();

    //处理客户端的请求
    void process();
    
    void init(int socketFd,const struct sockaddr_in &clientAddr,bool mode,string root);   //初始化函数
    void init();   //初始化连接其余的信息

    //关闭连接
    void closeConnect();   

    //非阻塞的读数据
    bool read();

    //非阻塞的写数据
    bool write();

    //得到一行数据
    char * getLine();

    HTTP_CODE processRead();   //解析http请求
    bool processWrite(HTTP_CODE ret);   //根据服务器处理HTTP请求的结果，决定返回给客户端的内容
    HTTP_CODE parseRequestLine(char *text);  //解析请求首行
    HTTP_CODE parseHeaders(char *text);    //解析请求头
    HTTP_CODE parseContent(char *text);    //解析请求体

    LINE_STATUS parseLine();     //解析一行

    HTTP_CODE doRequest();   //处理信息

    //对内存映射区执行munmap操作
    void unmap();

    //关于服务器回发给客户端的响应头信息相关函数
    //往写缓冲区种写入待发送的数据
    bool addResponse(const char* format,...);

    //添加响应行
    bool addStatusLine(int status,const char *title);

    //有关响应头的添加
    bool addHeaders(int contentLen);

    //响应头文件的长度
    bool addContentLength(int contentLen);

    //响应头文件的类型"text/html"
    bool addContentType();

    //响应头的Connection状态，比如是keep-alive还是close
    bool addLinger();

    //响应头的空行
    bool addBlankLine();

    //显示回馈信息的函数,
    bool addContent(const char * content);


    ~HttpConnect();

    static int m_epollFd;  //所有的socket上的事件都被注册到同一个epoll对象中
    static int m_userCount;  //统计用户的数量

    int m_socketFd;  //该http连接的socket

    UtilTimer *m_timer;  //定时器

    //reactor和proactor模型选择
    bool m_actorModel;

    bool m_reactorState;    //reactor是读事件的状态还是写事件的状态.ture表示读，false表示写
    bool m_reactorErrorStatus;          //reactor发送读事件或者写事件的时候，发生的是读错误还是写错误.true表示正确，false表示错误
    bool m_reactorEnd;    //判断reactor读事件结束和写事件结束
private:
    struct sockaddr_in m_address;  //通信的socket客户端地址
    char m_readBuf[READ_BUFFER_SIZE];   //读缓冲区
    int m_readIdx;  //标识读缓冲区中以及读入的客户端数据的最后一个字节的下一个数据的位置

    int m_checkedIndex;   //当前正在分析的字符在读缓冲区的位置
    int m_startLine;   //当前正在解析行的起始位置

    char *m_url;  //请求目标文件的文件名
    char *m_version;  //协议版本，只支持HTTP1.1
    METHOD m_method;  //请求方法
    char *m_host;     //主机名
    bool m_linger;  //HTTP请求是否需要保持连接
    int m_contentLength;  //请求消息体字节长度


    char m_realFile[200];  //客户端发送给过来的文件存在的路径
    struct stat m_fileStat;   //关于使用stat相关的结构体，用来判断文件的属性
    char * m_fileAddress;   //共享内存映射区的地址

    char m_writeBuf[WRITE_BUFFER_SIZE];   //写缓冲区
    int m_writeIdx;                     //写缓冲区中待发送的数据的字节数
    struct iovec m_iv[2];                // 采用writev来执行写操作，所以定义下面两个成员，其中。
                                        //这个结构数组为2的意思是有两部分需要发送的内存，一部分是写缓冲区中的内存，一部分是内存映射区中的内存
    int m_ivCount;                  //m_ivCount表示被写内存块的数量，就是上面两个部分的内存，也就是2

    CHECK_STATE m_checkState;  //主状态机当前所在的状态

    bool m_mode;

    char docRoot[100];
};


