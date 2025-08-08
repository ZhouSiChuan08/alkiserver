#include "taskManager.h"

void sendTaskStatus(zmq::socket_t& router, std::string& dealerID, TaskStatus status)
{
    // 通知前端 当前任务处理状态
    zmq::message_t dealerId(dealerID);
    zmq::message_t successMsg( TaskStatusMeta::name(status) + dealerID);
    router.send(dealerId, zmq::send_flags::sndmore);
    router.send(successMsg, zmq::send_flags::none); 
}

// 各任务处理器的实现
bool DealerUserChat_getAnswerSendAiReply::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    // 解析用户请求
    json userIdAndQuestion = json::parse(msgBody);
    int user_id = userIdAndQuestion["userId"];
    std::string question = userIdAndQuestion["userQuestion"];
    
    // 获取角色信息
    QueryResult userInfo = getUserInfo(user_id);
    if (userInfo.rows.empty())
    {
        stdCerrInColor(1, "In DealerUserChat_getAnswerSendAiReply::handle, 未找到用户信息\n");
        isSuccess = false;
    }
    else
    {
        std::string user_name = std::any_cast<std::string>(userInfo.rows[0][0]);
        std::string gender = std::any_cast<std::string>(userInfo.rows[0][1]);
        std::string area   = std::any_cast<std::string>(userInfo.rows[0][5]);
        std::string presupposition = std::any_cast<std::string>(userInfo.rows[0][7]);
        // 设置systemPrompt
        std::string systemPrompt = "你的名字是"+user_name+",性别是"+gender+",所属地区是"+area+",你的人物预设是"+presupposition+"。"+
            "你的所有回答都要符合这个预设, 此外还要符合所设性别、地区下的人物风格、知识背景。除开用户要求翻译的场景, 都要以用户提问的语言种类进行回复。";
        // 如果开启情绪提示词, 则加入情绪提示词
        if (LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::UseMotion)).get<bool>())
        {
            std::string otherPrompt = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::OtherPrompt)).get<std::string>();
            systemPrompt += otherPrompt;
        }

        // 新建 消息列表、系统消息json体
        json messages = json::array();
        json systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = systemPrompt;
        messages.push_back(systemMsg);
        // 加载历史消息
        std::vector<std::string> historyMsgs;
        QueryResult historyMsgQueryResult = getSingleChatMsgsByUserId(user_id, LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::MaxMemoryLength)).get<int>() * 2);  // 对话记忆轮数*2=历史消息条数
        if (historyMsgQueryResult.rows.empty())
        {
            // 历史消息为空
        }
        else
        {
            // 原排序为新->旧, 这里需要反转来适应历史消息的顺序
            std::reverse(historyMsgQueryResult.rows.begin(), historyMsgQueryResult.rows.end());
            for (const auto& row : historyMsgQueryResult.rows)
            {
                int sender_id       = std::any_cast<int>(row[0]);
                std::string content = std::any_cast<std::string>(row[1]);
                std::string created_at = std::any_cast<std::string>(row[3]);
                if (sender_id == TableMeta::LORD_ID)
                {
                    json historyMsg;
                    historyMsg["role"] = "user";
                    historyMsg["content"] = content;
                    messages.push_back(historyMsg);
                }
                else
                {
                    json historyMsg;
                    historyMsg["role"] = "assistant";
                    historyMsg["content"] = content;
                    messages.push_back(historyMsg);
                }
            }
        }
        // 最后添加当前问题
        json currentMsg;
        currentMsg["role"] = "user";
        currentMsg["content"] = question;
        messages.push_back(currentMsg);
        
        // 调用 AI 模型接口
        // 目标服务器地址和端口
        std::string const host   = "localhost";
        std::string const port   = "11434";
        std::string const target = "/api/chat";  // generate 搭配 request_json["prompt"] = msgBody;

        // 请求体
        json request_json;
        request_json["model"] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::ModelSelected));  // 选择的模型
        request_json["messages"] = messages;
        request_json["stream"] = true;
        // 2 发起提问
        HttpQuestioning httpQuestioning(host, port, target, request_json);                       // 初始化
        // 3 接收答案并发送至前端
        bool success = httpQuestioning.receiveAndSend(httpQuestioning.ask(), router, dealerID, ResponseType::chat);  // 提问
        if (success)
        {
            std::string ttsText;  // 语音文本
            for (const auto& response : httpQuestioning.responses)
            {
                ttsText += response.response;
            }
        }
        
        if (success)
        {
            httpQuestioning.socketShutdown();  // 关闭套接字
        }
        else
        {
            httpQuestioning.socketShutdown();  // 关闭套接字
            isSuccess = false;
        }
    }
    return isSuccess;
}

bool DealerAddRoleProfile_reciveAndAddRoleProfile::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    stdCoutInColor(1, StringColor::BLUE, "In DealerAddRoleProfile_reciveAndAddRoleProfile::handle, 接收到dealerID: %s, msgBody: %s\n", dealerID.c_str(), msgBody.c_str());
    // 创建一个数据库对象
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 在数据库文件内创建一个表格 只有std::vector会按照我插入时的顺序排列
    if( dataBase.createTable(Table::users, std::vector<std::pair<std::string, std::string>>{
        {TableMeta::column(UsersColumn::user_id), "INTEGER PRIMARY KEY"}, 
        {TableMeta::column(UsersColumn::user_name), "TEXT NOT NULL"}, 
        {TableMeta::column(UsersColumn::gender), "TEXT NOT NULL"}, 
        {TableMeta::column(UsersColumn::avatar_url), "TEXT"},
        {TableMeta::column(UsersColumn::post_url), "TEXT"},
        {TableMeta::column(UsersColumn::created_at), "DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP"},
        {TableMeta::column(UsersColumn::area), "TEXT NOT NULL"},
        {TableMeta::column(UsersColumn::flag_url), "TEXT"},
        {TableMeta::column(UsersColumn::presupposition), "TEXT NOT NULL"},
        {TableMeta::column(UsersColumn::is_deleted), "INTEGER DEFAULT 0"},
        {TableMeta::column(UsersColumn::voice_engine), "TEXT NOT NULL"}
    }))
    {
        // 解析用户输入的人物配置, 格式为JSON字符串
        json user_config = json::parse(msgBody);
        // 插入到数据库
        if( dataBase.insertData(Table::users, std::vector<SQLDataValuePair> {
            {TableMeta::column(UsersColumn::user_name), static_cast<std::string>(user_config["userName"])},
            {TableMeta::column(UsersColumn::gender), static_cast<std::string>(user_config["gender"])},
            {TableMeta::column(UsersColumn::avatar_url), static_cast<std::string>(user_config["avatarUrl"])},
            {TableMeta::column(UsersColumn::post_url), static_cast<std::string>(user_config["postUrl"])},
            {TableMeta::column(UsersColumn::area), static_cast<std::string>(user_config["area"])},
            {TableMeta::column(UsersColumn::flag_url), static_cast<std::string>(user_config["flagUrl"])},
            {TableMeta::column(UsersColumn::presupposition), static_cast<std::string>(user_config["presupposition"])},
            {TableMeta::column(UsersColumn::voice_engine), static_cast<std::string>(user_config["voiceEngine"])}
        }))
        {
            // 通知前端 任务处理成功
            sendTaskStatus(router, dealerID, TaskStatus::success);
            isSuccess = true;
        }
        else
        {
            isSuccess = false;
        }
    }
    else
    {
        isSuccess = false;
    }
    return isSuccess;
}

