#include <iostream>
#include <exception>
#include "locker.h"
using namespace std;

Locker::Locker(){
    //初始化互斥锁
    if(pthread_mutex_init(&m_mutex,NULL)!=0){
        throw exception();
    }
}

bool Locker::lock(){
    return pthread_mutex_lock(&m_mutex)==0;
}

bool Locker::unlock(){
    return pthread_mutex_unlock(&m_mutex)==0;
}

pthread_mutex_t *Locker::getMutex(){
    return &m_mutex;
}

Locker::~Locker(){
    //销毁互斥锁
    pthread_mutex_destroy(&m_mutex);
}

