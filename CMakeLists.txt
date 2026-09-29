cmake_minimum_required(VERSION 3.28)

project(ProjectName LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/build/.*")
add_executable(ProjectName ${PROJECT_SOURCES})

file(GLOB_RECURSE PROJECT_MODULES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cppm)
list(FILTER PROJECT_MODULES EXCLUDE REGEX ".*/build/.*")
# 护栏:每个 .cppm 都必须含 "export module",否则 CMake 的 CXX_MODULES 会整个 dyndep
# 失败且 clangd 也误报。任何缺 export module 的 .cppm 都在配置阶段点名报错。
foreach(_mod IN LISTS PROJECT_MODULES)
    file(READ "${_mod}" _modc)
    string(REGEX MATCH "export[ \t]+module" _has_export "${_modc}")
    if(NOT _has_export)
        message(FATAL_ERROR "${_mod} 不是合法的 C++ 模块接口单元:缺少 'export module xxx;'。\n"
            "CMake 把每个 .cppm 都当模块接口;若这是实现/碎片,请删掉它,"
            "或改成 'export module <模块:<分区>>;' 开头,例如 'export module test:te;'。")
    endif()
endforeach()
if(PROJECT_MODULES)
    target_sources(ProjectName PUBLIC FILE_SET CXX_MODULES BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules FILES ${PROJECT_MODULES})
endif()

# 头文件目录:modules/ 放自己的库,include/ 放第三方的
target_include_directories(ProjectName PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 第三方库目录与链接(放 include/<库名>/lib 下的 .a/.so 时取消注释)
# target_link_directories(ProjectName PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/<库名>/lib)
# target_link_libraries(ProjectName PRIVATE <库名>)