bool DealerReEditRoleProfile_reEditRoleProfile::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    stdCoutInColor(1, StringColor::BLUE, "In DealerAddRoleProfile_reciveAndAddRoleProfile::handle, 接收到dealerID: %s, msgBody: %s\n", dealerID.c_str(), msgBody.c_str());
    // 创建一个数据库对象
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 解析用户输入的人物配置, 格式为JSON字符串
    json user_config = json::parse(msgBody);
    int user_id = user_config["userId"].get<int>();
    // 插入到数据库
    if( dataBase.updateData(Table::users, std::vector<SQLDataValuePair> {
        {TableMeta::column(UsersColumn::user_name), static_cast<std::string>(user_config["userName"])},
        {TableMeta::column(UsersColumn::gender), static_cast<std::string>(user_config["gender"])},
        {TableMeta::column(UsersColumn::avatar_url), static_cast<std::string>(user_config["avatarUrl"])},
        {TableMeta::column(UsersColumn::post_url), static_cast<std::string>(user_config["postUrl"])},
        {TableMeta::column(UsersColumn::area), static_cast<std::string>(user_config["area"])},
        {TableMeta::column(UsersColumn::flag_url), static_cast<std::string>(user_config["flagUrl"])},
        {TableMeta::column(UsersColumn::presupposition), static_cast<std::string>(user_config["presupposition"])},
        {TableMeta::column(UsersColumn::voice_engine), static_cast<std::string>(user_config["voiceEngine"])}
    }, 
        std::string(" WHERE " + TableMeta::column(UsersColumn::user_id) + " = " + std::to_string(user_id))
    ))
    {
        // 通知前端 任务处理成功
        sendTaskStatus(router, dealerID, TaskStatus::success);
        isSuccess = true;
    }
    else
    {
        isSuccess = false;
    }
    return isSuccess;
}

bool DealeGetRoleProfiles_queryAndSendRoleProfiles::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    QueryResult queryResult = dataBase.getTableData(Table::users, std::vector<SQLDataType>{
        SQLDataType::INTEGER, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::INTEGER, SQLDataType::TEXT
    });

    // 创建 JSON 数组
    json roleProfilesArray = json::array();
    // 创建包含 roleProfiles 键的 JSON 对象
    json roleProfiles;
    if (queryResult.rows.empty())
    {
        stdCoutInColor(1, StringColor::YELLOW, "In DealeGetRoleProfiles_queryAndSendRoleProfiles::handle, 查询结果为空\n");
        isSuccess = false;
    }
    else
    {
        bool first = true;
        for (const auto& row : queryResult.rows)
        {
            if (first) {
                first = false;
                continue;  // 跳过第一行 因为第一行是机主信息
            }
            json roleProfile;
            roleProfile["user_id"]        = std::any_cast<int>(row[0]);
            roleProfile["user_name"]      = std::any_cast<std::string>(row[1]);
            roleProfile["gender"]         = std::any_cast<std::string>(row[2]);
            roleProfile["avatar_url"]     = std::any_cast<std::string>(row[3]);
            roleProfile["post_url"]       = std::any_cast<std::string>(row[4]);
            roleProfile["created_at"]     = std::any_cast<std::string>(row[5]);
            roleProfile["area"]           = std::any_cast<std::string>(row[6]);
            roleProfile["flag_url"]       = std::any_cast<std::string>(row[7]);
            roleProfile["presupposition"] = std::any_cast<std::string>(row[8]);
            roleProfile["is_deleted"]     = std::any_cast<int>(row[9]);
            roleProfile["voice_engine"]   = std::any_cast<std::string>(row[10]);
            roleProfilesArray.push_back(roleProfile);
        }
        roleProfiles["roleProfiles"] = roleProfilesArray;
        // 将 JSON 对象转换为字符串
        std::string roleProfilesStr = roleProfiles.dump();
        // 发送至前端
        zmq::message_t dealerId(dealerID);
        zmq::message_t roleProfilesMsg(roleProfilesStr);
        router.send(dealerId, zmq::send_flags::sndmore);
        router.send(roleProfilesMsg, zmq::send_flags::none);
    }
    return isSuccess;
}

