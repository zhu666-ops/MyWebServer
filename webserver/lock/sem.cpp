#include "sem.h"
#include <exception>
using namespace std;


Sem::Sem(){
    if(sem_init(&m_sem,0,0)!=0){
        throw exception();
    }
}

Sem::Sem(int num){
    if(sem_init(&m_sem,0,num)!=0){
        throw exception();
    }
}

//等待信号量
bool Sem::wait(){
    return sem_wait(&m_sem)==0;
}

//增加信号量
bool Sem::post(){
    return sem_post(&m_sem)==0;
}


Sem::~Sem(){
    sem_destroy(&m_sem);
}