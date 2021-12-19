#include "threadpool.h"
#include "taskqueue.h"
#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
using namespace std;

//创建线程池并初始化
template <typename T>
ThreadPool<T>::ThreadPool(int min,int max){
    //实例化任务队列
    do{
        m_taskQueue=new TaskQueue<T>;
        if(m_taskQueue==nullptr){
            cout<<"new taskQueue failure..."<<endl;
        }

        m_threadIds=new pthread_t[max];
        if(m_threadIds==nullptr){
            cout<<"new threadIds failure..."<<endl;
            break;
        }
        memset(m_threadIds,0,sizeof(m_threadIds));
        m_minNum=min;
        m_maxNum=max;
        m_shutdown=false;
        m_busyNum=0;
        m_liveNum=min;
        m_exitNum=0;

        if(pthread_mutex_init(&m_mutexPool,NULL)!=0||
            pthread_mutex_init(&m_mutexBusy,NULL)!=0||
            pthread_cond_init(&m_notEmpty,NULL)!=0)
        {
            cout<<"mutex or condition init failure..."<<endl;
            break;
        }
        pthread_create(&m_manager,NULL,manager,this);//这里需要传入静态成员函数，因为非静态实例化的时候，才是地址，而静态成员函数编译的时候就要地址了
                                                     //传入this指针是因为，静态成员函数里面只能访问静态成员变量，不能访问非静态成员变量，所以需要传一个对象进去
        for(int i=0;i<min;i++){
            pthread_create(&m_threadIds[i],NULL,worker,this);
        }
        return ;
    }while(0);
    
    if(m_taskQueue)
        delete m_taskQueue;
    if(m_threadIds)
        delete[] m_threadIds;
    
}
//销毁线程池
template <typename T>
ThreadPool<T>::~ThreadPool(){
    //关闭线程池
    m_shutdown=true;
    //阻塞回收管理者线程
    pthread_join(m_manager,NULL);
    //回收工作线程
    for(int i=0;i<m_liveNum;i++){
        pthread_cond_signal(&m_notEmpty);
    }
    //释放堆内存
    if(m_taskQueue)
        delete m_taskQueue;
    if(m_threadIds)
        delete[] m_threadIds;
    pthread_mutex_destroy(&m_mutexPool);
    pthread_mutex_destroy(&m_mutexBusy);
    pthread_cond_destroy(&m_notEmpty);

}
//给线程池添加任务
template <typename T>
void ThreadPool<T>::addTask(Task<T> task){
    if(m_shutdown)
        return ;
    //添加任务
    m_taskQueue->addTask(task);  //这个在任务队列里面有互斥锁了，这里就不需要加锁了
    pthread_cond_signal(&m_notEmpty);
}

//获取忙的线程个数
template <typename T>
int ThreadPool<T>::getBusyNum(){
    pthread_mutex_lock(&m_mutexBusy);
    int busyNum=m_busyNum;
    pthread_mutex_unlock(&m_mutexBusy);
    return busyNum;
}

//获取活着的线程个数
template <typename T>
int ThreadPool<T>::getAliveNum(){
    pthread_mutex_lock(&m_mutexPool);
    int aliveNum=m_liveNum;
    pthread_mutex_unlock(&m_mutexPool);
    return aliveNum;
}