bool DealerMakeChat_createConversationsOrAddNewConversation::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    stdCoutInColor(1, StringColor::BLUE, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 接收到dealerID: %s, msgBody: %s\n", dealerID.c_str(), msgBody.c_str());
    json chatInfoJsonObj = json::parse(msgBody);                                             // 解析前端传来的会话信息
    int user_id = chatInfoJsonObj["userId"];                                                 // 获取用户ID
    std::string user_name = chatInfoJsonObj["userName"];                                     // 获取用户名
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));  // 打开数据库, 默认开启外键约束
    
    // 新建或打开会话表 && 会话成员表
    if (dataBase.createTable(Table::conversations, std::vector<std::pair<std::string, std::string>>{
        {TableMeta::column(ConversationsColumn::conversation_id), "INTEGER PRIMARY KEY"},
        {TableMeta::column(ConversationsColumn::type), "TEXT NOT NULL CHECK(type IN ('single', 'group'))"},
        {TableMeta::column(ConversationsColumn::name), "TEXT"},
        {TableMeta::column(ConversationsColumn::updated_at), "DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP"}
    }) &&
        dataBase.createTable(Table::conversation_members, std::vector<std::pair<std::string, std::string>>{
        {TableMeta::column(ConversationMembersColumn::conversation_id), "INTEGER"},
        {TableMeta::column(ConversationMembersColumn::user_id), "INTEGER"},
        {"PRIMARY KEY ", "("+TableMeta::column(ConversationMembersColumn::conversation_id)+", "+TableMeta::column(ConversationMembersColumn::user_id)+")"},
        {"FOREIGN KEY ", "("+TableMeta::column(ConversationMembersColumn::conversation_id)+")"+" REFERENCES "+TableMeta::name(Table::conversations)+"("+TableMeta::column(ConversationsColumn::conversation_id)+")" + " ON DELETE CASCADE"},
        {"FOREIGN KEY ", "("+TableMeta::column(ConversationMembersColumn::user_id)+")"+" REFERENCES "+TableMeta::name(Table::users)+"("+TableMeta::column(UsersColumn::user_id)+")" + " ON DELETE CASCADE"}
    }))
    {
        // 拿到user_id, 在会话成员表中查询包含这个user_id的所有会话
        QueryResult queryResult = dataBase.getTableData(Table::conversation_members, std::string(TableMeta::column(ConversationMembersColumn::conversation_id)),
            std::vector<SQLDataType>{SQLDataType::INTEGER}, std::string("WHERE "+TableMeta::column(ConversationMembersColumn::user_id)+" = "+std::to_string(user_id))
        );
        if (queryResult.rows.empty())  // 会话成员表中没有用户的记录 新建会话、会话成员
        {
            stdCoutInColor(1, StringColor::YELLOW, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 会话成员表中没有用户 %s 的记录, 开始新建会话 \n", user_name.c_str());
            // 新建单聊会话到会话表
            if (dataBase.insertData(Table::conversations, std::vector<SQLDataValuePair>{
                {TableMeta::column(ConversationsColumn::type), std::string("single")},
                {TableMeta::column(ConversationsColumn::name), std::string(user_name)}
            }))
            {
                long long conversation_id = dataBase.getLastInsertId();
                stdCoutInColor(1, StringColor::GREEN, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 新建单聊会话成功 conversation_id: %lld name: %s\n", conversation_id, user_name.c_str());

                // 新建会话成员到会话成员表 Ai+机主
                if (dataBase.insertData(Table::conversation_members, std::vector<SQLDataValuePair>{
                    {TableMeta::column(ConversationMembersColumn::conversation_id), static_cast<int>(conversation_id)},
                    {TableMeta::column(ConversationMembersColumn::user_id), static_cast<int>(user_id)}}) &&
                    dataBase.insertData(Table::conversation_members, std::vector<SQLDataValuePair>{
                    {TableMeta::column(ConversationMembersColumn::conversation_id), static_cast<int>(conversation_id)},
                    {TableMeta::column(ConversationMembersColumn::user_id), static_cast<int>(TableMeta::LORD_ID)}})
                )
                {
                    stdCoutInColor(1, StringColor::GREEN, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 新建会话成员成功 conversation_id: %lld user_id: %d\n", conversation_id, user_id);
                    // 通知前端 新建会话成功
                    sendTaskStatus(router, dealerID, TaskStatus::success);
                    isSuccess = true;
                }
            }
            else
            {
                stdCoutInColor(1, StringColor::RED, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 新建单聊会话失败\n");
                isSuccess = false;
            }
        }
        else  // 有会话记录, 则判断是否存在"single"类型的conversation
        {
            std::vector<int> conversation_ids;
            // 获取所有包含该user_id的conversation_id
            for (const auto& row : queryResult.rows)
            {
                conversation_ids.push_back(std::any_cast<int>(row[0]));
                stdCoutInColor(1, StringColor::YELLOW, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 会话成员表中存在用户 %s 的会话 conversation_id: %d\n", user_name.c_str() ,std::any_cast<int>(row[0]));
            }
            // 在会话表中遍历conversation_ids, 查看是否存在"single"类型的conversation
            bool hasSingleConversation = false;
            for (const auto& conversation_id : conversation_ids)
            {
                QueryResult _queryResult = dataBase.getTableData(Table::conversations, std::string(TableMeta::column(ConversationsColumn::type)),
                    std::vector<SQLDataType>{SQLDataType::TEXT}, std::string("WHERE "+TableMeta::column(ConversationsColumn::conversation_id)+" = "+std::to_string(conversation_id))
                );
                if (_queryResult.rows.empty())
                {}
                else
                {
                    for (const auto& row : _queryResult.rows)
                    {
                        if (std::any_cast<std::string>(row[0]) == "single")
                        {
                            hasSingleConversation = true;
                            stdCoutInColor(1, StringColor::YELLOW, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 用户 %s 存在single类型的conversation\n", user_name.c_str());
                            break;
                        }
                    }
                }
                if(hasSingleConversation) break;
            }
            // 如果不存在"single"类型的conversation, 则新建一个"single"类型的conversation
            if (!hasSingleConversation)
            {
                // 新建单聊会话到会话表
                if (dataBase.insertData(Table::conversations, std::vector<SQLDataValuePair>{
                    {TableMeta::column(ConversationsColumn::type), std::string("single")},
                    {TableMeta::column(ConversationsColumn::name), std::string(user_name)}
                }))
                {
                    long long conversation_id = dataBase.getLastInsertId();
                    stdCoutInColor(1, StringColor::GREEN, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 新建单聊会话成功 conversation_id: %lld name: %s\n", conversation_id, user_name.c_str());

                    // 新建会话成员到会话成员表 Ai+机主
                    if (dataBase.insertData(Table::conversation_members, std::vector<SQLDataValuePair>{
                        {TableMeta::column(ConversationMembersColumn::conversation_id), static_cast<int>(conversation_id)},
                        {TableMeta::column(ConversationMembersColumn::user_id), static_cast<int>(user_id)}}) &&
                        dataBase.insertData(Table::conversation_members, std::vector<SQLDataValuePair>{
                        {TableMeta::column(ConversationMembersColumn::conversation_id), static_cast<int>(conversation_id)},
                        {TableMeta::column(ConversationMembersColumn::user_id), static_cast<int>(TableMeta::LORD_ID)}})
                    )
                    {
                        stdCoutInColor(1, StringColor::GREEN, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 新建会话成员成功 conversation_id: %lld user_id: %d\n", conversation_id, user_id);
                        // 通知前端 新建会话成功
                        sendTaskStatus(router, dealerID, TaskStatus::success);
                        isSuccess = true;
                    }
                    else
                    {
                        stdCerrInColor(1, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 新建会话成员失败\n");
                        isSuccess = false;
                    }
                }
                else
                {
                    stdCerrInColor(1, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 新建单聊会话失败\n");
                    isSuccess = false;
                }
            }
            else
            {
                // 存在"single"类型的conversation, 什么都不做
                stdCoutInColor(1, StringColor::YELLOW, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 用户 %s 存在single类型的conversation 无需新建会话\n", user_name.c_str());
                // 通知前端 新建会话成功
                sendTaskStatus(router, dealerID, TaskStatus::success);
                isSuccess = true;
            }
        }
    }
    else
    {
        stdCerrInColor(1, "In DealerMakeChat_createConversationsOrAddNewConversation::handle, 新建或打开会话表 && 会话成员表失败\n");
        isSuccess = false;
    }
    return isSuccess;
}

bool DealerGetChatList_getChatList::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 先查会话表, 获取所有单聊会话的conversation_id、 updated_at
    QueryResult queryResult = dataBase.getTableData(Table::conversations, addCommaSpaceInColumnNames(std::vector<std::string>{
        TableMeta::column(ConversationsColumn::conversation_id), TableMeta::column(ConversationsColumn::updated_at)}), 
        std::vector<SQLDataType>{SQLDataType::INTEGER, SQLDataType::TEXT}, 
        std::string("WHERE "+ TableMeta::column(ConversationsColumn::type) + " = " + std::string("'single'"))
    );
    if (queryResult.rows.empty())
    {
        stdCerrInColor(1, "In DealerGetChatList_getChatList::handle, 会话表中没有单聊会话\n");
        isSuccess = false;
    }
    else
    {
        std::vector<int> conversation_ids;
        std::vector<int> user_ids;
        std::vector<std::string> updated_ats;
        for (const auto& row : queryResult.rows)
        {
            conversation_ids.push_back(std::any_cast<int>(row[0]));
            updated_ats.push_back(std::any_cast<std::string>(row[1]));
        }
        for (size_t i = 0; i < conversation_ids.size(); i++)
        {
            QueryResult _queryResult = dataBase.getTableData(Table::conversation_members, std::string(TableMeta::column(ConversationMembersColumn::user_id)),
                std::vector<SQLDataType>{SQLDataType::INTEGER}, std::string("WHERE "+ TableMeta::column(ConversationMembersColumn::conversation_id) + " = " + std::to_string(conversation_ids[i])));
            if (_queryResult.rows.empty())
            {
                stdCerrInColor(1, "In DealerGetChatList_getChatList::handle, 会话成员表中没有conversation_id: %d 的单聊会话, 会话逻辑错误, 退出函数\n", conversation_ids[i]);
                isSuccess = false;
            }
            else
            {
                // 同一会话ID下的user_id数量为2, 需筛除机主
                for (const auto& row : _queryResult.rows)
                {
                    if (std::any_cast<int>(row[0]) == TableMeta::LORD_ID) continue;
                    user_ids.push_back(std::any_cast<int>(row[0]));
                }
            }
        }
        // 获取到所有会话的id、user_id、updated_at, 开始组装json对象
        json chatListArray = json::array();
        json chatListArrayObj;
        for (size_t i = 0; i < conversation_ids.size(); i++)
        {   
            json chatListObj;
            chatListObj["conversation_id"] = conversation_ids[i];
            chatListObj["user_id"]         = user_ids[i];
            chatListObj["updated_at"]      = updated_ats[i];
            chatListArray.push_back(chatListObj);
        }
        chatListArrayObj["chatList"] = chatListArray;
        // 将json对象转换为字符串
        std::string chatListStr = chatListArrayObj.dump();
        // 发送至前端
        zmq::message_t dealerId(dealerID);
        zmq::message_t chatListMsg(chatListStr);
        router.send(dealerId, zmq::send_flags::sndmore);
        router.send(chatListMsg, zmq::send_flags::none);
    }
    return isSuccess;
}

bool DealerMakeMsg_createMessagesOrAddNewMessage::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = false;
    json msgInfoJsonObj = json::parse(msgBody);
    // 解析会话ID、发送者ID、消息内容、消息类型
    int conversation_id = msgInfoJsonObj["conversationId"];
    int sender_id = msgInfoJsonObj["senderId"];
    std::string content = msgInfoJsonObj["content"];
    std::string type = msgInfoJsonObj["messageType"];
    // 打开数据库连接
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 新建或打开消息表
    if(dataBase.createTable(Table::messages, std::vector<std::pair<std::string, std::string>>{
        {TableMeta::column(MessagesColumn::message_id), "INTEGER PRIMARY KEY"},
        {TableMeta::column(MessagesColumn::conversation_id), "INTEGER"},
        {TableMeta::column(MessagesColumn::sender_id), "INTEGER"},
        {TableMeta::column(MessagesColumn::content), "TEXT NOT NULL"},
        {TableMeta::column(MessagesColumn::message_type), "TEXT NOT NULL DEFAULT 'text' CHECK(message_type IN ('text', 'image', 'audio', 'video'))"},
        {TableMeta::column(MessagesColumn::voice_path), "TEXT"},
        {TableMeta::column(MessagesColumn::created_at), "DATETIME DEFAULT CURRENT_TIMESTAMP"},
        {"FOREIGN KEY ", "("+TableMeta::column(MessagesColumn::conversation_id)+")"+" REFERENCES "+TableMeta::name(Table::conversations)+"("+TableMeta::column(ConversationsColumn::conversation_id)+")" + " ON DELETE CASCADE"},
        {"FOREIGN KEY ", "("+TableMeta::column(MessagesColumn::sender_id)+")"+" REFERENCES "+TableMeta::name(Table::users)+"("+TableMeta::column(UsersColumn::user_id)+")" + " ON DELETE CASCADE"},
    }))
    {
        // 插入消息到消息表
        if (dataBase.insertData(Table::messages, std::vector<SQLDataValuePair>{
            {TableMeta::column(MessagesColumn::conversation_id), static_cast<int>(conversation_id)},
            {TableMeta::column(MessagesColumn::sender_id), static_cast<int>(sender_id)},
            {TableMeta::column(MessagesColumn::content), std::string(content)},
            {TableMeta::column(MessagesColumn::message_type), std::string(type)}
        }))
        {
            if (dealerID == "dealer:MakeMsgAi")
            {
                // 获取新插入的消息ID
                int message_id = dataBase.getLastInsertId();
                // 获取voice_path
                QueryResult queryResult = dataBase.getTableData(Table::messages, std::string(TableMeta::column(MessagesColumn::voice_path)),
                    std::vector<SQLDataType>{SQLDataType::TEXT}, std::string("WHERE "+TableMeta::column(MessagesColumn::message_id)+" = "+std::to_string(message_id)));
                std::string voice_path = std::any_cast<std::string>(queryResult.rows[0][0]);
                // 将message_id、voice_path发送至前端
                json msgObj;
                msgObj["message_id"] = message_id;
                msgObj["voice_path"] = voice_path;
                std::string msgStr = msgObj.dump();
                zmq::message_t dealerId(dealerID);
                zmq::message_t msgMsg(msgStr);
                router.send(dealerId, zmq::send_flags::sndmore);
                router.send(msgMsg, zmq::send_flags::none);
            }
            else
            {
                sendTaskStatus(router, dealerID, TaskStatus::success);
            }
            isSuccess = true;
        }
        else
        {
            if (dealerID == "dealer:MakeMsgAi")
            {
                json msgObj;
                std::string msgStr = msgObj.dump();
                zmq::message_t dealerId(dealerID);
                zmq::message_t msgMsg(msgStr);
                router.send(dealerId, zmq::send_flags::sndmore);
                router.send(msgMsg, zmq::send_flags::none);
            }
            else
            {
                sendTaskStatus(router, dealerID, TaskStatus::failure);
            }
            isSuccess = false;
        }
    }
    else
    {
        if (dealerID == "dealer:MakeMsgAi")
        {
            json msgObj;
            std::string msgStr = msgObj.dump();
            zmq::message_t dealerId(dealerID);
            zmq::message_t msgMsg(msgStr);
            router.send(dealerId, zmq::send_flags::sndmore);
            router.send(msgMsg, zmq::send_flags::none);
        }
        else
        {
            sendTaskStatus(router, dealerID, TaskStatus::failure);
        }
        isSuccess = false;
    }
    return isSuccess;
}

bool DealerGetChatMsgList_getChatMsgList::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    // 收到消息样式：{ userIds: [2, 3, ...], limit: 10, offset: 0 }
    // 返回消息样式：[ { conversation_id: 1, messages: [ { sender_id: 1, content: "hello", message_type: "text", created_at: "2021-01-01 12:00:00" }, {...},...] }, {...},... ]
    stdCoutInColor(1, StringColor::BLUE, "In DealerGetChatMsgList_getChatMsgList::handle, msgBody: %s\n", msgBody.c_str());
    // 从Json对象中提取user_id数组
    try 
    {
        json chatMsgInfo = json::parse(msgBody);
        std::vector<int> user_ids;
        // 安全提取数组
        if (chatMsgInfo.contains("userIds") && chatMsgInfo["userIds"].is_array())
        {
            user_ids = chatMsgInfo["userIds"].get<std::vector<int>>();
        }
        else
        {
            stdCerrInColor(1, "In DealerGetChatMsgList_getChatMsgList::handle, 前端发送 JSON 格式错误, userIds 字段不存在或不是数组\n");
            isSuccess = false;
        }
        int limit = chatMsgInfo["limit"];    // 单页最大消息数量 用以分页提取
        int offset = chatMsgInfo["offset"];  // 偏移量
        // 打开数据库连接
        DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
        // 根据user_id数组, 查询两个用户之间的单聊会话ID, 在会话成员表、会话表中联合查询 返回会话ID // + ", " +  "cm2." + TableMeta::column(ConversationMembersColumn::user_id)
        QueryResult queryResult = dataBase.getTableData(Table::conversation_members, std::string("cm1.") + TableMeta::column(ConversationMembersColumn::conversation_id),
            std::vector<SQLDataType>{SQLDataType::INTEGER},
            std::string("cm1") + "\n" +   // FROM后面的表别名
            " INNER JOIN " + TableMeta::name(Table::conversation_members) + " cm2 ON cm1."+TableMeta::column(ConversationMembersColumn::conversation_id)+" = cm2."+TableMeta::column(ConversationMembersColumn::conversation_id)+ "\n" +
            " INNER JOIN " + TableMeta::name(Table::conversations) + " c ON cm1."+TableMeta::column(ConversationMembersColumn::conversation_id)+" = c."+TableMeta::column(ConversationsColumn::conversation_id)+ "\n" + 
            " WHERE cm1."+TableMeta::column(ConversationMembersColumn::user_id) + " = " + std::to_string(TableMeta::LORD_ID) + " AND cm2."+TableMeta::column(ConversationMembersColumn::user_id) + " IN " + intArrayToString(user_ids) +
            " AND c." +  TableMeta::column(ConversationsColumn::type) + " = " + "'single'" 
        );
        if (queryResult.rows.empty())  // 所有好友都没有对应的单聊会话 直接退出函数
        {
            stdCerrInColor(1, "In DealerGetChatMsgList_getChatMsgList::handle, 会话成员表中没有user_id: %s 的单聊会话\n", intArrayToString(user_ids).c_str());
            isSuccess = false;
        }
        else
        {
            std::vector<int> conversation_ids;
            // 提取查询到的单聊会话ID
            for (const auto& row : queryResult.rows)
            {
                conversation_ids.push_back(std::any_cast<int>(row[0]));
            }
            // 根据单聊会话ID, 在消息表中查询消息, 按时间降序(新->旧)排序, 限制数量为limit, 偏移量(跳过量)为offset
            json chatMsgListArray = json::array();
            for (const auto& conversation_id : conversation_ids)
            {
                QueryResult _queryResult = dataBase.getTableData(Table::messages, addCommaSpaceInColumnNames(std::vector<std::string>{
                    TableMeta::column(MessagesColumn::message_id), TableMeta::column(MessagesColumn::sender_id), 
                    TableMeta::column(MessagesColumn::content), TableMeta::column(MessagesColumn::message_type), 
                    TableMeta::column(MessagesColumn::voice_path), TableMeta::column(MessagesColumn::created_at) }),
                    std::vector<SQLDataType>{SQLDataType::INTEGER, SQLDataType::INTEGER, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT},
                    std::string("WHERE ") + TableMeta::column(MessagesColumn::conversation_id) + " = " + std::to_string(conversation_id) + " ORDER BY " + TableMeta::column(MessagesColumn::created_at) + " DESC " +"\n" +
                    "LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset)
                );
                if (_queryResult.rows.empty())
                {
                    stdCerrInColor(1, "In DealerGetChatMsgList_getChatMsgList::handle, 消息表中没有conversation_id: %d 的消息 或者 偏移量超出范围\n", conversation_id);
                }
                else
                {
                    // 组装json对象, 填入json数组
                    json chatMsgObj;
                    chatMsgObj["conversation_id"] = conversation_id;
                    json chatMsgArray = json::array();
                    // 读取一个会话ID下的所有消息
                    for (const auto& row : _queryResult.rows)
                    {
                        json msgObj;
                        msgObj["message_id"] = std::any_cast<int>(row[0]);
                        msgObj["sender_id"] = std::any_cast<int>(row[1]);
                        msgObj["content"] = std::any_cast<std::string>(row[2]);
                        msgObj["message_type"] = std::any_cast<std::string>(row[3]);
                        msgObj["voice_path"] = std::any_cast<std::string>(row[4]);
                        msgObj["created_at"] = std::any_cast<std::string>(row[5]);
                        chatMsgArray.push_back(msgObj);
                    }
                    chatMsgObj["messages"] = chatMsgArray;
                    chatMsgListArray.push_back(chatMsgObj);
                }
            }
            if (chatMsgListArray.empty())
            {
                stdCerrInColor(1, "In DealerGetChatMsgList_getChatMsgList::handle, 消息表中没有conversation_id: %s 的消息\n", intArrayToString(conversation_ids).c_str());
                isSuccess = false;
            }
            else
            {
                // 发送至前端
                json chatMsgListObj;
                chatMsgListObj["chatMsgList"] = chatMsgListArray;
                zmq::message_t dealerId(dealerID);
                zmq::message_t chatMsgListMsg(chatMsgListObj.dump());
                router.send(dealerId, zmq::send_flags::sndmore);
                router.send(chatMsgListMsg, zmq::send_flags::none);
                isSuccess = true;
            }
        }
    } catch (const json::exception& e) {
        stdCerrInColor(1, "In DealerGetChatMsgList_getChatMsgList::handle, JSON 解析错误: %s\n", e.what());
        isSuccess = false;
    }
    return isSuccess;
}

bool DealerGetLocalModelsSetting_getLocalModelsAndSettings::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    json localModelsSetting;
    try
    {
        // 获取本地可用模型列表
        localModelsSetting = GetLocalModels(); // { models: [ {}, {}, ...] }
    }
    catch(const std::exception& e)
    {
        stdCerrInColor(1, "In DealerGetLocalModelsSetting_getLocalModelsAndSettings::handle, 获取本地模型失败: %s\n", e.what());
        isSuccess = false;
    }
    // 再添加当前使用模型
    localModelsSetting[ConfigKeysMeta::name(ConfigKeys::ModelSelected)] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::ModelSelected));
    // 再添加最大对话记忆长度
    localModelsSetting[ConfigKeysMeta::name(ConfigKeys::MaxMemoryLength)] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::MaxMemoryLength));
    // 添加当前ASR模型
    localModelsSetting[ConfigKeysMeta::name(ConfigKeys::SenseVoiceModelSelected)] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceModelSelected));
    // 添加当前ASR 语种
    localModelsSetting[ConfigKeysMeta::name(ConfigKeys::SenseVoiceLanguage)] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceLanguage));
    // 添加当前ASR 处理线程数
    localModelsSetting[ConfigKeysMeta::name(ConfigKeys::SenseVoiceNumThreads)] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceNumThreads));
    // 添加云服务api
    localModelsSetting[ConfigKeysMeta::name(ConfigKeys::CloudServiceApi)] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::CloudServiceApi));
    // 添加是否启用情绪提示词
    localModelsSetting[ConfigKeysMeta::name(ConfigKeys::UseMotion)] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::UseMotion));
    // 添加其他提示词
    localModelsSetting[ConfigKeysMeta::name(ConfigKeys::OtherPrompt)] = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::OtherPrompt));

    // 发送至前端
    zmq::message_t dealerId(dealerID);
    zmq::message_t localModelsMsg(localModelsSetting.dump());
    router.send(dealerId, zmq::send_flags::sndmore);
    router.send(localModelsMsg, zmq::send_flags::none);
    return isSuccess;
}

