#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
using namespace std;

class Epoll{
public:
    Epoll();

    Epoll(int fd);

    ~Epoll();

    //取得epollFd
    void getEpollFd(int fd);

    //添加文件描述符到epoll中
    void addFd(int fd,bool oneShot,bool mode);

    //修改文件描述符
    void modFd(int fd,int ev);

    //从epoll中删除文件描述符
    void removeFd(int fd);

    //设置文件描述符为非阻塞
    void setNonblocking(int fd);
private:
    int epollFd;
};