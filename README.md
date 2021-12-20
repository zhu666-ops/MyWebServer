MyWebServer
===============

Linux平台下C++轻量级Web服务器

* 使用 **线程池 + 非阻塞socket + epoll(ET和LT均实现) + 事件处理(Reactor和模拟Proactor均实现)** 的并发模型
* 线程池实现**动态扩张大小**
* 使用**状态机**解析HTTP请求报文，支持解析**GET**请求
* 使用**定时器**处理非活动连接
* 使用Webbench压力测试实现**上万并发连接**

编译与运行
------------

* 服务器测试环境

  * Ubuntu版本18.04

* 浏览器测试环境

  * Windows、Linux均可
  * Chrome
  * FireFox
  * 其他浏览器暂无测试

* 在build目录

  ```C++
  make
  ```

* 启动server

  ```C++
  ./server
  ```

* 默认ip

  ```C++
  ip:8888
  ```

修改配置文件运行
------

可以根据个人情况在**conf**目录下的server.conf中修改服务器配置信息，server_default.conf表示备份配置文件

* **port**，自定义端口号
  * 默认8888
* **root**，浏览器访问的资源目录路径
  * 默认在本目录中的root目录下
* **mode**，connectFd的LT/ET
  * n,LT模式
  * y,ET模式
* **threadPoolMinNum**，线程池的线程的最小值
  * 默认是8
* **threadPoolMaxNum**，线程池的线程的最大值
  * 默认是1000
* **actorModel**，选择反应堆模式，默认Proactor
  * n，Proactor模式
  * y，Reactor模式
* **=**,表示对应字段赋值
* **;**,表示该行配置的结束
* **#**,表示注释的内容




