// 此头文件存放数据库相关的类和函数
#ifndef DATA_BASE_H
#define DATA_BASE_H
#include <iostream>
#include <sstream>
#include <variant>
#include <vector>
#include <any> 
#include <SQLiteCpp/SQLiteCpp.h>
#include "debugTools.h"
#include "config.h"

// 表名和列名强类型定义
enum class Table { Test, users, conversations, conversation_members, messages };
enum class TestColumn { user_id, user_name, gender };
enum class UsersColumn { user_id, user_name, gender, avatar_url, post_url, created_at, area, flag_url, presupposition, is_deleted, voice_engine };
enum class ConversationsColumn { conversation_id, type, name, updated_at };
enum class ConversationMembersColumn { conversation_id, user_id };
enum class MessagesColumn { message_id, conversation_id, sender_id, content, message_type, voice_path, created_at };
// 表结构元信息集中管理
struct TableMeta {
    // 机主ID
    static const int LORD_ID = 1;
    // 获取表名
    static std::string name(Table table) {
        switch(table) {
            case Table::Test:                 return "TEST";
            case Table::users:                return "users";
            case Table::conversations:        return "conversations";
            case Table::conversation_members: return "conversation_members";
            case Table::messages:             return "messages";
            default: throw std::invalid_argument("Unknown table");
        }
    }

    // 获取列名
    // Test表
    static std::string column(TestColumn col) {
        switch(col) {
            case TestColumn::user_id:   return "user_id";
            case TestColumn::user_name: return "NAME";
            case TestColumn::gender:    return "AGE";
            default: throw std::invalid_argument("Unknown Test column");
        }
    }
    // user表
    static std::string column(UsersColumn col) {
        switch(col) {
            case UsersColumn::user_id:        return "user_id";
            case UsersColumn::user_name:      return "user_name";
            case UsersColumn::gender:         return "gender";
            case UsersColumn::avatar_url:     return "avatar_url";
            case UsersColumn::post_url:       return "post_url";
            case UsersColumn::created_at:     return "created_at";
            case UsersColumn::area:           return "area";
            case UsersColumn::flag_url:       return "flag_url";
            case UsersColumn::presupposition: return "presupposition";
            case UsersColumn::is_deleted:     return "is_deleted";
            case UsersColumn::voice_engine:   return "voice_engine";
            default: throw std::invalid_argument("Unknown users column");
        }
    }
    // conversations表
    static std::string column(ConversationsColumn col) {
        switch(col) {
            case ConversationsColumn::conversation_id: return "conversation_id";
            case ConversationsColumn::type:            return "type";
            case ConversationsColumn::name:            return "name";
            case ConversationsColumn::updated_at:      return "updated_at";
            default: throw std::invalid_argument("Unknown conversations column");
        }
    }
    // conversation_members表
    static std::string column(ConversationMembersColumn col) {
        switch(col) {
            case ConversationMembersColumn::conversation_id: return column(ConversationsColumn::conversation_id);
            case ConversationMembersColumn::user_id:         return column(UsersColumn::user_id);
            default: throw std::invalid_argument("Unknown conversation_members column");
        }
    }
    // messages表
    static std::string column(MessagesColumn col) {
        switch(col) {
            case MessagesColumn::message_id:      return "message_id";
            case MessagesColumn::conversation_id: return column(ConversationsColumn::conversation_id);
            case MessagesColumn::sender_id:       return "sender_id";
            case MessagesColumn::content:         return "content";
            case MessagesColumn::message_type:    return "message_type";
            case MessagesColumn::voice_path:      return "voice_path";
            case MessagesColumn::created_at:      return "created_at";
            default: throw std::invalid_argument("Unknown messages column");
        }
    }
};
// 定义值的可能类型
using SQLDataValueType = std::variant<int, float, std::string>;
// 插入数据键值对类型：键是字符串，值可能是 int/float/string
using SQLDataValuePair = std::pair<std::string, SQLDataValueType>;

// SQL数据类型
enum class SQLDataType { NULLVALUE, INTEGER, REAL, TEXT, BLOB };
// 查询结果结构体
struct QueryResult {
    std::vector<SQLDataType> columnTypes;    // 列类型列表
    std::vector<std::vector<std::any>> rows; // 每一行的值
};

/**
 * @brief addCommaSpaceInColumnNames 给列名加空格 方便SQL语句拼接
 * @param columnNames  std::vector<std::string>  列名列表
 * @return std::string 加了逗号和空格的列名列表
 * @note ["name", "age"] => "name, age"
 */
std::string addCommaSpaceInColumnNames(const std::vector<std::string>& columnNames); // 给列名加空格 方便SQL语句拼接

