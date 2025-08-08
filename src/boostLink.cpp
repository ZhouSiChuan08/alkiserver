#include "boostLink.h"

// 构造器
HttpQuestioning::HttpQuestioning(const std::string _host, const std::string _port, const std::string _target,const json _request_json)
: host(_host), port(_port), target(_target), request_json(_request_json),  resolver(ioc), socket(ioc)
{

}
// 提问器
std::shared_ptr<http::response_parser<http::string_body>> HttpQuestioning::ask()
{
    try
    {
        // **超时控制**
        // 创建定时器
        net::deadline_timer timer(ioc);
        timer.expires_from_now(boost::posix_time::seconds(std::chrono::seconds(timeout).count()));  // count()返回整型
        // 定时器超时回调
        timer.async_wait([this, &timer](const boost::system::error_code& ec) {
            if (!ec) {
                // 定时器超时
                timer.cancel();
                socket.cancel();  // 取消当前所有异步操作
                stdCerrInColor(1, "In HttpQuestioning::ask(), 连接到服务器超时 %d 秒\n", timeout);
                isTimeout = true;
            }
        });
        auto const results = resolver.resolve(host, port);     // 解析域名
        auto parser = std::make_shared<http::response_parser<http::string_body>>();  // 原类型禁用拷贝构造移动 因此使用智能指针
        //net::connect(socket, results.begin(), results.end());  // 连接服务器
        // **异步连接 链式异步调用**
        // 1 发起连接
        net::async_connect(socket, results,
            [this, &parser, &timer](beast::error_code ec, const tcp::endpoint& endpoint){
                if (ec)
                {
                    socket.cancel();
                    timer.cancel();
                    stdCerrInColor(1, "In HttpQuestioning::ask(), 连接失败: %s\n", ec.what().c_str());
                    isConnected = false;
                }
                else
                {
                    // 连接成功
                    http::request<http::string_body> req;     // 请求对象
                    isConnected = true;
                    if (request_json.empty())                 // 请求体为空 使用GET请求
                    {
                        req = {http::verb::get, target, 11};
                    }
                    else
                    {
                        req = {http::verb::post, target, 11};
                        req.body() = request_json.dump();     //设置请求体 将 JSON 对象转换为字符串形式
                    }
                    req.set(http::field::host, host);
                    req.set(http::field::user_agent, "Boost.Asio HTTP Client");
                    req.set(http::field::content_type, "application/json");         
                    stdCoutInColor(1, StringColor::YELLOW, "In HttpQuestioning::ask(), 请求体: %s\n", req.body().c_str());
                    req.prepare_payload();
                    // 2 发送请求体
                    http::async_write(socket, req,
                        [this, &parser, &timer](beast::error_code ec, std::size_t){
                            if (ec)
                            {
                                socket.cancel();
                                timer.cancel();
                                stdCerrInColor(1, "In HttpQuestioning::ask(), 发送请求失败: %s\n", ec.what().c_str());
                                isSendHTTPReq = false;
                            }
                            else
                            {
                                // 发送请求体成功
                                isSendHTTPReq = true;
                                // 3 读取响应头
                                http::async_read_header(socket, buffer, *parser,
                                [this, &timer](beast::error_code ec, std::size_t){
                                    if (ec)
                                    {
                                        socket.cancel();  // 取消套接字 隐式使用this指针
                                        timer.cancel();   // 取消定时器
                                        stdCerrInColor(1, "In HttpQuestioning::ask(), 读取响应头失败 %s\n", ec.what().c_str());
                                    }
                                    else
                                    {
                                        timer.cancel();  // 读取成功 取消定时器
                                    }
                                });
                            }
                    });
                }
        });
        ioc.run();  // 阻塞程序 驱动事件循环来执行队列中的任务直至全部完成
        if (!isConnected || !isSendHTTPReq || isTimeout )
        {
            return nullptr;
        }
        
        return parser;
        
    }
    catch(const std::exception& e)
    {
        std::cerr <<  "In HttpQuestioning::ask(), Error: " << e.what() << '\n';
        return nullptr;
    }
}
// 答案接收器和发送器
bool HttpQuestioning::receiveAndSend(std::shared_ptr<http::response_parser<http::string_body>> parser,zmq::socket_t &router, std::string dealer_id, const ResponseType& responseType)
{
    if (parser == nullptr)
    {
        return false;
    }
    else
    {
        try
        {
            size_t begin_index = 0;                           // 逐块读取响应体
            std::vector<Response> responses_;                 // 答案容器
            CmdRouter cmdRouter(DealerCMDMeta::CmdAddress);   // 命令路由器
            while (!parser->is_done()) {
                http::read_some(socket, buffer, *parser);
                // 解析已读取的内容为 JSON 对象或进行其他处理
                std::string body_chunk = parser->get().body();  // 返回已读的所有内容 body_chunk会逐渐累积
                stdCoutInColor(1, StringColor::BLUE, "In HttpQuestioning::receiveAndSend(), Received chunk: %s\n", body_chunk.c_str());
                // 这里假设返回的内容是 JSON 字符串流，你需要根据实际情况解析
                responses_ = parseResponse(body_chunk, responseType);          // body_chunk是增长式的,相应responses是增长式的 需要每次只发送新读取的内容到前端
                for (size_t i = begin_index; i < responses_.size(); i++)
                {
                    // 向Electron发送答案
                    zmq::message_t client_id(dealer_id);
                    zmq::message_t answer_msg(responses_[i].response);
                    router.send(client_id, zmq::send_flags::sndmore);
                    router.send(answer_msg, zmq::send_flags::none);
                }
                begin_index = responses_.size();
                if (cmdRouter.receive())
                {
                    if (cmdRouter.dealerID == DealerCMDMeta::name(DealerCMD::CMD_STOP_CHAT))
                    {
                        break;  // 收到停止聊天命令 退出循环
                    }  
                }  
            }
            stdCoutInColor(1, StringColor::GREEN, "\nIn HttpQuestioning::receiveAndSend(), All chunks received.\n");
            // 发送结束消息
            zmq::message_t client_id(dealer_id);
            zmq::message_t end_msg(std::string("END"));
            router.send(client_id, zmq::send_flags::sndmore);
            router.send(end_msg, zmq::send_flags::none);
            // 保存已接收响应
            responses = responses_;
            return true;
        }
        catch(const std::exception& e)
        {
            std::cerr <<  "In HttpQuestioning::receiveAndSend(), Error: " << e.what() << '\n';
            return false;
        }
    }
}
// 纯接收器
std::string HttpQuestioning::receive(std::shared_ptr<http::response_parser<http::string_body>> parser)
{
    std::string response;
    if (parser == nullptr)
    {
        stdCerrInColor(1, "In HttpQuestioning::receive(), 接收失败: 解析器为空\n");
        response = "";
    }
    else
    {
        try
        {
            while (!parser->is_done()) {
                http::read_some(socket, buffer, *parser);
                // 解析已读取的内容为 JSON 对象或进行其他处理
                std::string body_chunk = parser->get().body();  // 返回已读的所有内容 body_chunk会逐渐累积
                stdCoutInColor(1, StringColor::BLUE, "In HttpQuestioning::receive(), Received chunk: %s\n", body_chunk.c_str());
                // 这里假设返回的内容是 JSON 字符串流，你需要根据实际情况解析
                response = body_chunk;          // body_chunk是增长式的,每次都会在上一次的基础上增添新内容, 只需拿取最后一次的响应即可
            }
            stdCoutInColor(1, StringColor::GREEN, "\nIn HttpQuestioning::receive(), All chunks received.\n");
        }
        catch(const std::exception& e)
        {
            std::cerr <<  "In HttpQuestioning::receive(), Error: " << e.what() << '\n';
        }
    }
    return response;
}
// 关闭套接字
void HttpQuestioning::socketShutdown()
{
    // 关闭套接字
    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_both, ec);
    socket.close(ec);
}
// 析构器
HttpQuestioning::~HttpQuestioning()
{}