bool DealerWriteSetting_writeAndSaveSetting::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    json settingObj = json::parse(msgBody);
    isSuccess = UpdateConfig(settingObj);
    if (isSuccess)
    {
        sendTaskStatus(router, dealerID, TaskStatus::success);
    }
    else
    {
        sendTaskStatus(router, dealerID, TaskStatus::failure);
    }
    return isSuccess;
}

bool DealerDeleteMsg_deleteMessageByConversationId::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    const int conversation_id = std::stoi(msgBody);  // 解析会话ID, 只有单一的一个整数
    // 打开数据库连接
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 删除所有消息对应音频文件, 如果存在
    isSuccess = deleteTtsFiles(dataBase, conversation_id);
    // 删除会话ID对应的所有消息
    if(
        dataBase.deleteData( Table::messages,
            std::string("WHERE ") + TableMeta::column(MessagesColumn::conversation_id) + " = " + std::to_string(conversation_id)
        )
    )
    {
        isSuccess = true;
    }
    else
    {
        stdCerrInColor(1, "In DealerDeleteMsg_deleteMessageByConversationId::handle, 删除会话ID: %d 对应的消息失败\n", conversation_id);
        isSuccess = false;
    }
    return isSuccess;
}

bool DealerDeleteContact_deleteContact::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    const int user_id = std::stoi(msgBody);  // 解析用户ID, 只有单一的一个整数
    // 打开数据库连接
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 寻找对应单聊会话ID
    const int conversation_id = getConversationIdByUserId(user_id);
    if (conversation_id != -1) // 存在单聊会话 则删除会话表记录
    {
        // 删除所有消息对应音频文件, 如果存在
        isSuccess = deleteTtsFiles(dataBase, conversation_id);
        if (dataBase.deleteData( Table::conversations, "WHERE " + TableMeta::column(ConversationsColumn::conversation_id) + " = " + std::to_string(conversation_id)))
        {
            isSuccess = true;
        }
        else
        {
            isSuccess = false;
            stdCerrInColor(1, "In DealerDeleteContact_deleteContact::handle, 删除会话ID: %d 失败\n", conversation_id);
        }
    }
    // 后删除用户表记录, 因为先删除会导致会话成员表缺失用户ID, 导致必然找不到单聊会话ID
    if (dataBase.deleteData( Table::users, "WHERE " + TableMeta::column(UsersColumn::user_id) + " = " + std::to_string(user_id)))
    {
        isSuccess = true;
    }
    else
    {
        isSuccess = false;
        stdCerrInColor(1, "In DealerDeleteContact_deleteContact::handle, 删除用户ID: %d 失败\n", user_id);
    }
    return isSuccess;
}

