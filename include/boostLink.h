#ifndef BOOSTLINK_H
#define BOOSTLINK_H

#include <iostream>
#include <string>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <curl/curl.h>
#include "dataType.h"
#include "debugTools.h"
#include "cmdLink.h"
#include "toolFun.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>  命名空间的别名
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace websocket = beast::websocket; // WebSocket 类型别名
namespace ssl = net::ssl;               // SSL/TLS
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>  类型的别名

using json = nlohmann::json;            // from <nlohmann/json.hpp>  类型的别名
/**
 * @brief 使用http协议向模型后台发送问题
 */
class HttpQuestioning {
// 成员函数
public:
    // 构造器 
    HttpQuestioning(const std::string _host, const std::string _port, const std::string _target, const json _request_json);

    // 提问器
    /**
     * @brief 提问器 使用http协议向模型后台发送问题
     * @return 返回响应头
     */
    std::shared_ptr<http::response_parser<http::string_body>> ask();

    /**
     * @brief 获取响应头 流式接收响应并向前端发送响应
     * @param parser 响应头解析器的智能指针
     * @return bool 是否接收完毕
     */
    bool receiveAndSend(std::shared_ptr<http::response_parser<http::string_body>> parser,zmq::socket_t &router, std::string dealer_id, const ResponseType& responseType);

    /**
     * @brief 纯接收响应 返回json字符串
     * @param parser 响应头解析器的智能指针
     * @return json字符串
     */
    std::string receive(std::shared_ptr<http::response_parser<http::string_body>> parser);

    /**
     * @brief 一次提问 接受答案 发送到前端完毕 关闭套接字
     */
    void socketShutdown();

    // 析构器
    ~HttpQuestioning();

    std::vector<Response> responses;  // 接收到的响应

// 成员变量
private:
    std::string host;                 // 模型后台地址
    std::string port;                 // 模型后台端口
    std::string target;               // 模型后台接口

    net::io_context ioc;              // 异步IO上下文
    tcp::resolver resolver;           // 域名解析器
    tcp::socket socket;               // 套接字
    beast::flat_buffer buffer;        // 接收响应的缓冲区 因为buffer被read_header调用后
                                      // 会残留一些数据，为接收到完整消息，所以改为成员变量，共享给两个函数ask、receiveAndSen
    bool isConnected   = false;       // 是否连接成功
    bool isSendHTTPReq = false;       // 是否成功发送http请求
    bool isTimeout     = false;       // 是否超时
    int  timeout       = 10;          // 超时时间 单位秒
    
public:
    json request_json;  // json请求体

};

enum class CurlMethod {
    GET,
    POST
};

/**
 * @brief 回调函数：接收服务器响应数据 仅处理std::string 响应
 */
size_t writeCallbackString(void* contents, size_t size, size_t nmemb, std::string* s);

/**
 * @brief 回调函数：接收服务器响应数据 处理文件流响应
 */
size_t WriteCallbackFile(void* contents, size_t size, size_t nmemb, void* userdata);

/**
 * @brief 普通的curl请求 请求头 + json boody 自行决定如何处理response
 * @param url 请求url
 * @param method 请求方法
 * @param headers 请求头
 * @param body 请求body
 * @param response 响应数据
 * @param callback 回调函数
 * @return bool 是否成功
 */
bool curlJsonRequest(const std::string& url, const CurlMethod& method, 
    const std::vector<std::string>& headers, const std::string& jsonBody,void* response, 
    curl_write_callback callback);

/**
 * @brief 带上传文件的curl请求 请求头 + form data 自行决定如何处理response
 * @param url 请求url
 * @param headers 请求头
 * @param formTextPair form data的键值对
 * @param filePath 上传文件路径
 * @param response 响应数据
 * @param callback 回调函数
 * @return bool 是否成功
 */
bool curlFormRequest(const std::string& url, const std::vector<std::string>& headers, 
    const std::vector<std::pair<std::string, std::string>>& formTextPair ,const std::string& filePath, void* response, 
    curl_write_callback callback);

class SiliVoiceService
{
public:
    SiliVoiceService(const std::string& apiKey_);

    /**
     * @brief 统计字符串中英文, 中文字符的个数
     * @param str 字符串
     * @return 一个tuple, 第一个元素是中文字符的个数, 第二个元素是英文字符的个数, 第三个元素是其他字符的个数
     */
    std::tuple<int, int, int> countChars(const std::string& str);

    /**
     * @brief 克隆音色接口
     * @param audioFilePath 文件路径
     * @param spkLines 对应台词
     * @return 克隆后的音色id
     */
    std::string cloneVoice(const std::string& audioFilePath, const std::string& spkLines);

    /**
     * @brief 合成音频接口
     * @param voiceId 音色id
     * @param text 待合成文本
     * @param mp3FilePath 合成后的音频文件路径
     * @return bool 是否成功
     */
    bool TTS(const std::string& voiceId, const std::string& text, const std::string& mp3FilePath);

    /**
     * @brief 查询音色列表
     * @return std::vector<std::string> 音色列表
     */
    std::vector<std::string> queryVoiceList();

    /**
     * @brief 删除指定音色
     * @return bool 是否成功
     */
    bool deleteVoice(const std::string& voiceId);

private:
    std::string apiKey;
};

#endif // BOOSTLINK_H