#pragma once 
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;


class Conf{
public:
    Conf();
    ~Conf();

    
    enum CONFMETHOD {ACTORMODEL,ROOT,MODE,PORT,THREADPOOLMINNUM,THREADPOOLMAXNUM};

    //相关初始化
    void init();

    //读取server.conf配置文件，获取配置信息
    void readServerConf();

    //读取配置文件中的字段，获得对应的字段和值
    bool subString(string &strKey,string &strValue,const string &line);

    //行号错误
    void errorConf(int number);

    //字段和值的选择函数
    bool selectMethod(const string &strKey,const string &strValue);

    bool m_actorModel;   //默认是proactor模型，true为reactor模型

    string m_resourcesPath;   //访问资源目录的默认路径

    bool m_mode;   //默认是LT模式，true为ET模式

    int m_port;  //默认端口号是9999

    int m_threadPoolMinNum;  //线程池中默认线程的最小值

    int m_threadPoolMaxNum;  //线程池中默认线程的最大值

private:
    string m_confPath;
    unordered_map<string,CONFMETHOD> m_hashMap;
};