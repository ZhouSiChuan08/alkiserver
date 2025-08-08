#ifndef SENSE_VOICE_CXX_API_H
#define SENSE_VOICE_CXX_API_H
#include <chrono>  // NOLINT
#include <iostream>
#include <string>
#include <filesystem>

#include "senseVoice/cxx-api.h"
#include "debugTools.h"

using namespace sherpa_onnx::cxx;

class SenseVoice {
public:
    SenseVoice(const std::string& modelName_, const std::string& lang_ = "auto", const size_t numThreads_ = 1);

    /**
     * @brief ASR推理
     * @param wavPath 音频文件路径
     * @return 推理是否成功
     */
    bool inference(const std::string& wavPath);

    /**
     * @brief 重新加载模型
     * @param configModelName 配置模型名称
     * @param configLang 配置语言
     * @param configNumThreads 配置线程数
     * @return 是否需要重新加载
     */
    bool isNeedReload(const std::string& configModelName, const std::string& configLang, const size_t configNumThreads);

    std::string jsonStr;  // 输出结果, 格式为json字符串

private:
    std::unique_ptr<OfflineRecognizer> recognizer;
    size_t numThreads;
    std::string modelName;
    std::string lang;
    
};

#endif  // SENSE_VOICE_CXX_API_H