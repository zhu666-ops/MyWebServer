#include "conf.h"
#include <stdlib.h>
using namespace std;

Conf::Conf(){
    init();
    readServerConf();
}

//相关初始化
void Conf::init(){
    m_actorModel=false;   //默认是proactor模型，true为reactor模型

    m_resourcesPath="/home/zhu/niukewang/MyWebSerser/root";   //访问资源目录的默认路径

    m_mode=false;   //默认是LT模式，true为ET模式

    m_port=9999;  //默认端口号是9999

    m_threadPoolMinNum=4;  //线程池中默认线程的最小值

    m_threadPoolMaxNum=100;  //线程池中默认线程的最大值

    m_confPath="../conf/server.conf";

    m_hashMap.insert(make_pair("actorModel",ACTORMODEL));
    m_hashMap.insert(make_pair("root",ROOT));
    m_hashMap.insert(make_pair("mode",MODE));
    m_hashMap.insert(make_pair("port",PORT));
    m_hashMap.insert(make_pair("threadPoolMinNum",THREADPOOLMINNUM));
    m_hashMap.insert(make_pair("threadPoolMaxNum",THREADPOOLMAXNUM));

}

//读取server.conf配置文件，获取配置信息
void Conf::readServerConf(){
    fstream f(m_confPath); //创建一个fstream文件流对象
    if(!f.is_open()){
        cout<<"open server.conf error"<<endl;
        exit(-1);
    }
    string line;  //保存读入的每一行
    string strKey="";
    string strValue="";
    int lineNum=1;
    while(getline(f,line))//会自动把\n换行符去掉 
    {
        int flag=subString(strKey,strValue,line);  //取出每一行的字段和对应的值
        if(flag==false){
            errorConf(lineNum);
        }
        if(strKey!=""&&strValue!=""){
            if(!selectMethod(strKey,strValue)){  //选择所需的模式
                errorConf(lineNum);
            }   
        }
        lineNum++;
    }
    f.close();
}

//读取配置文件中的字段，获得对应的字段和值
bool Conf::subString(string &strKey,string &strValue,const string &line){
    int prevIndex=-1;
    int minIndex=-1; 
    int backIndex=-1;
    int flag=1;

    for(int i=0;i<line.size();i++){
        if(line[i]=='='&&flag==1)
            prevIndex=i;
        if(line[i]==';')
            minIndex=i;
        if(line[i]=='#')   //#号表示注释
            backIndex=i;
    }
    if(minIndex>prevIndex&&backIndex>minIndex){
        strKey=line.substr(0,prevIndex);
        strValue=line.substr(prevIndex+1,minIndex-prevIndex-1);
        return true;
    }
    else if(backIndex==-1&&prevIndex<minIndex){
        strKey=line.substr(0,prevIndex);
        strValue=line.substr(prevIndex+1,minIndex-prevIndex-1);
        return true;
    }
    else if(backIndex!=-1&&(backIndex<prevIndex||backIndex<minIndex)){
        return true;
    }
    else if(backIndex!=-1&&prevIndex==-1&&minIndex==-1)
        return true;
    else 
        return false;
}

//行号错误
void Conf::errorConf(int number){
    cout<<"server.conf:line "<<number<<" has error"<<endl;
    exit(-1);
}

//字段和值的选择函数
bool Conf::selectMethod(const string &strKey,const string &strValue){
    CONFMETHOD mode=m_hashMap[strKey];
    switch (mode)
    {
    case ACTORMODEL:
        if(strValue=="y"||strValue=="Y")
            m_actorModel=true;
        else if(strValue=="n"||strValue=="N")
            m_actorModel=false;
        else 
            return false;
        break;
    case ROOT:
        m_resourcesPath=strValue;
        break;
    case MODE:
        if(strValue=="y"||strValue=="Y")
            m_mode=true;
        else if(strValue=="n"||strValue=="N")
            m_mode=false;
        else 
            return false;
        break;
    case PORT:
        m_port=atoi(strValue.c_str());
        break;
    case THREADPOOLMINNUM:
        m_threadPoolMinNum=atoi(strValue.c_str());
        break;
    case THREADPOOLMAXNUM:
        m_threadPoolMaxNum=atoi(strValue.c_str());
        break;
    default:
        return false;
    }
    return true;
}

Conf::~Conf(){
    
}