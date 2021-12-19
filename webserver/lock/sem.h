#pragma once
#include <semaphore.h>
using namespace std;

class Sem
{
public:
    Sem();

    //有参构造，传入初始化信号量的数目
    Sem(int num);

    //等待信号量
    bool wait();

    //增加信号量
    bool post();

    ~Sem();
private:
    sem_t m_sem;
};