//工作线程任务函数
template <typename T>
void *ThreadPool<T>::worker(void *arg){
    ThreadPool<T> * pool=static_cast<ThreadPool<T>*>(arg);
    while(true){
        pthread_mutex_lock(&pool->m_mutexPool);
        //当前任务队列是否为空
        while(pool->m_taskQueue->taskNumber()==0&&!pool->m_shutdown){
            //阻塞工作的线程
            pthread_cond_wait(&pool->m_notEmpty,&pool->m_mutexPool);

            //判断是否需要销毁线程
            if(pool->m_exitNum>0){
                pool->m_exitNum--;
                if(pool->m_liveNum>pool->m_minNum){
                    pool->m_liveNum--;
                    pthread_mutex_unlock(&pool->m_mutexPool);
                    pool->threadExit(pool);
                }
            }
        }
        //判断线程池是否关闭
        if(pool->m_shutdown){
            pthread_mutex_unlock(&pool->m_mutexPool);
            pool->threadExit(pool);
        }

        //从任务队列中取出任务
        Task<T> task=pool->m_taskQueue->takeTask();

        pthread_mutex_unlock(&pool->m_mutexPool);
        

        pthread_mutex_lock(&pool->m_mutexBusy);
        pool->m_busyNum++;   //忙线程数++
        pthread_mutex_unlock(&pool->m_mutexBusy);
        
        //处理任务
        //reactor
        if(task.arg->m_actorModel){
            if(task.arg->m_reactorState){
                if(task.arg->read()){
                    task.arg->m_reactorEnd=true;  
                    task.arg->process();
                }else{
                    task.arg->m_reactorEnd=true;
                    task.arg->m_reactorErrorStatus=false;
                }
            }else{
                if(task.arg->write()){  
                    task.arg->m_reactorEnd=true;  
                }else{      //发生写错误
                    task.arg->m_reactorEnd=true;
                    task.arg->m_reactorErrorStatus=false;
                }
            }
        }else{   //proactor  
            task.arg->process();  
        }
   

        //任务处理完
        cout<<"thread:"<<pthread_self()<<"end...."<<endl;

        pthread_mutex_lock(&pool->m_mutexBusy);
        pool->m_busyNum--;   //忙线程数--
        pthread_mutex_unlock(&pool->m_mutexBusy);

    }
    return NULL;
}

//管理者线程工作函数
template <typename T>
void* ThreadPool<T>::manager(void *arg){
    ThreadPool<T> *pool=static_cast<ThreadPool<T>*>(arg);
    while(!pool->m_shutdown){
        //每隔3s检测一次
        sleep(3);

        //取出线程池中任务的数量和当前线程的数量
        pthread_mutex_lock(&pool->m_mutexPool);
        int queueSize=pool->m_taskQueue->taskNumber();
        int liveNum=pool->m_liveNum;
        pthread_mutex_unlock(&pool->m_mutexPool);

        //取出忙线程的数量
        pthread_mutex_lock(&pool->m_mutexBusy);
        int busyNum=pool->m_busyNum;
        pthread_mutex_unlock(&pool->m_mutexBusy);

        //添加线程
        if(queueSize>liveNum&&liveNum<pool->m_maxNum){
            pthread_mutex_lock(&pool->m_mutexPool);
            int counter=0;
            for(int i=0;i<pool->m_maxNum&&counter<NUMBER;i++){
                if(pool->m_threadIds[i]==0){
                    pthread_create(&pool->m_threadIds[i],NULL,worker,pool);
                    counter++;
                    pool->m_liveNum++;
                }
            }
        }
        pthread_mutex_unlock(&pool->m_mutexPool);

        //销毁线程
        if(queueSize*2<liveNum&&liveNum>pool->m_minNum){
            pthread_mutex_lock(&pool->m_mutexPool);
            pool->m_exitNum=NUMBER;
            pthread_mutex_unlock(&pool->m_mutexPool);
            //让工作线程自杀
            for(int i=0;i<NUMBER;i++){
                pthread_cond_signal(&pool->m_notEmpty);
            }
        }
    }
    return NULL;
}

//单个线程退出函数
template <typename T>
void ThreadPool<T>::threadExit(ThreadPool<T> *pool){
    pthread_t tid=pthread_self();
    for(int i=0;i<pool->m_maxNum;i++){
        if(pool->m_threadIds[i]==tid){
            m_threadIds[i]=0;
            cout<<"threadExit() called,"<<tid<<"exiting..."<<endl;
            break;
        }
    }
    pthread_exit(NULL);
}