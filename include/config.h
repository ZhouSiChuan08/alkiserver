#ifndef CONFIG_H
#define CONFIG_H
// 本文件负责配置文件的读取,实际大量字符串常量的定义
#include <nlohmann/json.hpp>
#include <fstream>
#include "debugTools.h"
#include "boostLink.h"

using json = nlohmann::json;
// 配置文件路径
namespace Config {
    inline constexpr const std::string_view configPath = "config.json";
    inline constexpr const std::string_view generateDir = "../../config/";
}
// 配置读取名称标志化 使用枚举类型
enum class ConfigKeys { DatabasePath, ModelSelected, MaxMemoryLength, SenseVoiceModelSelected, SenseVoiceLanguage, SenseVoiceNumThreads, CloudServiceApi,
    UseMotion, OtherPrompt
};
// 配置名称消息元数据
struct ConfigKeysMeta {
    // 获取配置键的静态名称
    static std::string name(ConfigKeys key) {
        switch(key) {
            case ConfigKeys::DatabasePath: return "DatabasePath";
            case ConfigKeys::ModelSelected: return "ModelSelected";
            case ConfigKeys::MaxMemoryLength: return "MaxMemoryLength";
            case ConfigKeys::SenseVoiceModelSelected: return "SenseVoiceModelSelected";
            case ConfigKeys::SenseVoiceLanguage: return "SenseVoiceLanguage";
            case ConfigKeys::SenseVoiceNumThreads: return "SenseVoiceNumThreads";
            case ConfigKeys::CloudServiceApi: return "CloudServiceApi";
            case ConfigKeys::UseMotion: return "UseMotion";
            case ConfigKeys::OtherPrompt: return "OtherPrompt";
            default: throw std::invalid_argument("Unknown ConfigKeys");
        }
    }
};

/**
 * @brief 读取配置文件中的值
 * @param key 要读取的键值
 * @return 读取到的json对象 需手动指定类型解析
 */
json LoadValueFromConfig(const std::string& key);

/**
 * @brief 获取olamap模型列表
 * @return 模型列表的json对象
 */
json GetLocalModels();

/**
 * @brief 获取配置文件的所有值
 * @return 配置文件的所有值 json对象格式
 */
json GetTotalConfig();

/**
 * @brief 更新配置文件
 * @return 是否成功更新
 */
bool UpdateConfig(const json& newConfig);
#endif // CONFIG_H