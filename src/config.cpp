#include "config.h"

json LoadValueFromConfig(const std::string& key) 
{
    std::ifstream file(Config::configPath.data()); // std::string(Config::configPath)
    json config;
    try
    {
        file >> config;
        return config[key];
    }
    catch(const std::exception& e)
    {
        stdCerrInColor(1, "In LoadValueFromConfig, 解析配置文件变量 %s 出错, 主动退出程序: ",  key.c_str(), e.what());
        exit(1);
    }
}

json GetLocalModels()
{
    std::string const host   = "localhost";
    std::string const port   = "11434";
    std::string const target = "/api/tags";
    json request_json;
    HttpQuestioning httpQuestioning(host, port, target, request_json);    // 初始化
    // 提问并接收回答
    std::string response = httpQuestioning.receive(httpQuestioning.ask());
    if (response.empty())
    {
        httpQuestioning.socketShutdown();  // 关闭套接字
        stdCerrInColor(1, "In ListLocalModels, 获取本地模型列表为空.\n");
    }
    else
    {
        httpQuestioning.socketShutdown();  // 关闭套接字
    }
    try
    {
        request_json = json::parse(response);        
    }
    catch(const std::exception& e)
    {
        stdCerrInColor(1, "In ListLocalModels, json解析出错: ", e.what());
        throw e;
    }
    return request_json;
}

json GetTotalConfig()
{
    json config;
    // 读取配置文件内容到输入流
    std::ifstream ifs(Config::configPath.data());  //std::string(Config::configPath)
    if (!ifs.is_open())
    {
        stdCerrInColor(1, "In GetTotalConfig, 读取配置文件失败, 主动退出程序: ", Config::configPath);
        exit(1);
    }
    config = json::parse(ifs);
    ifs.close();
    return config;
}

bool UpdateConfig(const json& newConfig)
{
    bool isSuccess = true;
    try
    {
        json oldConfig = GetTotalConfig();
        oldConfig.update(newConfig);        // 合并更新json对象
        // 创建一个临时文件
        const std::string tempFilePath =  std::string(Config::configPath)+std::string(".temp");
        std::ofstream ofs(tempFilePath);
        ofs << oldConfig.dump(4);           // 写入json字符串
        ofs.close();
        // 原子性 替换文件 最终只会留下一个文件
        std::filesystem::rename(tempFilePath, std::string(Config::configPath));
        isSuccess = true;
    }
    catch(const std::exception& e)
    {
        stdCerrInColor(1, "In UpdateConfig, 更新配置文件失败: ", e.what());
        isSuccess = false;
    }
    return isSuccess;
}

 