#include <iostream>
#include "zmqLink.h"
#include "taskManager.h"



// 全局变量
int global_argc;
char **global_argv;
int main(int argc, char *argv[])
{
    global_argc = argc;  // 输入参数个数
    global_argv = argv;  // 输入参数数组

    // 设置编码以支持在windows上对中文路径的访问
    std::setlocale(LC_ALL, ".UTF-8");
    // 数据库初始化
    stdCoutInColor(1, StringColor::YELLOW, "🚀🚀🚀 程序首次启动, 初始化数据库, 创建uses表, 添加机主ID = 1.\n");
    if (initDataBase())
    {
        stdCoutInColor(1, StringColor::GREEN, "🥳🥳🥳 数据库初始化成功.\n");
    }
    else
    {
        stdCerrInColor(1, "😭😭😭 数据库初始化失败, 程序已自动退出.\n");
        exit(1);
    }

    // dealer列表
    std::map<std::string, std::string> dealerList;  // <key, dealerID>
    // 注册dealer
    dealerList["UserChat"]               = "dealer:UserChat";
    dealerList["AddRoleProfile"]         = "dealer:AddRoleProfile";
    dealerList["ReEditRoleProfile"]      = "dealer:ReEditRoleProfile";
    dealerList["GetRoleProfiles"]        = "dealer:GetRoleProfiles";
    dealerList["MakeChat"]               = "dealer:MakeChat";
    dealerList["GetChatList"]            = "dealer:GetChatList";
    dealerList["MakeMsgLord"]            = "dealer:MakeMsgLord";
    dealerList["MakeMsgAi"]              = "dealer:MakeMsgAi";
    dealerList["GetChatMsgList"]         = "dealer:GetChatMsgList";
    dealerList["GetLocalModelsSetting"]  = "dealer:GetLocalModelsSetting";
    dealerList["WriteSetting"]           = "dealer:WriteSetting";
    dealerList["DeleteMsg"]              = "dealer:DeleteMsg";
    dealerList["DeleteContact"]          = "dealer:DeleteContact";
    dealerList["TTS"]                    = "dealer:TTS";
    dealerList["TtsVoiceManage"]         = "dealer:TtsVoiceManage";
    dealerList["ASR"]                    = "dealer:ASR";
    // router 接入点
    std::string zmqRouterAddress = "ipc://alkiserver";

    TaskManager taskManager;  // 任务管理器
    // 向任务管理器注册任务处理器及其初始化
    try
    {
        taskManager.Register<DealerUserChat_getAnswerSendAiReply>(dealerList.at("UserChat"), LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::MaxMemoryLength)).get<int>());  // 接收用户输入, 发起提问, 接收答案，向前端发送答案
        taskManager.Register<DealerAddRoleProfile_reciveAndAddRoleProfile>(dealerList.at("AddRoleProfile"));                     // 接收用户输入的角色配置, 并将配置存储到数据库
        taskManager.Register<DealerReEditRoleProfile_reEditRoleProfile>(dealerList.at("ReEditRoleProfile"));                     // 接收用户输入的角色配置, 并更新数据库中的配置
        taskManager.Register<DealeGetRoleProfiles_queryAndSendRoleProfiles>(dealerList.at("GetRoleProfiles"));                   // 查询数据库, 并将角色配置发送给前端
        taskManager.Register<DealerMakeChat_createConversationsOrAddNewConversation>(dealerList.at("MakeChat"));                 // 创建会话表, 或加入新会话
        taskManager.Register<DealerGetChatList_getChatList>(dealerList.at("GetChatList"));                                       // 获取会话列表及相应角色配置
        taskManager.Register<DealerMakeMsg_createMessagesOrAddNewMessage>(dealerList.at("MakeMsgLord"));                         // 创建消息表, 或加入机主新消息
        taskManager.Register<DealerMakeMsg_createMessagesOrAddNewMessage>(dealerList.at("MakeMsgAi"));                           // 创建消息表, 或加入Ai新消息
        taskManager.Register<DealerGetChatMsgList_getChatMsgList>(dealerList.at("GetChatMsgList"));                              // 获取会话消息列表, 针对单个会话ID或多个会话ID
        taskManager.Register<DealerGetLocalModelsSetting_getLocalModelsAndSettings>(dealerList.at("GetLocalModelsSetting"));     // 获取本地模型列表
        taskManager.Register<DealerWriteSetting_writeAndSaveSetting>(dealerList.at("WriteSetting"));                             // 写入并保存配置
        taskManager.Register<DealerDeleteMsg_deleteMessageByConversationId>(dealerList.at("DeleteMsg"));                         // 删除一个会话中的所有聊天记录
        taskManager.Register<DealerDeleteContact_deleteContact>(dealerList.at("DeleteContact"));                                 // 删除一个联系人
        taskManager.Register<DealerTTS_getTTSResult>(dealerList.at("TTS"));                                                      // 获取TTS回复
        taskManager.Register<DealerTtsVoiceManage_voiceManage>(dealerList.at("TtsVoiceManage"));                                 // 管理TTS音色 克隆/查询/删除                     
        taskManager.Register<DealerASR_getASRResult>(dealerList.at("ASR"));                                                      // 获取ASR结果
    }
    catch(const std::out_of_range& e)
    {
        stdCerrInColor(1, "In main(), 注册任务处理器失败, 未知的dealerID 程序已退出 %s\n", e.what());
        return 0;
    }
    catch(const std::exception& e)
    {
        stdCerrInColor(1, "In main(), 注册任务处理器失败, 异常信息: %s\n", e.what());
    }

    // 1 建立zmqRouter 接收前端用户输入
    Router router(zmqRouterAddress);
    while (true)
    {
        stdCoutInColor(1, StringColor::YELLOW, "In main(), 🚗🚗🚗 zmqrouter 等待前端用户输入📦📦📦\n");
        if (router.receive())  // 成功接收到用户输入
        {
            try 
            {
                bool result = taskManager.Process(router.routerSocket, router.dealerID, router.msgBody);
                if (!result) 
                {
                    // 通知前端 任务处理失败
                    sendTaskStatus(router.routerSocket, router.dealerID, TaskStatus::failure);
                    stdCerrInColor(1, "In main(), 任务处理失败, dealerID: %s\n", router.dealerID.c_str());
                }
            } 
            catch (const std::exception& e) 
            {
                stdCerrInColor(1, "In main(), ❌❌❌ 任务处理错误, dealerID: %s, 异常信息: %s\n", router.dealerID.c_str(), e.what());
            }
        }
    }
    return 0;
}

