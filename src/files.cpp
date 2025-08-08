//getline 按行读取文件流 ifstream， 字符串流 ifstream  直到无更多内容可读
#include <fstream> // 包含ifstream的头文件
#include <sstream> // 包含stringstream的头文件
#include <vector>  // 包含vector的头文件
#include <string>  // 包含string的头文件
#include <iostream> // 包含iostream的头文件，用于输入输出
#include "global.h" // 包含全局变量的头文件

/**
 * @brief 按行读取字符串流 并根据指定分隔符分割字符串
 * @param str 字符串流 类型：istringstream 适合读取 ostringstream 适合写入 stringstream 常用于同时需要读和写操作的场景
 * @param delimiter 分隔符
 * @return 包含所有子字符串的vector
 */
std::vector<std::string> splitStr(const std::string& str, const char delimiter)  //
{
    std::vector<std::string> result; // 定义一个空的vector
    std::istringstream iss(str); // 定义一个字符串流对象
    std::string item; // 定义一个空字符串变量
    // 使用while循环和std::getline函数来读取iss中的内容
    while (std::getline(iss, item, delimiter)) 
    {
        result.push_back(item); // 如果读取成功，将得到的子字符串添加到result向量中
    }
    return result; // 返回包含所有子字符串的向量
}
/**
 * @brief 去除两端空白字符
 * @param str 字符串
 * @return 去除两端空白字符的字符串
 */
std::string trim(const std::string& str) 
{
    std::string result = str;
    // 去除开头的空白字符
    result.erase(0, result.find_first_not_of(" \t\n\r\f\v"));
    // 去除结尾的空白字符
    result.erase(result.find_last_not_of(" \t\n\r\f\v") + 1);
    return result;
}

/**
 * @brief 统计字符串中英文和中文字符的个数
 * @param str 字符串
 * @return 一个pair，第一个元素是中文字符的个数，第二个元素是英文字符的个数
 */
std::pair<int, int> count_chars(const std::string& str) {
    int chinese_count = 0;
    int english_count = 0;
    
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        if (c <= 0x7F) {  // 英文字符（ASCII）
            english_count++;
            i += 1;
        } else if (c >= 0xE0 && c <= 0xEF) {  // 中文字符（常见范围）
            chinese_count++;
            i += 3;  // 跳过 3 个字节
        } else {
            // 其他情况（如特殊符号或生僻字）
            i += 1;
        }
    }
    
    return {chinese_count, english_count};
}

