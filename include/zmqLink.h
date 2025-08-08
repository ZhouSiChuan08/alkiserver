#ifndef ZMQLINK_H
#define ZMQLINK_H
#include <iostream>
#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include "debugTools.h"

/**
 * @brief 使用zmq router套接字创建接收端
 */
class Router 
{
// 成员函数 
public:
    // 构造器
    Router(const std::string& address);
    // 析构器
    ~Router();
    // 接收消息
    /**
     * @brief 接收消息 并把发送者ID、消息内容保存到成员变量中
     * @return 成功返回true，失败返回false
     */
    bool receive();
// 成员变量
private:
    zmq::context_t context;  // 上下文信息
    std::string address;     // 接收端地址
public:
    zmq::socket_t routerSocket;  // 接收端套接字
    // 循环接收消息， 所有消息视作zmq::multipart_t类型
    std::string dealerID;       // 发送者的dealer_id
    std::string msgBody;    // 接收到的问题

};

#endif // ZMQLINK_H