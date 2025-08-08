#include "dataBase.h"


std::string addCommaSpaceInColumnNames(const std::vector<std::string>& columnNames)
{
    std::string result;
    std::string space = ", ";
    for (const auto& columnName : columnNames)
    {
        result += columnName + space;
    }
    if (columnNames.empty())
    {
        stdCerrInColor(1, "In addCommaSpaceInColumnNames(), 列名为空.\n");
    }
    if (!result.empty()) 
    {
        result.erase(result.rfind(','), 1);  // 返回最后逗号索引并删除 1表示从索引开始删除的字符数量
    }
    return result;
}

std::string intArrayToString(const std::vector<int>& arr)
{
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i != 0) oss << ", ";
        oss << arr[i];
    }
    oss << ")";
    return oss.str();
}

QueryResult getUserInfo(const int user_id)
{
    QueryResult userInfo;
    // 打开数据库链接
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 构造查询SQL语句
    userInfo = dataBase.getTableData(Table::users, addCommaSpaceInColumnNames(
        std::vector<std::string>{
            TableMeta::column(UsersColumn::user_name),
            TableMeta::column(UsersColumn::gender),
            TableMeta::column(UsersColumn::avatar_url),
            TableMeta::column(UsersColumn::post_url),
            TableMeta::column(UsersColumn::created_at),
            TableMeta::column(UsersColumn::area),
            TableMeta::column(UsersColumn::flag_url),
            TableMeta::column(UsersColumn::presupposition),
            TableMeta::column(UsersColumn::is_deleted),
            TableMeta::column(UsersColumn::voice_engine)
        }),
        std::vector<SQLDataType>{
            SQLDataType::TEXT,
            SQLDataType::TEXT,
            SQLDataType::TEXT,
            SQLDataType::TEXT,
            SQLDataType::TEXT,
            SQLDataType::TEXT,
            SQLDataType::TEXT,
            SQLDataType::TEXT,
            SQLDataType::INTEGER,
            SQLDataType::TEXT
        },
        std::string("WHERE user_id = " + std::to_string(user_id) )
    );
    return userInfo;
}

QueryResult getSingleChatMsgsByUserId(const int user_id, const int limit)
{
    QueryResult singleChatMsgs;
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 根据单个user_id, 查询用户和机主之间的单聊会话ID, 在会话成员表、会话表中联合查询 返回会话ID
    QueryResult queryResult = dataBase.getTableData(Table::conversation_members, std::string("cm1.") + TableMeta::column(ConversationMembersColumn::conversation_id),
        std::vector<SQLDataType>{SQLDataType::INTEGER},
        std::string("cm1") + "\n" +   // FROM后面的表别名
        " INNER JOIN " + TableMeta::name(Table::conversation_members) + " cm2 ON cm1."+TableMeta::column(ConversationMembersColumn::conversation_id)+" = cm2."+TableMeta::column(ConversationMembersColumn::conversation_id)+ "\n" +
        " INNER JOIN " + TableMeta::name(Table::conversations) + " c ON cm1."+TableMeta::column(ConversationMembersColumn::conversation_id)+" = c."+TableMeta::column(ConversationsColumn::conversation_id)+ "\n" + 
        " WHERE cm1."+TableMeta::column(ConversationMembersColumn::user_id) + " = " + std::to_string(TableMeta::LORD_ID) + " AND cm2."+TableMeta::column(ConversationMembersColumn::user_id) + " = " + std::to_string(user_id) +
        " AND c." +  TableMeta::column(ConversationsColumn::type) + " = " + "'single'" 
    );
    if (queryResult.rows.empty())  // 所有好友都没有对应的单聊会话 直接退出函数
    {
        stdCerrInColor(1, "In getSingleChatMsgsByUserId, 会话成员表中没有user_id: %s 的单聊会话\n", std::to_string(user_id).c_str());
    }
    else
    {
        // 提取查询到的单聊会话ID
        int conversation_id = std::any_cast<int>(queryResult.rows[0][0]);
        // 根据单聊会话ID, 在消息表中查询消息, 按时间降序(新->旧)排序, 限制数量为limit, 偏移量为offset
        singleChatMsgs = dataBase.getTableData(Table::messages, addCommaSpaceInColumnNames(std::vector<std::string>{
            TableMeta::column(MessagesColumn::sender_id), TableMeta::column(MessagesColumn::content), 
            TableMeta::column(MessagesColumn::message_type), TableMeta::column(MessagesColumn::created_at) }),
            std::vector<SQLDataType>{SQLDataType::INTEGER, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT},
            std::string("WHERE ") + TableMeta::column(MessagesColumn::conversation_id) + " = " + std::to_string(conversation_id) + " ORDER BY " + TableMeta::column(MessagesColumn::created_at) + " DESC " +"\n" +
            "LIMIT " + std::to_string(limit)
        );
        if (singleChatMsgs.rows.empty())
        {
            stdCerrInColor(1, "In getSingleChatMsgsByUserId, 消息表中没有conversation_id: %d 的消息\n", conversation_id);
        }
    }
    return singleChatMsgs;
}