/**
 * @brief intArrayToString 整型数组转字符串
 * @param arr  std::vector<int> 整型数组
 * @return std::string 整型数组转字符串
 * @note [1, 2, 3] => "(1, 2, 3)"
 */
std::string intArrayToString(const std::vector<int>& arr); // 整型数组转字符串

/**
 * @brief initDataBase 初始化数据库 创建users表并添加机主信息 指定其user_id为1
 * @note 仅在第一次运行时调用
 * @return bool 是否成功
 */
bool initDataBase();

/**
 * @brief getUserInfo 获取用户信息
 * @param user_id 用户ID
 * @return QueryResult 用户信息 只有一行数据
 */
QueryResult getUserInfo(const int user_id);

/**
 * @brief getSingleChatMsgsByUserId 获取单聊消息列表
 * @param user_id 用户ID
 * @param limit 限制条数
 * @return QueryResult 单聊消息列表
 */
QueryResult getSingleChatMsgsByUserId(const int user_id, const int limit);

/**
 * @brief getConversationIdByUserId 获取某个用户的单聊会话ID
 * @param user_id 用户ID
 * @return int 会话ID
 */
int getConversationIdByUserId(const int user_id);

class DataBase {
public:
    // 构造函数
    /**
     * @brief DataBase 构造函数
     * @param dbPath_ 数据库文件路径
     */
    DataBase(const std::string& dbPath_, bool isForeignKeysEnabled = true);

    // 成员函数
    /**
     * @brief 打开或新建数据库表
     * @param table 表名
     * @param colNameInfoMap 列名和描述映射 std::vector<std::pair<std::string, std::string>> 映射为<列名, 描述>
     * @return bool 是否成功
     */
    template<typename ColNameInfoMap>
    bool createTable(Table table, const ColNameInfoMap& colNameInfoMap)
    {
        // 构造创建表的SQL语句
        // 遍历ColNameInfoMap <colName, description>, 使用结构化绑定, 构造创建表的SQL语句
        std::ostringstream createTableSql;
        createTableSql << "CREATE TABLE IF NOT EXISTS " << TableMeta::name(table) << "(\n";
        for (const auto& [colName, description] : colNameInfoMap)
        {
            createTableSql << SQL_Space << colName << " " << description << ",\n";
        }
        createTableSql << ");";
        // 删除最后一个,符号 保证SQL语句完整性
        std::string SqlContent = createTableSql.str();
        if (!SqlContent.empty()) 
        {
            SqlContent.erase(SqlContent.rfind(','), 1);  // 返回最后逗号索引并删除 1表示从索引开始删除的字符数量
        }
        try
        {
            db.exec(SqlContent);
        }
        catch(const std::exception& e)
        {
            stdCerrInColor(1, "In DataBase::createTable(), 创建表 %s 失败: %s.\n", TableMeta::name(table).c_str(), e.what());
            stdCerrInColor(1, "SQL语句:\n %s\n", SqlContent.c_str());
            return false;
        }
        stdCoutInColor(1, StringColor::GREEN, "In DataBase::createTable(), 打开/创建表 %s 成功.\n", TableMeta::name(table).c_str());
        return true;
    }

    /**
     * @brief isTableExist 检查某个表是否存在
     * @param table 表名
     * @return bool 是否存在
     */
    bool isTableExist(Table table);

    /**
     * @brief insertData 插入数据
     * @param table 表名
     * @param colNameValueMap 列名和值映射 插入数据 map类型为std::vector<SQLDataValuePair> 值类型只可能是 int/float/string
     * @return bool 是否成功   插入数据时, 严格使用static_cast<int/float/std::string>(value)进行类型转换, 否则可能会导致运行时错误
     */
    template<typename ColNameValueMap>
    bool insertData(Table table, const ColNameValueMap& colNameValueMap)
    {
        // 构造插入数据的SQL语句
        std::ostringstream insertDataSql;
        insertDataSql << "INSERT INTO " << TableMeta::name(table) << "(";
        std::string tail;
        for (const auto& [colName, value] : colNameValueMap)
        {
            insertDataSql << SQL_Space << colName << ", ";
            tail = tail + "?,";
        }
        std::string SqlContent = insertDataSql.str();
        if (!SqlContent.empty())
        {
            SqlContent.erase(SqlContent.rfind(','), 1);  // 返回最后逗号索引并删除
        }
        if (!tail.empty())
        {
            tail.erase(tail.rfind(','), 1);  // 返回最后逗号索引并删除
        }
        SqlContent = SqlContent + ") VALUES(" + tail + ");";
        SQLite::Statement insert(db, SqlContent);
        // 开始批量绑定数据
        int bindCount = 0;
        for (const auto& [colName, value] : colNameValueMap)
        {
            bindCount++;
            // 运行时依次检查可能的类型
            if (const int* ptr = std::get_if<int>(&value)) {
                insert.bind(bindCount, *ptr);
            } else if (const float* ptr = std::get_if<float>(&value)) {
                insert.bind(bindCount, *ptr);
            } else if (const std::string* ptr = std::get_if<std::string>(&value)) {
                insert.bind(bindCount, *ptr);
            } else {
                stdCerrInColor(1, "In DataBase::insertData(), 插入未知类型值.\n");
            }
        }
        try
        {
            insert.exec();
            stdCoutInColor(1, StringColor::GREEN, "In DataBase::insertData(),  向表 %s 插入数据成功.\n", TableMeta::name(table).c_str());
            return true;
        }
        catch(const std::exception& e)
        {
            stdCerrInColor(1, "In DataBase::insertData(), 向表 %s 插入数据失败: %s.\n", TableMeta::name(table).c_str(), e.what());
            return false;
        }
    }