bool DealerTTS_getTTSResult::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    std::string voice_path = "";
    json ttsObj = json::parse(msgBody);                          // 解析TTS请求对象 { userId: int, messageId: int, text: string }
    const int user_id = ttsObj["userId"].get<int>();
    int message_id = ttsObj["messageId"].get<int>();
    const std::string ttsText = ttsObj["text"].get<std::string>();
    // 打开数据库连接
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 在消息表中查找该消息是否有语音文件路径
    QueryResult queryResult = dataBase.getTableData(Table::messages, TableMeta::column(MessagesColumn::voice_path), std::vector<SQLDataType>{SQLDataType::TEXT},
        std::string("WHERE ") + TableMeta::column(MessagesColumn::message_id) + " = " + std::to_string(message_id));

    // 只有一行, 其一行中只有一个数据
    voice_path = std::any_cast<std::string>(queryResult.rows[0][0]);
    stdCoutInColor(1, StringColor::BLUE, "In DealerTTS_getTTSResult::handle, voice_path: %s\n", voice_path.c_str());

    if (voice_path.empty())  // 没有语音文件路径, 则需要生成语音文件
    {
        // 在用户表中查找音色id
        QueryResult _queryResult = dataBase.getTableData(Table::users, TableMeta::column(UsersColumn::voice_engine), std::vector<SQLDataType>{SQLDataType::TEXT},
            std::string("WHERE ") + TableMeta::column(UsersColumn::user_id) + " = " + std::to_string(user_id));
        std::string voiceId = std::any_cast<std::string>(_queryResult.rows[0][0]);
        stdCoutInColor(1, StringColor::BLUE, "In DealerTTS_getTTSResult::handle, voiceId: %s\n", voiceId.c_str());

        // 开始执行tts任务
        const std::string HOST = "dashscope.aliyuncs.com";
        const std::string PORT = "443";
        const std::string TARGET = "/api-ws/v1/inference/";
        const json CloudServiceApi = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::CloudServiceApi));
        const std::string API_KEY = CloudServiceApi["TTS"]["ApiKey"].get<std::string>();
        SiliVoiceService siliVoiceService(API_KEY);
        // 字符数量检测 超过1000字符拒绝处理
        auto [chineseCount, englishCount, otherCount] = siliVoiceService.countChars(ttsText);
        float otherCountf = otherCount / 3.0;
        if (chineseCount + englishCount + otherCountf > 1000)
        {
            stdCerrInColor(1, "In DealerTTS_getTTSResult::handle, 字符数量超过1000, 请重新输入\n");
            isSuccess = false;
        }
        else
        {
            // 开始合成语音
            std::string mp3Name = getLocalTimestampForFilename() + ".mp3";
            std::string mp3Path = "../../config/tts/" + mp3Name;
            
            // std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            // std::string mp3Name = "20250714_105148_996.mp3";
            if (!siliVoiceService.TTS(voiceId, ttsText, mp3Path))
            {
                voice_path = "";
            }
            else
            {
                voice_path = mp3Path;
            }
            // 更新到数据库
            if (dataBase.updateData(
            Table::messages, 
            std::vector<SQLDataValuePair>{{TableMeta::column(MessagesColumn::voice_path), voice_path}},
            std::string("WHERE ") + TableMeta::column(MessagesColumn::message_id) + " = " + std::to_string(message_id)))
            {
                isSuccess = true;
                voice_path = mp3Name;  // 前端自主决定播放路径
            }
            else
            {
                isSuccess = false;
                stdCerrInColor(1, "In DealerTTS_getTTSResult::handle, 更新语音文件路径失败\n");
            }
        }
    }
    // 发送至前端
    zmq::message_t dealerId(dealerID);
    zmq::message_t voicePathMsg(voice_path);
    router.send(dealerId, zmq::send_flags::sndmore);
    router.send(voicePathMsg, zmq::send_flags::none);
    return isSuccess;
}

