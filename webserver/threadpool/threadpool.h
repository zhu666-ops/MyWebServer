#pragma once
#include "taskqueue.h"

using namespace std;

template <typename T>
class ThreadPool{
public:
    //创建线程池并初始化
    ThreadPool(int min,int max);
    //销毁线程池
    ~ThreadPool();
    //给线程池添加任务
    void addTask(Task<T> task);

    //获取忙的线程个数
    int getBusyNum();

    //获取活着的线程个数
    int getAliveNum();

private:
    //工作线程任务函数
    static void *worker(void *arg);
    //管理者线程工作函数
    static void *manager(void *arg);
    //单个线程退出函数
    void threadExit(ThreadPool<T> *pool);

private:
    TaskQueue<T> *m_taskQueue;

    pthread_t m_manager;
    pthread_t *m_threadIds;
    int m_maxNum;
    int m_minNum;
    int m_busyNum;
    int m_liveNum;
    int m_exitNum;

    pthread_mutex_t m_mutexPool;
    pthread_mutex_t m_mutexBusy;
    pthread_cond_t m_notEmpty;

    bool m_shutdown;

    static const int NUMBER=2;  //管理者线程要添加和销毁的线程数量
};