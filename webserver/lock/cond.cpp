#include <iostream>
#include <exception>
#include "cond.h"

using namespace std;

Cond::Cond(){
    if(pthread_cond_init(&m_cond,NULL)!=0){
        throw exception();
    }
}

//使用条件变量进行pthread_cond_wait操作--
bool Cond::wait(pthread_mutex_t * mutex){
    return pthread_cond_wait(&m_cond,mutex)==0;
}

//使用条件变量进行pthread_cond_timedwait操作的超时时间操作
bool Cond::timedWait(pthread_mutex_t * mutex,struct timespec t){
    return pthread_cond_timedwait(&m_cond,mutex,&t)==0;
}

 //使用条件变量进行pthread_cond_signal操作++,只有一个
bool Cond::signal(){
    return pthread_cond_signal(&m_cond)==0;
}

//将所有条件变量唤醒
bool Cond::broadcast(){
    return pthread_cond_broadcast(&m_cond)==0;
}


//销毁条件变量
Cond::~Cond(){
    pthread_cond_destroy(&m_cond);
}