## WebServer
我的 WebServer 项目是一个基于 C++ 的高性能 Web 服务器，采用主从分离设计，结合 epoll 和线程池技术，极大地提升了服务器的吞吐能力。，一个主reactor和四个子reactor,主 Reactor 主要负责监听客户端连接请求 (accept)，并将新连接分发给子 Reactor 处理。经过webbenchh压力测试可以实现6000+的 QPS。

## 功能
* 利用 IO 复用技术中的 epoll 与线程池，构建主从 Reactor 架构，高效处理大规模并发请求，显著提升服务器吞吐量。
* 使用正则表达式和状态机技术，实现对 HTTP 请求报文的高效解析，支持静态资源请求处理（如 HTML、CSS、JavaScript 文件的传输）。
* 提供灵活的配置文件功能，支持动态调整服务器运行参数，包括监听端口、线程池大小、静态资源路径等，提高服务器的可维护性。
* 利用RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销，同时实现了用户注册登录功能。
* 利用标准库容器封装char，实现自动增长的缓冲区；

## 环境要求
* Linux
* C++14
* Cmake
* Mysql
* Json

## 项目启动
// 建立yourdb库
```
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
```
```
make
./bin/server
```

## 压力测试

## TODO

## 致谢
Linux高性能服务器编程，游双著.
@markparticle MARK WebServer[!https://github.com/markparticle/WebServer]