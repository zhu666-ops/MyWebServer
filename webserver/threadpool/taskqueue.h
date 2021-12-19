#pragma once
#include <queue>
#include <iostream>
#include <deque>
#include <pthread.h>
using namespace std;

using callback=void(*)(void *);

//任务结构体
template <typename T>
struct Task{
    Task(){
        function=nullptr;
        arg=nullptr;
    }
    Task(T *arg){
        function=nullptr;
        this->arg=arg;
    }
    Task(callback f,T *arg){
        this->arg=(T*)arg;
        this->function=f;
    }
    callback function;
    T *arg;
};

template <typename T>
class TaskQueue{
public:
    TaskQueue();
    ~TaskQueue();

    //添加任务
    void addTask(Task<T> task);

    //添加任务
    void addTask(Task<T> task,callback f,void *arg);

    //取出一个任务
    Task<T> takeTask();

    //获取当前任务的个数
    inline size_t taskNumber(){
        return m_taskQ.size();
    }

private:
    pthread_mutex_t m_mutex;
    queue<Task<T>> m_taskQ;
};