int getConversationIdByUserId(const int user_id)
{
    int conversation_id = -1;
    DataBase dataBase(LoadValueFromConfig(ConfigKeysMeta::name(ConfigKeys::DatabasePath)));
    // 根据单个user_id, 查询用户和机主之间的单聊会话ID, 在会话成员表、会话表中联合查询 返回会话ID
    QueryResult queryResult = dataBase.getTableData(Table::conversation_members, std::string("cm1.") + TableMeta::column(ConversationMembersColumn::conversation_id),
        std::vector<SQLDataType>{SQLDataType::INTEGER},
        std::string("cm1") + "\n" +   // FROM后面的表别名
        " INNER JOIN " + TableMeta::name(Table::conversation_members) + " cm2 ON cm1."+TableMeta::column(ConversationMembersColumn::conversation_id)+" = cm2."+TableMeta::column(ConversationMembersColumn::conversation_id)+ "\n" +
        " INNER JOIN " + TableMeta::name(Table::conversations) + " c ON cm1."+TableMeta::column(ConversationMembersColumn::conversation_id)+" = c."+TableMeta::column(ConversationsColumn::conversation_id)+ "\n" + 
        " WHERE cm1."+TableMeta::column(ConversationMembersColumn::user_id) + " = " + std::to_string(TableMeta::LORD_ID) + " AND cm2."+TableMeta::column(ConversationMembersColumn::user_id) + " = " + std::to_string(user_id) +
        " AND c." +  TableMeta::column(ConversationsColumn::type) + " = " + "'single'" 
    );
    if (queryResult.rows.empty())  // 所有好友都没有对应的单聊会话 直接退出函数
    {
        stdCerrInColor(1, "In getConversationIdByUserId, 会话成员表中没有user_id: %s 的单聊会话\n", std::to_string(user_id).c_str());
    }
    else
    {
        // 提取查询到的单聊会话ID
        conversation_id = std::any_cast<int>(queryResult.rows[0][0]);
    }
    return conversation_id;
}


// 构造器
DataBase::DataBase(const std::string& dbPath_, bool isForeignKeysEnabled) : dbPath(dbPath_), db(dbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)  // 打开或创建数据库(不存在)
{
    // 初始化数据库路径
    // 打开或者创建新数据库(如果不存在)
    stdCoutInColor(1, StringColor::GREEN, "In DataBase::DataBase(), 打开数据库: %s 成功!\n", dbPath.c_str());
    if (isForeignKeysEnabled)
    {
        db.exec("PRAGMA foreign_keys = ON;");  // 只要一直持有db实例, 外键约束一直有效
        stdCoutInColor(1, StringColor::GREEN, "In DataBase::DataBase(), 打开数据库: %s 时启用外键约束成功!\n", dbPath.c_str());
    }
    
}

bool DataBase::isTableExist(Table table)
{
    // 构造查询SQL语句
    std::string SqlContent = "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name =:tableName;";
    SQLite::Statement checkTable(db, SqlContent);
    checkTable.bind(":tableName", TableMeta::name(table));
    // 执行查询
    try
    {
        if (checkTable.executeStep()) {
            int count = checkTable.getColumn(0).getInt();
            return (count > 0);
        }
    }
    catch(const std::exception& e)
    {
        stdCerrInColor(1, "In DataBase::isTableExist(), 查询表 %s 发生错误: %s.\n", TableMeta::name(table).c_str(), e.what());
        stdCerrInColor(1, "SQL语句:\n %s\n", SqlContent.c_str());
        return false;
    }
    return false;
}