size_t writeCallbackString(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
    } catch (std::bad_alloc& ) {
        return 0; // 内存分配失败
    }
    return newLength;
}

size_t WriteCallbackFile(void* contents, size_t size, size_t nmemb, void* userdata) {
    std::ofstream* file = static_cast<std::ofstream*>(userdata);
    size_t dataSize = size * nmemb;
    
    if (file->is_open()) {
        file->write(static_cast<char*>(contents), dataSize);
        return dataSize;
    }
    return 0;
}

bool curlJsonRequest(const std::string& url, const CurlMethod& method, 
    const std::vector<std::string>& headers, const std::string& jsonBody,void* response, 
    curl_write_callback callback)
{
    bool isSuccess = false;
    // 初始化libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    // 初始化curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        stdCerrInColor(1, "In curlRequest, curl初始化失败!n");
        return false;
    }

    // 设置curl选项
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    switch (method) {
        case CurlMethod::GET:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        case CurlMethod::POST:
            {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonBody.length());
            }
            break;
        default:
            stdCerrInColor(1, "In curlRequest, 不支持的HTTP方法!n");
            return false;
            break;
    }

    // 设置请求头
    struct curl_slist* headersList = nullptr;
    for (const auto& header : headers) {
        headersList = curl_slist_append(headersList, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);

    // 设置流式响应处理
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    isSuccess = (res == CURLE_OK);

    // 检查HTTP响应码
    if (isSuccess) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        if (httpCode != 200) {
            stdCerrInColor(1, "In curlRequest, HTTP错误! HTTP响应码: %d!\n", httpCode);
            isSuccess = false;
        }
    } else {
        stdCerrInColor(1, "In curlRequest, 请求失败! 错误信息: %s!\n", curl_easy_strerror(res));
    }
    // 清理资源
    curl_slist_free_all(headersList);
    curl_easy_cleanup(curl);
    // 清理全局资源
    curl_global_cleanup();
    return isSuccess;
}

