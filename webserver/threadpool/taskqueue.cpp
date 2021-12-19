#include "taskqueue.h"

template <typename T>
TaskQueue<T>::TaskQueue(){
    pthread_mutex_init(&m_mutex,NULL);
}

template <typename T>
TaskQueue<T>::~TaskQueue(){
    pthread_mutex_destroy(&m_mutex);
}

//添加任务
template <typename T>
void TaskQueue<T>::addTask(Task<T> task){
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(task);
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(Task<T> task,callback f,void *arg){
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(Task<T>(f,arg));
    pthread_mutex_unlock(&m_mutex);
}

//取出一个任务
template <typename T>
Task<T> TaskQueue<T>::takeTask(){
    Task<T> t;
    pthread_mutex_lock(&m_mutex);
    if(!m_taskQ.empty()){
        t=m_taskQ.front();
        m_taskQ.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return t;
}