bool DataBase::deleteData(Table table, const std::string &condition)
{
    // 先查询表格是否存在
    if(isTableExist(table))
    {
        // 构造删除数据的SQL语句
        std::string SqlContent = "DELETE FROM " + TableMeta::name(table) + " " + condition + ";";
        SQLite::Statement deleteData(db, SqlContent);
        try
        {
            deleteData.exec();
            stdCoutInColor(1, StringColor::GREEN, "In DataBase::deleteData(),  从表 %s 删除数据成功.\n", TableMeta::name(table).c_str());
            return true;
        }
        catch(const std::exception& e)
        {
            stdCerrInColor(1, "In DataBase::deleteData(), 从表 %s 删除数据失败: %s.\n", TableMeta::name(table).c_str(), e.what());
            stdCerrInColor(1, "SQL语句:\n %s\n", SqlContent.c_str());
            return false;
        }
    }
    else
    {
        stdCerrInColor(1, "In DataBase::deleteData(), 表 %s 不存在.\n", TableMeta::name(table).c_str());
        return false;
    }
}

long long DataBase::getLastInsertId()
{
    return db.getLastInsertRowid();
}

QueryResult DataBase::getTableData(Table table, const std::vector<SQLDataType> &columnTypes)
{
    QueryResult queryResult;
    queryResult.columnTypes = columnTypes;
    // 先查询表格是否存在
    if(isTableExist(table))
    {
        // 构造查询数据的SQL语句
        std::string SqlContent = "SELECT * FROM " + TableMeta::name(table) + ";";
        SQLite::Statement queryData(db, SqlContent);
        size_t dataLength = columnTypes.size();
        try
        {
            while (queryData.executeStep())
            {
                std::vector<std::any> row;
                for (int i = 0; i < static_cast<int>(dataLength); i++)
                {
                    switch (columnTypes[i])
                    {
                        case SQLDataType::NULLVALUE:
                            row.push_back(nullptr);
                            break;
                        case SQLDataType::INTEGER:
                            row.push_back(queryData.getColumn(i).getInt());
                            break;
                        case SQLDataType::REAL:
                            row.push_back(queryData.getColumn(i).getDouble());
                            break;
                        case SQLDataType::TEXT:
                            row.push_back(queryData.getColumn(i).getString());
                            break;
                        case SQLDataType::BLOB:
                        {
                            // 处理 BLOB（二进制数据）
                            const void* blob = queryData.getColumn(i).getBlob();
                            int size = queryData.getColumn(i).getBytes();
                            //  vector 的范围构造函数可以直接将二进制数据转化为 vector
                            std::vector<uint8_t> data(static_cast<const uint8_t*>(blob), 
                                                    static_cast<const uint8_t*>(blob) + size);
                            row.push_back(data);
                            break;
                        }
                        default:
                            throw std::runtime_error(
                                "In DataBase::getTableData(), 第 " + 
                                std::to_string(i) + 
                                " 列是未知SQL数据类型"
                            );  // 直接跳出函数
                    }
                }
                queryResult.rows.push_back(row);
            }
            return queryResult;
        }
        catch(const std::exception& e)
        {
            stdCerrInColor(1, "In DataBase::getTableData(), 从表 %s 查询数据失败: %s.\n", TableMeta::name(table).c_str(), e.what());
            stdCerrInColor(1, "SQL语句:\n %s\n", SqlContent.c_str());
            return QueryResult();
        }
    }
    else
    {
        stdCerrInColor(1, "In DataBase::getTableData(), 表 %s 不存在.\n", TableMeta::name(table).c_str());
        return QueryResult();
    }
}

