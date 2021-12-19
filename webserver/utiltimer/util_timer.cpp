#include "util_timer.h"
using namespace std;


//初始化定时器链表类
SortTimerList::SortTimerList(){
    this->head=nullptr;
    this->tail=nullptr;
}

//将目标定时器timer添加到链表中
void SortTimerList::addTimer(UtilTimer *timer){
    if(!timer)
        return;
    if(!head){
        head=tail=timer;
        return;
    }

    /*如果目标定时器的超时时间小于当前链表中所有定时器的超时时间，则把该定时器插入链表头部，作为链表新的头节点
      否则就需要调用重载函数addTimer，把它插入链表中合适的位置，以保证链表的升序特性
    */
    if(timer->m_expire<head->m_expire){
        timer->next=head;
        head->prev=timer;
        head=timer;
        return;
    }
    addTimer(timer,head);
}

/*一个重载的辅助函数，它被公有的addTimer函数和adjustTimer函数调用
    该函数表示将目标定时器timer添加到节点listHead之后的部分链表中*/
void SortTimerList::addTimer(UtilTimer *timer,UtilTimer *listHead){
    UtilTimer *prev=listHead;
    UtilTimer *tmp=prev->next;
    /*遍历listHead节点之后的部分链表，直到找到一个超时时间大于目标时间定时器的超时时间节点*/
    while(tmp){
        if(timer->m_expire<tmp->m_expire){
            prev->next=timer;
            timer->next=tmp;
            timer->prev=prev;
            tmp->prev=timer;
            break;
        }
        prev=tmp;
        tmp=prev->next;
    }

    /* 如果遍历完 listhead 节点之后的部分链表，仍未找到超时时间大于目标定时器的超时时间的节点，
        则将目标定时器插入链表尾部，并把它设置为链表新的尾节点。*/
    if(!tmp){
        prev->next=timer;
        timer->prev=prev;
        timer->next=nullptr;
        tail=timer;
    }
}

/*当某个定时任务发送改变时，调整对应的定时器在链表中的位置。这个函数只考虑被调整的定时器的
超时时间延长情况，即该定时器需要往链表的尾部移动*/
void SortTimerList::adjustTimer(UtilTimer *timer){
    if(!timer)
        return;
    UtilTimer *tmp=timer->next;
    /*如果被调整的目标定时器处在链表的尾部，或者该定时器新的超时时间仍然小于其下一个定时器的
      超时时间则不用调整*/
    if(!tmp||(timer->m_expire<tmp->m_expire)){
        return ;
    }
    //如果目标定时器是链表的头节点，则将该定时器从链表中取出并重新插入链表中
    if(head==timer){
        head=head->next;
        head->prev=nullptr;
        timer->next=nullptr;
        addTimer(timer,head);
    }
    else{
        //如果目标定时器不是链表的头节点，则将该定时器从链表中取出，然后插入其原来所在位置后的部分链表中
        timer->prev->next=timer->next;
        timer->next->prev=timer->prev;
        addTimer(timer,timer->next);
    } 
}

//将定时器timer从链表中删除
void SortTimerList::deleteTimer(UtilTimer *timer){
    if(!timer)
        return;
    //下面这个条件成立表示链表中只有一个定时，即目标定时器
    if((head==timer)&&(tail==timer)){
        delete timer;
        head=tail=nullptr;
        return;
    }
    /*如果链表中至少有两个定时器，并且目标定时器是链表的头节点，
    则将链表的头节点重置为原来头节点的下一个节点，然后删除目标定时器*/
    if(head==timer){
        head=head->next;
        head->prev=nullptr;
        delete timer;
        return;
    }
    /* 如果链表中至少有两个定时器，且目标定时器是链表的尾节点，
        则将链表的尾节点重置为原尾节点的前一个节点，然后删除目标定时器。*/
    if(tail==timer){
        tail=tail->prev;
        tail->next=nullptr;
        delete timer;
        return;
    }
    //如果目标定时器位于链表的中间，则把它前后的定时器串联起来，然后删除目标定时器
    timer->prev->next=timer->next;
    timer->next->prev=timer->prev;
    delete timer;
}

//SIGALARM信号每次被触发就在其信号处理函数中执行一次tick()函数，以处理链表上到期任务
void SortTimerList::tick(){
    if(!head)
        return ;
    cout<<"timer tick"<<endl;
    time_t cur=time(NULL);   //获取当前系统的时间
    UtilTimer *tmp=head;
    //从头节点开始依次处理每个定时器，直到遇到一个尚未到期的定时器
    while(tmp){
        //遇到一个未到期的定时器，推出函数
        if(cur<tmp->m_expire)
            break;
        //调用定时器的回调函数，以执行定时任务
        tmp->cb_func(tmp->m_userData);
        //执行完定时器中的定时任务之后，就将它从链表中删除，并重置链表头记得
        head=tmp->next;
        if(head)
            head->prev=nullptr;
        delete tmp;
        tmp=head;
    }
}


//定时器链表析构函数
SortTimerList::~SortTimerList(){
    UtilTimer *tmp=head;
    if(tmp){
        head=tmp->next;
        delete tmp;
        tmp=head;
    }
} 