DealerASR_getASRResult::DealerASR_getASRResult()
{
    std::string modelName = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceModelSelected)).get<std::string>();
    std::string lang = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceLanguage)).get<std::string>();
    size_t numThreads = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceNumThreads)).get<size_t>();

    senseVoice = std::make_unique<SenseVoice>(modelName, lang, numThreads);
}

bool DealerASR_getASRResult::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = true;
    // 先判断是否需要重新加载模型
    std::string modelName = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceModelSelected)).get<std::string>();
    std::string lang = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceLanguage)).get<std::string>();
    size_t numThreads = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::SenseVoiceNumThreads)).get<size_t>();
    if (senseVoice->isNeedReload(modelName, lang, numThreads))
    {
        senseVoice = std::make_unique<SenseVoice>(modelName, lang, numThreads);
        stdCoutInColor(1, StringColor::BLUE, "In DealerASR_getASRResult::handle, 重新加载模型: %s, 语言: %s, 线程数: %d\n", modelName.c_str(), lang.c_str(), numThreads);
    }
    
    // ASR推理
    if (senseVoice)
    {
        std::filesystem::path wavPath = std::filesystem::path("..") / ".." / "config" / "wav" / "lord.wav";
        if (senseVoice->inference(wavPath.string()))
        {
            /*json: 
            {
                "lang": "<|zh|>", "emotion": "<|NEUTRAL|>", 
                "event": "<|Speech|>", 
                "text": "犯我中华者虽远必诛。", 
                "timestamps": [1.26, 1.44, 1.68, 1.86, 1.98, 2.34, 2.58, 2.88, 3.06, 7.08], 
                "tokens":["犯", "我", "中", "华", "者", "虽", "远", "必", "诛", "。"], 
                "words": []
            }*/
            // 发送至前端
            zmq::message_t dealerId(dealerID);
            zmq::message_t asrText(senseVoice->jsonStr);
            router.send(dealerId, zmq::send_flags::sndmore);
            router.send(asrText, zmq::send_flags::none);
            isSuccess = true;
        }
        else
        {
            stdCerrInColor(1, "In DealerASR_getASRResult::handle, 语音识别失败\n");
            isSuccess = false;
        }   
    }
    else
    {
        stdCerrInColor(1, "In DealerASR_getASRResult::handle, 未初始化SenseVoice\n");
        isSuccess = false;
    }
    return isSuccess;
}