QueryResult DataBase::getTableData(Table table,const std::string &columnNames,const std::vector<SQLDataType> &columnTypes, const std::string &limitDescription)
{
    QueryResult queryResult;
    queryResult.columnTypes = columnTypes;
    // 先查询表格是否存在
    if(isTableExist(table))
    {
        // 构造查询数据的SQL语句
        std::string SqlContent = "SELECT " + columnNames + " FROM " + TableMeta::name(table) + " " + limitDescription + ";";
        SQLite::Statement queryData(db, SqlContent);
        size_t dataLength = columnTypes.size();
        try
        {
            while (queryData.executeStep())
            {
                std::vector<std::any> row;
                for (int i = 0; i < static_cast<int>(dataLength); i++)
                {
                    switch (columnTypes[i])
                    {
                        case SQLDataType::NULLVALUE:
                            row.push_back(nullptr);
                            break;
                        case SQLDataType::INTEGER:
                            row.push_back(queryData.getColumn(i).getInt());
                            break;
                        case SQLDataType::REAL:
                            row.push_back(queryData.getColumn(i).getDouble());
                            break;
                        case SQLDataType::TEXT:
                            row.push_back(queryData.getColumn(i).getString());
                            break;
                        case SQLDataType::BLOB:
                        {
                            // 处理 BLOB（二进制数据）
                            const void* blob = queryData.getColumn(i).getBlob();
                            int size = queryData.getColumn(i).getBytes();
                            //  vector 的范围构造函数可以直接将二进制数据转化为 vector
                            std::vector<uint8_t> data(static_cast<const uint8_t*>(blob), 
                                                    static_cast<const uint8_t*>(blob) + size);
                            row.push_back(data);
                            break;
                        }
                        default:
                            throw std::runtime_error(
                                "In DataBase::getTableData(), 第 " + 
                                std::to_string(i) + 
                                " 列是未知SQL数据类型"
                            );  // 直接跳出函数
                    }
                }
                queryResult.rows.push_back(row);
            }
            return queryResult;
        }
        catch(const std::exception& e)
        {
            stdCerrInColor(1, "In DataBase::getTableData(), 从表 %s 查询数据失败: %s.\n", TableMeta::name(table).c_str(), e.what());
            stdCerrInColor(1, "SQL语句:\n %s\n", SqlContent.c_str());
            return QueryResult();
        }
    }
    else
    {
        stdCerrInColor(1, "In DataBase::getTableData(), 表 %s 不存在.\n", TableMeta::name(table).c_str());
        return QueryResult();
    }
}

bool initDataBase()
{
    
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
        QueryResult queryResult = dataBase.getTableData(Table::users, std::vector<SQLDataType>{
            SQLDataType::INTEGER, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::TEXT, SQLDataType::INTEGER, SQLDataType::TEXT
        });
        // 如果查询结果为空, 说明不存在机主信息, 则插入机主信息
        if (queryResult.rows.empty())
        {
            // 插入到数据库
            if( dataBase.insertData(Table::users, std::vector<SQLDataValuePair> {
                {TableMeta::column(UsersColumn::user_id), static_cast<int>(1)},
                {TableMeta::column(UsersColumn::user_name), static_cast<std::string>("lord")},
                {TableMeta::column(UsersColumn::gender), static_cast<std::string>("")},
                {TableMeta::column(UsersColumn::avatar_url), static_cast<std::string>("")},
                {TableMeta::column(UsersColumn::post_url), static_cast<std::string>("")},
                {TableMeta::column(UsersColumn::area), static_cast<std::string>("")},
                {TableMeta::column(UsersColumn::flag_url), static_cast<std::string>("")},
                {TableMeta::column(UsersColumn::presupposition), static_cast<std::string>("")},
                {TableMeta::column(UsersColumn::voice_engine), static_cast<std::string>("")}
            }))
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else  // 如果查询结果不为空, 说明存在机主信息, 则不插入机主信息 直接返回true
        {
            return true;
        }
    }
    else
    {
        return false;
    }
}

bool deleteTtsFiles(DataBase& db, const int conversation_id)
{
    bool isSuccess = true;
    QueryResult queryResult = db.getTableData(Table::messages, TableMeta::column(MessagesColumn::voice_path), std::vector<SQLDataType>{SQLDataType::TEXT},
        std::string("WHERE ") + TableMeta::column(MessagesColumn::conversation_id) + " = " + std::to_string(conversation_id));
    if (!queryResult.rows.empty())
    {
        for (const auto& row : queryResult.rows)
        {
            std::string voice_path = std::any_cast<std::string>(row[0]);
            if (!voice_path.empty())
            {
                if(!deleteFile(voice_path))
                {
                    stdCerrInColor(1, "In deleteTtsFiles, 删除语音文件失败: %s\n", voice_path.c_str());
                    isSuccess = false;
                    break;
                }
            }
        }
    }
    return isSuccess;
}

