cmake_minimum_required(VERSION 3.12.0)
# 精确控制CMake对MSVC运行库的设置方式, 可以用 MSVC_RUNTIME_LIBRARY 选项来控制
if (POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif (POLICY CMP0091)

project(alkiserver VERSION 0.1.0 LANGUAGES C CXX)

# include(CTest) # 启动C测试程序 目前没用
# enable_testing()
# 设置C++标准
# set(CMAKE_CXX_FLAGS "-std=c++17") # g++编译器设置C++标准
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")   # cl编译器设置C++标准
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8") # cl编译器设置编码格式
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive") # 使用不严谨的C++语法检查
# add_definitions(-ENACMP) # 添加宏定义 旧方法 有-D前缀
# add_definitions(-DZMQ_STATIC)
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /W0")   # cl编译器设置utf-8编码
  add_compile_options(/Zc:__cplusplus)         # 强制 MSVC 正确报告 __cplusplus
  add_compile_definitions(_WIN32_WINNT=0x0A00) # 制定最低兼容的Windows版本 Windows 10
  # add_link_options(/VERBOSE:LIB)               # 启用链接器的详细日志（包含所有链接的库）
  # 设置所有后续目标的默认运行时库 因为sensevoice必须使用MT
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  # 第三方库相关
  add_compile_definitions(SQLITECPP_COMPILE_DLL)  # 开启Win平台的DLL符号导入 避免重定义 否则.h中会默认生成一些SQLiteCpp的析构器,导致重定义
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 强制要求编译器支持该标准
message("Default C++ Standard: ${CMAKE_CXX_STANDARD}")
message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}") # 查看当前C++编译器设置
message("CMake version: ${CMAKE_VERSION}") # 查看当前Cmake版本

# 添加预编译指令
# add_compile_definitions(ENACMP)  宏定义 新方法
# add_compile_definitions(ZMQ_STATIC)

# 设置可执行文件的输出目录为项目根目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

# 设置库路径
set(VCPKG_X64-windows "F:/Zlab/C++/vcpkg/installed/x64-windows") # x64-mingw-static
set(VCPKG_X64-LINUX "/media/root/908E32BF8E329E22/Zlab/C++/vcpkg/installed/x64-linux")
set(MYLIB "F:/Zlab/C++/mylib/windows-msvc")
message(STATUS "***VCPKG_X64-windows***: ${VCPKG_X64-windows}")
message(STATUS "***VCPKG_X64-LINUX***  : ${VCPKG_X64-LINUX}")
message(STATUS "          ***MYLIB***  : ${MYLIB}")

# 设置CMake查找前缀路径
set(CMAKE_PREFIX_PATH "${VCPKG_X64-windows};${VCPKG_X64-LINUX};${MYLIB}")
# 设置CMake查找模块路径
set(CMAKE_MODULE_PATH "${VCPKG_X64-windows}/share;${VCPKG_X64-LINUX}/share;${MYLIB}/share")
# 设置CMake查找库路径
set(CMAKE_LIBRARY_PATH "${VCPKG_X64-windows}/lib;${VCPKG_X64-LINUX}/lib;${MYLIB}/lib")




# 设置include目录
include_directories(
  include
  ${VCPKG_X64-windows}/include
  ${VCPKG_X64-LINUX}/include
  ${MYLIB}/include
)


# 搜索当前目录下的所有.cpp文件 并将地址存储到变量SRC_LIST
# aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)
# message(STATUS "<<Project source directory>>: ${PROJECT_SOURCE_DIR}/src")

# 首先创建一个接口库来封装sensevoice的所有依赖
add_library(sensevoice_deps INTERFACE IMPORTED)

# 设置依赖库的目录（仅对这个接口库可见）
target_link_directories(sensevoice_deps INTERFACE ${MYLIB}/lib/sensevoice)

# 添加所有依赖库到接口库
target_link_libraries(sensevoice_deps INTERFACE
  debugTools
  sherpa-onnx-cxx-api
  sherpa-onnx-c-api
  sherpa-onnx-core
  kaldi-native-fbank-core
  kissfft-float
  kaldi-decoder-core
  sherpa-onnx-kaldifst-core
  ssentencepiece_core
  onnxruntime
  sherpa-onnx-fstfar
  sherpa-onnx-fst
  piper_phonemize
  espeak-ng
  ucd
)

# 然后创建实际的sensevoice库
add_library(sensevoice ./src/senseVoice/sense-voice-cxx-api.cc)

# 链接接口库到sensevoice（使用PRIVATE确保依赖不会传递）
target_link_libraries(sensevoice PRIVATE sensevoice_deps)


# 准备静态库
add_library(dataType ./src/dataType.cpp)
add_library(zmqLink ./src/zmqLink.cpp)
add_library(boostLink ./src/boostLink.cpp)
add_library(taskManager ./src/taskManager.cpp)
add_library(debugTools ./src/debugTools.cpp)
add_library(dataBase ./src/dataBase.cpp)
add_library(config ./src/config.cpp)
add_library(cmdLink ./src/cmdLink.cpp)
add_library(toolFun ./src/toolFun.cpp)
# 定义目标名称变量
set(TARGET_NAME alkiserver)
# 生成可执行文件
add_executable(
  ${TARGET_NAME} src/alkiserver.cpp
)

# 添加其他静态库目录
target_link_directories(${TARGET_NAME} PRIVATE ${VCPKG_X64-windows}/lib ${VCPKG_X64-LINUX}/lib ${MYLIB}/lib )  #单个执行文件链接库目录
#link_directories(${VCPKG_LIB} ${MYLIB_LIB})  #全局链接库目录 不推荐


# 找到并链接 指定组件
# find_package(cppzmq CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# 越基础的库位置放后
target_link_libraries(${TARGET_NAME} PRIVATE
  taskManager
  sensevoice
  sensevoice_deps
  dataBase
  zmqLink
  boostLink
  cmdLink
  dataType
  config
  toolFun
  debugTools
  libzmq-mt-4_3_5
  CURL::libcurl
  OpenSSL::SSL 
  ws2_32
  SQLiteCpp
)

# SQLiteCpp.lib
# sqlite3.lib
# 设置运行时搜索动态库目录RPATH
# set_target_properties(${TARGET_NAME} PROPERTIES
#     LINK_FLAGS "-Wl,--disable-new-dtags"  #强制启用RPATH
#     BUILD_RPATH ""  #追加RPATH
# )
# 获取并打印 BUILD_RPATH
get_target_property(GOTBUILD_RPATH ${TARGET_NAME} BUILD_RPATH)
message(STATUS "BUILD_RPATH: ${GOTBUILD_RPATH}")
set(CPACK_PROJECT_NAME ${PROJECT_NAME}) # 打包发布用 目前没用
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
