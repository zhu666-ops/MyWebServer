#pragma once
#include <pthread.h>
using namespace std;
//线程同步机制封装类

//互斥锁类
class Locker
{
public:
    Locker();

    //上锁
    bool lock();
    //解锁
    bool unlock();

    //获取互斥锁
    pthread_mutex_t *getMutex();

    //销毁锁
    ~Locker();

private:
    pthread_mutex_t m_mutex;


};