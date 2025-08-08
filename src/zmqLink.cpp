#include "zmqLink.h"

// 构造器
Router::Router(const std::string& address)
{
    context = zmq::context_t(1);  // 1表示使用一个IO线程处理
    routerSocket = zmq::socket_t(context, ZMQ_ROUTER);
    routerSocket.bind(address.c_str());
    // **手动设置 linger，防止 socket 关闭时等待**
    routerSocket.set(zmq::sockopt::linger, 0);  // 在关闭场景 丢弃未发送消息 直接关闭套接字
}

// 析构器
Router::~Router()
{
    // 因为zmq::socket_t的析构函数会自动处理关闭socket，所以不需要手动关闭socket。routerSocket.close();
    // context.close()也是可选的，因为zmq::context_t析构时会自动处理 自己关反而会阻塞
}

// 接收消息
bool Router::receive()
{
    // 清空问题字符串
    msgBody.clear();
    while (true)
    {
        zmq::multipart_t msgQueue;
        stdCoutInColor(1, StringColor::YELLOW, "In Router::receive(), router开始接收消息\n");
        msgQueue.recv(routerSocket);
        int partNum = 1;
        for(auto& part : msgQueue)
        {
            stdCoutInColor(1, StringColor::BLUE, "In Router::receive(), Part %d: %s\n", partNum, part.to_string().c_str());
            partNum++;
        }
        dealerID = msgQueue.front().to_string();               // 获取dealerID
        stdCoutInColor(1, StringColor::BLUE, "In Router::receive(), DealerID: %s\n", dealerID.c_str());
        if (msgQueue.size() >= 3 && msgQueue.back().to_string() == "END")
        {
            for (size_t i = 1; i < msgQueue.size() - 1; i++)  // 首消息是ID忽略 末尾END忽略
            {
                msgBody += msgQueue[i].to_string();           // 获取消息内容
            }
            stdCoutInColor(1, StringColor::GREEN, "In Router::receive(), Router receive over.\n");
            break;
        }
        else
        {
            stdCerrInColor(1, "In Router::receive(), dealerID: %s 发送非标准消息, 尾帧不是END.\n", dealerID.c_str());
            return false;
        }
    }
    return true;
}