bool curlFormRequest(const std::string& url, const std::vector<std::string>& headers, 
    const std::vector<std::pair<std::string, std::string>>& formTextPair ,const std::string& filePath, void* response, 
    curl_write_callback callback)
{
    bool isSuccess = false;
    // 初始化libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    // 初始化curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        stdCerrInColor(1, "In curlRequest, curl初始化失败!n");
        return false;
    }

    // 初始化表单数据
    curl_mime* form = curl_mime_init(curl);
    if (!form) {
        curl_easy_cleanup(curl);
        stdCerrInColor(1, "In curlFormRequest, curl_mime_init失败!n");  
        return false;
    }

    // 添加文本字段
    for (const auto& textPair : formTextPair) {
        curl_mimepart* part = curl_mime_addpart(form);
        curl_mime_name(part, textPair.first.c_str());
        curl_mime_data(part, textPair.second.c_str(), CURL_ZERO_TERMINATED);
    }
    // 添加文件字段
    curl_mimepart* filePart = curl_mime_addpart(form);
    curl_mime_name(filePart, "file");
    curl_mime_filedata(filePart, filePath.c_str()); // 自动处理文件二进制数据

    // 设置请求参数
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form); // 自动构造multipart/form-data
    // 设置请求头
    struct curl_slist* headersList = nullptr;
    for (const auto& header : headers) {
        headersList = curl_slist_append(headersList, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);

    // 设置流式响应处理
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    isSuccess = (res == CURLE_OK);

    // 检查HTTP响应码
    if (isSuccess) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        if (httpCode != 200) {
            stdCerrInColor(1, "In curlRequest, HTTP错误! HTTP响应码: %d!\n", httpCode);
            isSuccess = false;
        }
    } else {
        stdCerrInColor(1, "In curlRequest, 请求失败! 错误信息: %s!\n", curl_easy_strerror(res));
    }
    // 清理资源
    curl_slist_free_all(headersList);
    curl_mime_free(form);
    curl_easy_cleanup(curl);
    // 清理全局资源
    curl_global_cleanup();
    return isSuccess;
}

SiliVoiceService::SiliVoiceService(const std::string& apiKey_) : apiKey(apiKey_)
{}

std::tuple<int, int, int> SiliVoiceService::countChars(const std::string& str)
{
    int chineseCount = 0;
    int englishCount = 0;
    int otherCount = 0;
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        if (c <= 0x7F) {  // 英文字符（ASCII）
            englishCount++;
            i += 1;
        } else if (c >= 0xE0 && c <= 0xEF) {  // 中文字符（常见范围）
            chineseCount++;
            i += 3;  // 跳过 3 个字节
        } else {
            // 其他情况（如特殊符号或生僻字）
            otherCount++;
            i += 1;
        }
    }
    return {chineseCount, englishCount, otherCount};
}

std::string SiliVoiceService::cloneVoice(const std::string& audioFilePath, const std::string& spkLines)
{
    std::string voiceId;
    const std::string url = "https://api.siliconflow.cn/v1/uploads/audio/voice";
    const std::string model = "FunAudioLLM/CosyVoice2-0.5B";
    const std::string customName = "spk";

    std::string response;
    bool isSuccess = curlFormRequest(url, std::vector<std::string>{
            "Authorization: Bearer " + apiKey,
        },
        std::vector<std::pair<std::string, std::string>>{
            {"model", model},
            {"customName", customName},
            {"text", spkLines}
        },
        audioFilePath,
        &response,
        reinterpret_cast<curl_write_callback>(writeCallbackString)  // 显示转换 自行关注合法性
    );
    if (isSuccess)
    {
        stdCoutInColor(1,StringColor::GREEN, "In SiliVoiceService::cloneVoice, 音色克隆成功! 响应内容: %s!\n", response.c_str());
        try
        {
            json res = json::parse(response);
            if (res.contains("uri"))
            {
                voiceId = res["uri"].get<std::string>();
            }
            else
            {
                stdCerrInColor(1, "In SiliVoiceService::cloneVoice, 响应内容中没有uri字段! 响应内容: %s!\n", response.c_str());
            }
        }
        catch(const std::exception& e)
        {
            stdCerrInColor(1, "In SiliVoiceService::cloneVoice, Json解析响应内容失败! 错误信息: %s! 响应内容: %s!\n", e.what(), response.c_str());
        }
    }
    else
    {
        stdCerrInColor(1, "In SiliVoiceService::cloneVoice, 音色克隆失败! 响应内容: %s!\n", response.c_str());
    }
    return   voiceId;
}

