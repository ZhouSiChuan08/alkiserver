#include "cmdLink.h"

// 构造器
CmdRouter::CmdRouter(const std::string& address)
{
    context = zmq::context_t(2);  // 1表示使用一个IO线程处理
    routerSocket = zmq::socket_t(context, ZMQ_ROUTER);
    routerSocket.bind(address.c_str());
    // **手动设置 linger，防止 socket 关闭时等待**
    routerSocket.set(zmq::sockopt::linger, 0);  // 在关闭场景 丢弃未发送消息 直接关闭套接字
}

// 析构器
CmdRouter::~CmdRouter()
{
    // 因为zmq::socket_t的析构函数会自动处理关闭socket，所以不需要手动关闭socket。routerSocket.close();
    // context.close()也是可选的，因为zmq::context_t析构时会自动处理 别手动关闭, 会出现故障阻塞
}

// 接收消息
bool CmdRouter::receive()
{
    bool isSuccess = false;
    zmq::multipart_t msgQueue;
    if (msgQueue.recv(routerSocket, ZMQ_DONTWAIT))
    {
        dealerID = msgQueue.front().to_string();  // 取出DealerID
        isSuccess = true;
    }
    return isSuccess;
}