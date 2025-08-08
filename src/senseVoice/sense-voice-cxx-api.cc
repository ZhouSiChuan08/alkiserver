#include "senseVoice/sense-voice-cxx-api.h"

SenseVoice::SenseVoice(const std::string& modelName_, const std::string& lang_, const size_t numThreads_ ) : modelName(modelName_),
    lang(lang_), numThreads(numThreads_) 
{
    // 模型配置
    OfflineRecognizerConfig config;
    std::filesystem::path modelPath = std::filesystem::path("sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17") / modelName;
    config.model_config.sense_voice.model = modelPath.string();
    config.model_config.sense_voice.use_itn = true;
    config.model_config.sense_voice.language = lang;
    config.model_config.tokens =
        "./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/tokens.txt";
    
    // 模型初始化
    recognizer = std::make_unique<OfflineRecognizer>(OfflineRecognizer::Create(config));
    if (!recognizer->Get()) 
    {
        stdCerrInColor(1, "In SenseVoice::SenseVoice(), SenseVoice 模型初始化失败！ 程序已退出\n");
        exit(1);
    }
    else
    {
        stdCoutInColor(1, StringColor::GREEN, "In SenseVoice::SenseVoice(), SenseVoice 模型初始化成功！\n");
    }
}

bool SenseVoice::inference(const std::string& wavPath)
{
    bool isSuccess = true;
    Wave wave = ReadWave(wavPath);
    if (wave.samples.empty()) 
    {
        isSuccess = false;
        stdCerrInColor(1, "In SenseVoice::inference(), 读取wav音频文件失败!\n");
        return isSuccess;
    }
    stdCoutInColor(1, StringColor::YELLOW, "In SenseVoice::inference(), 开始识别...\n");

    const auto begin = std::chrono::steady_clock::now();

    OfflineStream stream = recognizer->CreateStream();
    stream.AcceptWaveform(wave.sample_rate, wave.samples.data(),
                            wave.samples.size());

    recognizer->Decode(&stream);

    OfflineRecognizerResult result = recognizer->GetResult(&stream);

    // 获取结果
    jsonStr.clear();
    jsonStr = result.json;

    const auto end = std::chrono::steady_clock::now();
    const float elapsed_seconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin)
            .count() /
        1000.;
    float duration = wave.samples.size() / static_cast<float>(wave.sample_rate);
    float rtf = elapsed_seconds / duration;
    stdCoutInColor(1, StringColor::BLUE, "In SenseVoice::inference(), 线程数 %d 时长 %.3fs 耗时 %.3fs 单字符耗时 RTF = %.3f / %.3f = %.3f\n, 识别结果:%s\n", 
        numThreads, duration, elapsed_seconds, elapsed_seconds, duration, rtf, result.json.c_str());
    return isSuccess;
}

bool SenseVoice::isNeedReload(const std::string& configModelName, const std::string& configLang, const size_t configNumThreads)
{
    if (configModelName != modelName || configLang != lang || configNumThreads != numThreads)
    {
        return true;
    }
    return false;
}