bool DealerTtsVoiceManage_voiceManage::handle(zmq::socket_t& router, std::string& dealerID, std::string& msgBody)
{
    bool isSuccess = false;

    json voiceManageObj = json::parse(msgBody);
    const std::string spkLines = voiceManageObj["spkLines"].get<std::string>();
    const std::string spkId = voiceManageObj["spkId"].get<std::string>();
    const std::string audioFilePath = voiceManageObj["audioFilePath"].get<std::string>();
    const std::string action = voiceManageObj["action"].get<std::string>();

    json CloudServiceApi = LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::CloudServiceApi));
    const std::string apiKey = CloudServiceApi["TTS"]["ApiKey"].get<std::string>();
    SiliVoiceService siliVoiceService(apiKey);
    // 查询本地音色列表
    if (action == "list")
    {
        json SpkIds = CloudServiceApi["TTS"]["SpkIds"];
        // 发送至前端
        zmq::message_t dealerId(dealerID);
        zmq::message_t voiceList(SpkIds.dump());
        router.send(dealerId, zmq::send_flags::sndmore);
        router.send(voiceList, zmq::send_flags::none);
        isSuccess = true;
    }
    else if (action == "clone")
    {
        std::string testText = "非常开心地说话<|endofprompt|>你好啊！我是alki, 你有什么想聊的吗？";
        std::string newVoiceId =  siliVoiceService.cloneVoice(audioFilePath, spkLines);
        if (newVoiceId.empty())
        {
            stdCerrInColor(1, "In DealerTtsVoiceManage_voiceManage::handle, 克隆音色失败\n");
            sendTaskStatus(router, dealerID, TaskStatus::failure);
            isSuccess = false;
        }
        else
        {
            // 创建新的试听音频文件
            std::string testAudioFilePath = Config::generateDir.data() + std::string("tts/") + spkId + ".mp3";
            if(!siliVoiceService.TTS(newVoiceId, testText, testAudioFilePath))
            {
                stdCerrInColor(1, "In DealerTtsVoiceManage_voiceManage::handle, 生成试听音频文件失败\n");
                sendTaskStatus(router, dealerID, TaskStatus::failure);
                return false;
            }
            // 先检查是否有同名的音色spkId, 有的话先删除
            json voiceList = CloudServiceApi["TTS"]["SpkIds"];
            size_t index = 0;
            for (const auto& voice : voiceList)
            {
                bool isSame = false;
                // 分别获取键和值
                for (auto& [name, voiceId] : voice.items()) {
                    if (name == spkId)
                    {
                        // 先在配置文件中删除
                        CloudServiceApi["TTS"]["SpkIds"].erase(index);
                        json newCloudServiceApi = {{"CloudServiceApi", CloudServiceApi}};
                        UpdateConfig(newCloudServiceApi);
                        // 再删除云端音色
                        if(!siliVoiceService.deleteVoice(voiceId))
                        {
                            stdCerrInColor(1, "In DealerTtsVoiceManage_voiceManage::handle, 删除云端音色 %s 失败\n", name.c_str());
                            return false;
                        }
                        isSame = true;
                        break;
                    }
                }
                index++;
                if (isSame) break;
            }
            CloudServiceApi["TTS"]["SpkIds"].push_back({{spkId, newVoiceId}, {spkId + "mp3Name", spkId + ".mp3"}});
            json newCloudServiceApi = {{"CloudServiceApi", CloudServiceApi}};
            // 更新配置文件
            UpdateConfig(newCloudServiceApi);
            sendTaskStatus(router, dealerID, TaskStatus::success);
            isSuccess = true;
        }
    }
    else if (action == "delete")
    {
        // 发送来的sokId = ["spkId1", "spkId2", "spkId3", ...]
        json spkIds = json::parse(spkId);
        // 逐个删除本地和云端音色、试听文件
        for (const auto& spkId_ : spkIds)
        {
            std::string spkId_str = spkId_.get<std::string>();
            std::string testAudioFilePath = Config::generateDir.data() + std::string("tts/") + spkId_str + ".mp3";
            // 删除本地试听文件
            if(!deleteFile(testAudioFilePath))
            {
                stdCerrInColor(1, "In DealerTtsVoiceManage_voiceManage::handle, 删除本地试听文件 %s 失败\n", testAudioFilePath.c_str());
                sendTaskStatus(router, dealerID, TaskStatus::failure);
                return false;
            }
            json voiceList = CloudServiceApi["TTS"]["SpkIds"];
            size_t index = 0;
            for (const auto& voice : voiceList)
            {
                bool isSame = false;
                // 分别获取键和值 遍历一个对象内的键和值
                for (auto& [name, voiceId] : voice.items()) {
                    if (name == spkId_str)
                    {

                        // 先在配置文件中删除
                        CloudServiceApi["TTS"]["SpkIds"].erase(index);
                        json newCloudServiceApi = {{"CloudServiceApi", CloudServiceApi}};
                        UpdateConfig(newCloudServiceApi);
                        // 再删除云端音色
                        if(!siliVoiceService.deleteVoice(voiceId))
                        {
                            stdCerrInColor(1, "In DealerTtsVoiceManage_voiceManage::handle, 删除云端音色 %s 失败\n", name.c_str());
                            sendTaskStatus(router, dealerID, TaskStatus::failure);
                            return false;
                        }
                        isSame = true;
                        break;
                    }
                }
                index++;
                if (isSame) break;
            }
        }
        // 再查询云端音色列表
        if (siliVoiceService.queryVoiceList().empty())
        {
            stdCoutInColor(1, StringColor::BLUE, "In DealerTtsVoiceManage_voiceManage::handle, 云端音色列表为空\n");
        }
        sendTaskStatus(router, dealerID, TaskStatus::success);
        isSuccess = true;
    }
    return isSuccess;
}