#ifndef CONSTS_H
#define CONSTS_H
#include <string>
#include <stdexcept>
#include <zmq.hpp>
#include <zmq_addon.hpp>

// 非主函数事件任务类的dealer, 用以在任务执行中途接收命令消息, 并执行相应的命令
enum class DealerCMD {
    CMD_STOP_CHAT  // 停止聊天
};
// 配置dealer的名称元数据
struct DealerCMDMeta {
    // 获取命令dealer的名称
    static std::string name(DealerCMD cmd) {
        switch (cmd) {
            case DealerCMD::CMD_STOP_CHAT: return "dealer:CMD_STOP_CHAT";
            default: throw std::invalid_argument("Unknown DealerCMD");
        }
    }
    static inline const std::string CmdAddress = "ipc://alkiserver_cmd";
};

/**
 * @brief 使用zmq router套接字创建接收端
 */
class CmdRouter 
{
// 成员函数 
public:
    // 构造器
    CmdRouter(const std::string& address);
    // 析构器
    ~CmdRouter();
    // 接收消息
    /**
     * @brief 非阻塞接收消息 并把发送者ID保存到成员变量中
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
};

#endif  // CONSTS_H