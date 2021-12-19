#pragma once
#include <iostream>
#include <time.h>
using namespace std;

class HttpConnect; //前置声明


//定时器类
class UtilTimer{
public:
    UtilTimer():m_expire(0),prev(nullptr),next(nullptr),m_userData(nullptr){}

    time_t m_expire; //任务超时时间，这里使用了绝对时间

    void (*cb_func) (HttpConnect*);//任务回调函数，回调函数处理的客户端数据，由定时器的执行者传递给回调函数

    HttpConnect * m_userData;

    UtilTimer *prev;  //指向前一个定时器
    UtilTimer *next; //指向后一个定时器
};

//定时器链表类，它是一个升序、双向链表，并且带有头节点和尾节点
class SortTimerList{
public:
    SortTimerList();  //定时器链表构造函数

    //将目标定时器timer添加到链表中
    void addTimer(UtilTimer *timer);

    //当某个定时任务发送改变时，调整对应的定时器在链表中的位置。这个函数只考虑被调整的定时器的
    //超时时间延长情况，即该定时器需要往链表的尾部移动
    void adjustTimer(UtilTimer *timer);

    //将定时器timer从链表中删除
    void deleteTimer(UtilTimer *timer);

    //SIGALARM信号每次被触发就在其信号处理函数中执行一次tick()函数，以处理链表上到期任务
    void tick();

    ~SortTimerList();  //定时器链表析构函数

private:
    /*一个重载的辅助函数，它被公有的addTimer函数和adjustTimer函数调用
    该函数表示将目标定时器timer添加到节点listHead之后的部分链表中*/
    void addTimer(UtilTimer *timer,UtilTimer *listHead);


private:
    UtilTimer *head; //头节点
    UtilTimer *tail;  //尾节点

};