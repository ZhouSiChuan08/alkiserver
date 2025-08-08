#ifndef DATATYPE_H
#define DATATYPE_H
#include <string>
#include <sstream> // 包含stringstream的头文件
#include <nlohmann/json.hpp>

using json = nlohmann::json;
enum class ResponseType {
    generate,
    chat
};

/**
 * @brief 响应时间结构体 存储回复消息的时间戳
 */
struct ResponseTime
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    double second = 0;
};
/**
 * @brief 响应结构体 存储回复消息的相关信息
 */
class Response
{
public:
    Response(std::string model = "default_model", std::string created_at = "default_created_at", 
        std::string response = "default_response", bool done = false, 
        std::string done_reason = "default_done_reason" ) : model(model), created_at(created_at), 
        response(response), done(done), done_reason(done_reason) {}

    // 成员函数
    ResponseTime parseResponseTime(const std::string& time_str) {
        ResponseTime response_time;
        // TODO: 解析时间字符串 时间格式 2025-02-09T08:48:44.3204254Z
        response_time.year = std::stoi(time_str.substr(0, 4));
        response_time.month = std::stoi(time_str.substr(5, 2));
        response_time.day = std::stoi(time_str.substr(8, 2));
        response_time.hour = std::stoi(time_str.substr(11, 2));
        response_time.minute = std::stoi(time_str.substr(14, 2));
        response_time.second = std::stod(time_str.substr(17, 9));

        return response_time;
    }
    // 成员变量
    std::string model;         // 模型名称
    std::string created_at;    // 创建时间
    std::string response;      // 回复文本消息
    bool done;                 // 是否完成
    std::string done_reason;   // 完成原因
};
/**
 * @brief 解析响应数据
 * @param response 响应数据字符串
 * @param responseType 响应类型
 * @return 解析后的响应数据队列
 */
std::vector<Response> parseResponse(const std::string& response, const ResponseType& responseType);


#endif // DATATYPE_H