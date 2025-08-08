#include "dataType.h"

std::vector<Response> parseResponse(const std::string& response, const ResponseType& responseType)
{
    // 首先分割response字符串 按行分割
    std::vector<Response> responses;  // 定义一个空的vector
    std::istringstream iss(response); // 定义一个字符串流对象
    std::string json_item; // 定义一个空字符串变量
    // 使用while循环和std::getline函数来读取iss中的内容
    while (std::getline(iss, json_item)) // 每行都是一个jsontext
    {
        // 判断行字符串是否是JSON格式
        if (json_item.front() == '{' && json_item.back() == '}')
        {
            // 解析JSON字符串
            json j = json::parse(json_item);
            // 解析JSON字符串中的各个字段
            if (json_item.find("done_reason")!= std::string::npos)  // 检查JSON中是否包含"done_reason"字段 npos表示未找到或无效的特殊值
            {
                switch (responseType)
                {
                    case ResponseType::generate: {
                        Response response(j["model"], j["created_at"], j["response"], j["done"], j["done_reason"]);
                        responses.push_back(response);
                    } break;
                    case ResponseType::chat: {
                        Response response(j["model"], j["created_at"], j["message"]["content"], j["done"], j["done_reason"]);
                        responses.push_back(response);
                    } break;
                    default:
                        break;
                }
            }
            else
            {
                switch (responseType)
                {
                    case ResponseType::generate: {
                        Response response(j["model"], j["created_at"], j["response"], j["done"]);
                        responses.push_back(response);
                    } break;
                    case ResponseType::chat: {
                        Response response(j["model"], j["created_at"], j["message"]["content"], j["done"]);
                        responses.push_back(response);
                    } break;
                    default:
                        break;
                }
            }
        }
        
    }
    return responses;
}