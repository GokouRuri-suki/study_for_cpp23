cmake_minimum_required(VERSION 4.3)

# 编译器与 flags 必须在 project() 之前设置,
# 否则 CMake 检测阶段会选错 libstdc++ 的 std 模块导致报错
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "-stdlib=libc++")
project(ProjectName CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/build/.*")
add_executable(ProjectName ${PROJECT_SOURCES})

file(GLOB_RECURSE PROJECT_MODULES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cppm)
list(FILTER PROJECT_MODULES EXCLUDE REGEX ".*/build/.*")
if(PROJECT_MODULES)
    target_sources(ProjectName PUBLIC FILE_SET CXX_MODULES FILES ${PROJECT_MODULES})
endif()

# 头文件目录:modules/ 放自己的库,include/ 放第三方的
target_include_directories(ProjectName PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 第三方库目录与链接(放 include/<库名>/lib 下的 .a/.so 时取消注释)
# target_link_directories(ProjectName PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/<库名>/lib)
# target_link_libraries(ProjectName PRIVATE <库名>)