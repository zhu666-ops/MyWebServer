#pragma once
#include <pthread.h>
using namespace std;


//条件变量
class Cond
{
public:
    Cond();

    //使用条件变量进行pthread_cond_wait操作--
    bool wait(pthread_mutex_t *mutex);

    //使用条件变量进行pthread_cond_timedwait操作的超时时间操作
    bool timedWait(pthread_mutex_t * mutex,struct timespec t);

    //使用条件变量进行pthread_cond_signal操作++,唤醒
    bool signal();

    //将所有条件变量唤醒
    bool broadcast();

    //销毁条件变量
    ~Cond();

private:
    pthread_cond_t m_cond;
};