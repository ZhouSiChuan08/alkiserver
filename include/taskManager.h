#ifndef TASKMANAGER_H
#define TASKMANAGER_H
#include <iostream>
#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include "boostLink.h"
#include "dataBase.h"
#include "config.h"
#include "senseVoice/sense-voice-cxx-api.h"

// 任务处理反馈消息标准化
enum class TaskStatus {failure, success};
// 任务处理反馈消息元数据
struct TaskStatusMeta {
    // 获取任务处理状态名称
    static std::string name(TaskStatus taskStatus) {
        switch(taskStatus) {
            case TaskStatus::failure: return "failure";
            case TaskStatus::success: return "success";
            default: throw std::invalid_argument("Unknown TaskStatus");
        }
    }
};

/**
 * @brief 通知前端任务处理状态
 * @param router 用于发送消息的socket
 * @param dealerID 前端发起请求的dealerID
 * @param status 任务处理状态
 */
void sendTaskStatus(zmq::socket_t& router, std::string& dealerID, TaskStatus status);

// 抽象处理器基类
class TaskHandler 
{
public:
    virtual ~TaskHandler() = default;
    virtual bool handle(zmq::socket_t& router, 
                        std::string& dealerID,
                        std::string& msgBody) = 0;
};

// 工厂类
class TaskManager
{
public:
    // 注册处理器类型
    template<typename T, typename... Args>
    void Register(const std::string& dealerID, Args&&... args)  // args是任务处理器派生类的构造器输入参数 
    {
        taskHandlers[dealerID] = std::make_unique<T>(std::forward<Args>(args)...);
    }
    // 执行处理器（带安全校验）
    bool Process(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) 
    {
        auto it = taskHandlers.find(dealerID);
        if (it == taskHandlers.end()) 
        {
            throw std::runtime_error("In TaskManager::Process, 未定义行为的dealerID: " + dealerID);  // 若成功抛出错误，程序会自动终止
        }
        return it->second->handle(router, dealerID, msgBody);
    }
    private:
    std::map<std::string, std::unique_ptr<TaskHandler>> taskHandlers;
};

// 处理器派生类
/**
 * @brief 处理器派生类：DealerUserChat_getAnswerSendAiReply
 * @note 接收用户输入，发起提问，接收答案，向前端发送答案
 */
class DealerUserChat_getAnswerSendAiReply : public TaskHandler 
{
public:
    DealerUserChat_getAnswerSendAiReply(const int maxMemoryLength) : maxMemoryLength(maxMemoryLength) {}
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
    // 变量成员
    const int maxMemoryLength;  // 最长记忆消息数

};

/**
 * @brief 处理器派生类：DealerAddRoleProfile_reciveAndAddRoleProfile
 * @note 接收用户输入的角色配置，并将配置存储到数据库
 */
class DealerAddRoleProfile_reciveAndAddRoleProfile : public TaskHandler 
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerReEditRoleProfile_reEditRoleProfile
 * @note 接收用户输入的角色配置，并更新数据库中相应的角色配置
 */
class DealerReEditRoleProfile_reEditRoleProfile : public TaskHandler 
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerGetRoleProfile_getRoleProfile
 * @note 访问users表查询所有角色配置, 并向前端发送角色配置
 */
class DealeGetRoleProfiles_queryAndSendRoleProfiles : public TaskHandler 
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerMakeChat_createConversationsOrAddNewConversation
 * @note 创建会话表或向会话表中添加新的单聊会话
 */
class DealerMakeChat_createConversationsOrAddNewConversation : public TaskHandler 
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerGetChatList_getChatList
 * @note 查询会话表获取所有单聊会话列表及对应角色信息
 */
class DealerGetChatList_getChatList : public TaskHandler
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerMakeMsg_createMessagesOrAddNewMessage
 * @note 创建消息表或向消息表中添加新的消息
 */
class DealerMakeMsg_createMessagesOrAddNewMessage : public TaskHandler 
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerGetChatMsgList_geChatMsgList
 * @note 查询消息表获取单聊会话的消息列表 可获取单conversation_id或多个conversation_id的消息列表
 */
class DealerGetChatMsgList_getChatMsgList : public TaskHandler 
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/*
 * @brief 处理器派生类：DealerGetLocalModels_getLocalModels
 * @note 获取本地模型列表
 */
class DealerGetLocalModelsSetting_getLocalModelsAndSettings : public TaskHandler 
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerWriteSetting_writeAndSaveSetting
 * @note 接收前端设置并保存到配置文件
 */
class DealerWriteSetting_writeAndSaveSetting : public TaskHandler
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerDeleteMsg_deleteMessageByConversationId
 * @note 根据conversation_id删除会话内所有消息
 */
class DealerDeleteMsg_deleteMessageByConversationId : public TaskHandler
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerDeleteContact_deleteContact
 *@note 根据user_id删除联系人、会话、会话成员、会话内所有消息
 */
class DealerDeleteContact_deleteContact : public TaskHandler
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerTTS_getTTSResult
 * @note 接收前端TTS请求，调用TTS接口获取TTS结果(音源文件路径)并返回
 */
class DealerTTS_getTTSResult : public TaskHandler
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};

/**
 * @brief 处理器派生类：DealerASR_getASRResult
 * @note 接收前端ASR请求，调用接口获取ASR结果(音源文件路径)并返回
 */
class DealerASR_getASRResult : public TaskHandler
{
public:
    DealerASR_getASRResult();
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
private:
    std::unique_ptr<SenseVoice> senseVoice;
};
#endif // TASKMANAGER_H

/**
 * @brief 处理器派生类: 
 * @note 实现音色管理功能 DealerTtsVoiceManage_voiceManage
 */
class DealerTtsVoiceManage_voiceManage : public TaskHandler
{
public:
    virtual bool handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody) override;
};
