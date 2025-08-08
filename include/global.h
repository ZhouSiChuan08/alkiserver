#ifndef GLOBAL_H
#define GLOBAL_H
#include <iostream>
#include <iomanip>  //设置输出精度
#include <fstream>  //写入文件
#include <string>
#include <vector>
#include <Eigen/Dense>

#include "tic_toc.h"
//全局常量宏定义
#define TRACESIZE 250
//声明外部引用参数
//main.cpp
extern int  global_argc;
extern char **global_argv;
extern TicToc timer;  //记录程序开始时间
extern std::string LogFile;
extern std::ofstream foutC;
extern std::vector<std::ofstream*> fouts;

//zoptions.cpp
extern int speed_GNSS;
extern int speed_SSR;
extern std::string file_GNSS;
extern std::string file_SSR;
extern std::string COM_GNSS;
extern std::string COM_SSR;

//声明外部引用函数 及 模板函数
//ztrace.cpp
bool ztrace(const int level, const std::string& format);
template<typename... Args>
bool ztrace(const int level, const std::string& format, Args... args)
{
    if (global_argc >= 2)
    {
        if (level == atoi(global_argv[global_argc-1]))
        {
            //开始打印
            char buffer[TRACESIZE];
            snprintf(buffer, sizeof(buffer), format.c_str(), args...);  //格式化写入
            std::cout << buffer;  
        }
    }
    return true;
}
//ztracemat 打印Eigen矩阵到终端
/**
 * @brief 在终端打印Eigen矩阵
 * @param 消息级别 消息提示 Eigen矩阵 打印精度(默认小数点后4位)
 * @return 成功调用返回true
 */
template<typename T>
bool ztracemat(const int level, const std::string& format, const T& matrix, const int precision = 4, 
            const bool enablescientific = false) 
{
    if (global_argc >= 2)
    {
        if (level == atoi(global_argv[global_argc-1]))
        {
            //开始打印
            char buffer[TRACESIZE];
            snprintf(buffer, sizeof(buffer), format.c_str());  //格式化写入
            std::cout << buffer;
            // 定义打印格式：精度为4，不显示科学计数法，元素之间用逗号分隔，行之间用换行符分隔
            Eigen::IOFormat PrintFormat(precision, Eigen::DontAlignCols, ", ", "\n", "[", "]");
            // 使用定义的格式打印矩阵
            if (enablescientific == false)
            {
                std::cout << std::fixed << std::setprecision(precision);
            }
            else
            {
                std::cout << std::scientific << std::setprecision(precision);
            }
            std::cout << matrix.format(PrintFormat) << std::endl;  
        }
    }
    return true;
}
//ztracet.cpp
bool ztracet(const int level, const std::string& format);  //无参数列表的情况
template<typename... Args>
bool ztracet(const int level, const std::string& format, Args... args)
{
    if (global_argc >= 2)
    {
        if (level == atoi(global_argv[global_argc-1]))
        {
            std::string filePath = LogFile;
            if (!foutC.is_open())
            {
                foutC.open(filePath, std::ios::app);  //打开logPath,std::ios::ate标志位表示文件打开后定位到文件末尾
                if (!foutC.is_open()) 
                {
                    std::cerr << "无法打开文件：" << filePath << std::endl;
                    return false;
                }   
            }
            //开始写入
            char buffer[TRACESIZE];
            snprintf(buffer, sizeof(buffer), format.c_str(), args...);  //格式化写入
            foutC << buffer;
        }
    }
    return true;
}
bool zlog(std::ofstream& foutC, std::string LogFile, const int level, const std::string& format);  //无参数列表的情况
template<typename... Args>
bool zlog(std::ofstream& foutC, std::string LogFile, const int level, const std::string& format, Args... args)  //有参数列表的情况
{
    // 查找文件句柄的循环
    bool found = false;
    for (std::ofstream* fout : fouts) 
    {
        if (fout == &foutC) 
        {
            found = true;
            break;
        }
    }
    // 如果没有找到，添加到容器中
    if (!found) 
    {
        fouts.push_back(&foutC);  ///保存文件句柄
    }
    if (global_argc >= 2)
    {
        if (level == atoi(global_argv[global_argc-1]))
        {
            std::string filePath = LogFile;
            if (!foutC.is_open())
            {
                foutC.open(filePath, std::ios::app);  //打开logPath,std::ios::ate标志位表示文件打开后定位到文件末尾
                if (!foutC.is_open()) 
                {
                    std::cerr << "无法打开文件：" << filePath << std::endl;
                    return false;
                }   
            }
            //开始写入
            char buffer[TRACESIZE];
            snprintf(buffer, sizeof(buffer), format.c_str(), args...);  //格式化写入
            foutC << buffer;
        }
    }
    return true;
}
//ztracetmat 打印Eigen矩阵到文件
/**
 * @brief 在文件打印Eigen矩阵
 * @param 消息级别 消息提示 Eigen矩阵 打印精度(默认小数点后4位)
 * @return 成功调用返回true
 */
template<typename T>
bool ztracetmat(const int level, const std::string& format, const T& matrix, const int precision = 4,
            const bool enablescientific = false) 
{
    if (global_argc >= 2)
    {
        if (level == atoi(global_argv[global_argc-1]))
        {
            std::string filePath = LogFile;
            if (!foutC.is_open())
            {
                foutC.open(filePath, std::ios::app);  //打开logPath,std::ios::ate标志位表示文件打开后定位到文件末尾
                if (!foutC.is_open()) 
                {
                    std::cerr << "无法打开文件：" << filePath << std::endl;
                    return false;
                }   
            }
            //开始写入
            char buffer[TRACESIZE];
            snprintf(buffer, sizeof(buffer), format.c_str());  //格式化写入
            foutC << buffer;
            // 定义打印格式：精度为4，不显示科学计数法，元素之间用逗号分隔，行之间用换行符分隔
            Eigen::IOFormat PrintFormat(precision, Eigen::DontAlignCols, ", ", "\n", "[", "]");
            // 使用定义的格式打印矩阵
            if (enablescientific == false)
            {
                foutC << std::fixed << std::setprecision(precision);
            }
            else
            {
                foutC << std::scientific << std::setprecision(precision);
            }
            foutC << matrix.format(PrintFormat) << std::endl;  
        }
    }
    return true;
}
//zoptions.cpp
bool zoptions();
//zquit.cpp
void zquit();
void zblock();
//file.cpp
std::vector<std::string> splitStr(const std::string& str, const char delimiter);  //分割字符串
std::string trim(const std::string& str);  // 去除字符串两端空格

#endif  //GLOBAL_H