    /**
     * @brief deleteData 在限定条件下删除数据
     * @param table 表名
     * @param condition 限定条件 末尾不需要加';'
     * @return bool 是否成功
     */
    bool deleteData(Table table, const std::string &condition);

    /**
     * @brief getTableData 获取表所有行数据
     * @param table 表名
     * @param columnTypes 列类型列表 输入几个类型, 就返回几个列的数据
     * @return QueryResult 查询结果
     */
    QueryResult getTableData(Table table, const std::vector<SQLDataType> &columnTypes);

    /**
     * @brief getTableData 根据限定条件获取表指定列的指定行数据
     * @param table 表名
     * @param columnNames 列名列表 指定要获取的列名 记得手动加空格
     * @param columnTypes 列类型列表 指定每列数据类型
     * @param limitDescription 限定条件 末尾不需要加;
     * @return QueryResult 查询结果
     */
    QueryResult getTableData(Table table,const std::string &columnNames,const std::vector<SQLDataType> &columnTypes, const std::string &limitDescription);

    /**
     * @brief getLasInsertId 获取最后插入的主键值ID
     * @return long long 最后插入的ID
     */
    long long getLastInsertId();

    /**
     * @brief uppdateData 更新数据
     * @param table 表名
     * @param colNameValueMap 列名和值映射 输入要更新的列名和值 map类型为std::vector<SQLDataValuePair> 值类型只可能是 int/float/string
     * @param condition 限定条件 末尾不需要加';'
     * @return bool 是否成功
     */
    template<typename ColNameValueMap>
    bool updateData(Table table, const ColNameValueMap& colNameValueMap, const std::string &condition)
    {
        for (const auto& [colName, value] : colNameValueMap)
        {
            // 构造更新数据的SQL语句
            std::ostringstream updateDataSql;
            updateDataSql << "UPDATE " << TableMeta::name(table) << " SET " << colName << " = ";
            
            // 运行时依次检查可能的类型
            if (const int* ptr = std::get_if<int>(&value)) {
                updateDataSql << *ptr;
            } else if (const float* ptr = std::get_if<float>(&value)) {
                updateDataSql << *ptr;
            } else if (const std::string* ptr = std::get_if<std::string>(&value)) {
                updateDataSql << std::string("\"") + *ptr + std::string("\"");
            } else {
                stdCerrInColor(1, "In DataBase::updateData(), 插入未知类型值.\n");
            }
            updateDataSql << condition << ";";
            std::string SqlContent = updateDataSql.str();
            SQLite::Statement update(db, SqlContent);
            try
            {
                update.exec();
                stdCoutInColor(1, StringColor::GREEN, "In DataBase::updateData(),  向表 %s 行 %s 更新数据成功.\n", TableMeta::name(table).c_str(), colName.c_str());
            }
            catch(const std::exception& e)
            {
                stdCerrInColor(1, "In DataBase::updateData(), 向表 %s 行 %s 更新数据失败: %s.\n", TableMeta::name(table).c_str(), colName.c_str(), e.what());
                return false;
            }
        }
        return true;
    }

    // 成员变量
private:
    std::string dbPath;                                   // 数据库文件路径
    SQLite::Database db;                                  // 打开的数据库实例 离开作用域自动关闭数据库
    static inline const std::string SQL_Space = "    ";   // SQL语句缩进空格 四个空格 增加易读性 加inline才能在类内定义 否则只能在cpp中定义
};

/**
 * @brief deleteTtsFiles 删除某个会话的TTS文件
 * @param db 数据库实例
 * @param conversation_id 会话ID
 * @return bool 是否成功
 */
bool deleteTtsFiles(DataBase& db, const int conversation_id);

#endif // DATA_BASE_H