bool SiliVoiceService::TTS(const std::string& voiceId, const std::string& text, const std::string& mp3FilePath)
{
    // 打开输出文件（二进制模式）
    std::ofstream outputFile(mp3FilePath, std::ios::binary | std::ios::trunc);
    if (!outputFile.is_open()) {
        stdCerrInColor(1, "In SiliVoiceService::TTS, 打开输出文件失败! 文件路径: %s!\n", mp3FilePath.c_str());
        return false;
    }
    const std::string url = "https://api.siliconflow.cn/v1/audio/speech";
    const std::string model = "FunAudioLLM/CosyVoice2-0.5B";
    json requestJson = {
        {"model", model},
        {"voice", voiceId},
        {"input", text},
        {"response_format", "mp3"}
    };
    std::string requestBody = requestJson.dump();
    bool isSuccess = curlJsonRequest(url, CurlMethod::POST, std::vector<std::string>{
            "Authorization: Bearer " + apiKey,
            "Content-Type: application/json"
        },
        requestBody,
        &outputFile,
        reinterpret_cast<curl_write_callback>(WriteCallbackFile)  // 写入文件
    );

    return isSuccess;
}

std::vector<std::string> SiliVoiceService::queryVoiceList()
{
    std::vector<std::string> voiceList;
    std::string response;
    const std::string url = "https://api.siliconflow.cn/v1/audio/voice/list";
    bool isSuccess = curlJsonRequest(url, CurlMethod::GET, std::vector<std::string>{
            "Authorization: Bearer " + apiKey,
        },
        "",
        &response,
        reinterpret_cast<curl_write_callback>(writeCallbackString)  // 显示转换 自行关注合法性
    );
    if (isSuccess)
    {
        try
        {
            json res = json::parse(response);
            if (res.contains("result"))
            {
                json resultList = res["result"];
                for (const auto& voice : resultList)
                {
                    voiceList.push_back(voice["uri"].get<std::string>());
                }
                stdCoutInColor(1,StringColor::GREEN, "In SiliVoiceService::queryVoiceList, 查询音色列表成功! 共有音色: %d 个!\n", resultList.size());
            }
            else
            {
                stdCerrInColor(1, "In SiliVoiceService::queryVoiceList, 响应内容中没有result字段! 响应内容: %s!\n", response.c_str());
            }
        }
        catch(const std::exception& e)
        {
            stdCerrInColor(1, "In SiliVoiceService::queryVoiceList, Json解析响应内容失败! 错误信息: %s! 响应内容: %s!\n", e.what(), response.c_str());
        }
        
    }
    else
    {
        stdCerrInColor(1, "In SiliVoiceService::queryVoiceList, 查询语音列表失败! 响应内容: %s!\n", response.c_str());
    }
    return voiceList;
}

bool SiliVoiceService::deleteVoice(const std::string& voiceId)
{
    const std::string url = "https://api.siliconflow.cn/v1/audio/voice/deletions";
    std::string response;
    json requestJson = {
        {"uri", voiceId}
    };
    std::string requestBody = requestJson.dump();
    bool isSuccess = curlJsonRequest(url, CurlMethod::POST, std::vector<std::string>{
            "Authorization: Bearer " + apiKey,
            "Content-Type: application/json"
        
        },
        requestBody,
        &response,
        reinterpret_cast<curl_write_callback>(writeCallbackString)  // 显示转换 自行关注合法性
    );
    if (isSuccess)
    {
        stdCoutInColor(1,StringColor::GREEN, "In SiliVoiceService::deleteVoice, 删除语音成功! 响应内容: %s!\n", response.c_str());
    }
    else
    {
        stdCerrInColor(1, "In SiliVoiceService::deleteVoice, 删除语音失败! 响应内容: %s!\n", response.c_str());
    }
    
    return isSuccess;
}
