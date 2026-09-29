# C++23 模块工程 · g++ 构建 + clangd 补全 + Neovim 保存即红字 完整配置

本文档是 `/home/ruri/code_file/study_for_cpp23` 工程的唯一配置说明。
全部内容来自对本机实测，所有文件均为磁盘原文，可逐字复制复现。

---

## 0. 版本矩阵

| 组件 | 版本 | 查看命令 |
|---|---|---|
| 操作系统 | `Fedora Linux 44` | `grep PRETTY_NAME /etc/os-release` |
| g++ | `16.2.1 20260819 (Red Hat 16.2.1-2)` | `g++ --version` |
| clangd | `22.1.8` | `clangd --version` |
| clang++ | `22.1.8` | `clang++ --version` |
| CMake | `4.3.0` | `cmake --version` |
| Ninja | `1.13.2` | `ninja --version` |
| Neovim | `0.12.5` | `nvim --version` |
| LazyVim | 发行版 | `:Lazy` |
| blink.cmp | LazyVim 自带 | `:Lazy` |
| neocmakelsp | mason 包 | `ls ~/.local/share/nvim/mason/packages/` |

```text
$ g++ --version
g++ (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2)

$ clangd --version
clangd version 22.1.8

$ cmake --version
cmake version 4.3.0

$ ninja --version
1.13.2

$ nvim --version
NVIM v0.12.5
Build type: RelWithDebInfo
LuaJIT 2.1.1767980792
```

---

## 1. 依赖安装

```bash
sudo dnf install -y gcc-c++ clang-tools-extra cmake ninja-build neovim
```

判据：

```bash
g++ --version | head -1      # g++ (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2)
clangd --version | head -1   # clangd version 22.1.8
cmake --version | head -1    # cmake version 4.3.0
ninja --version              # 1.13.2
nvim --version | head -1     # NVIM v0.12.5
```

> `clang-tools-extra` 提供 `/usr/bin/clangd`。本工程**不用 mason 装 clangd**，用系统包。

---

## 2. 架构与分工

```
                    ┌──────────────────────────────┐
   写代码 ──────────►│  Neovim 0.12.5               │
                    │   ├── clangd 22.1.8          │──► 补全 / 跳转 / 悬浮 / 引用
                    │   ├── blink.cmp              │──► 补全菜单 UI 与键位
                    │   ├── neocmakelsp            │──► CMakeLists 补全
                    │   └── autocmds.lua           │──► 保存触发 g++
                    └──────────────────────────────┘
                                   │
              :w / autosave(500ms) │
                                   ▼
                    /usr/bin/g++ 16.2.1
                     -std=gnu++23 -fmodules-ts
                     -fmodule-mapper=<build>/.../<file>.o.modmap
                     -fsyntax-only
                                   │
                                   ▼
                    vim.diagnostic (namespace "gcc_make")  ──► 红字 + quickfix
```

| 能力 | 提供者 | 配置文件 |
|---|---|---|
| 补全 / 跳转 / 悬浮 / 引用 | `/usr/bin/clangd` 22.1.8 | `~/.config/nvim/lua/plugins/clangd.lua` + 工程根 `.clangd` |
| 补全菜单 UI 与键位 | `saghen/blink.cmp` | `~/.config/nvim/lua/plugins/blink.lua` |
| CMake 文件补全 | `neocmakelsp` | `~/.config/nvim/lua/plugins/cmake.lua` |
| 报错红字 | `/usr/bin/g++` 16.2.1 | `~/.config/nvim/lua/config/autocmds.lua` |
| 自动保存 | 内置 timer | `~/.config/nvim/lua/config/autosave.lua` |

**两条链路互不干涉**：clangd 只负责语义补全与跳转，其自身诊断被 `.clangd` 的 `Diagnostics.Suppress: '*'` 关闭；报错完全由 g++ 实编译给出。

---

## 3. 目录结构

```
/home/ruri/code_file/study_for_cpp23/
├── .clangd                     # clangd 工程级配置（剥 g++ 模块参数）
├── .vscode/                    # 保留，不影响 nvim
├── CMakeLists.txt              # 构建脚本
├── CMakePresets.json           # preset: default（Ninja + g++ + CDB）
├── README.md                   # 项目说明
├── how_to_setup.md             # 本文档
├── build/                      # 构建产物（.gcm / .o / .o.modmap / compile_commands.json）
├── include/                    # 第三方头文件目录（占位）
├── main.cpp                    # 入口，12 行
└── modules/                    # 15 个模块文件
```

> `build/`、`.cache/` 已在 `.gitignore` 中。

---

## 4. 项目三个文件原文

### 4.1 `CMakeLists.txt`

```cmake
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
```

### 4.2 `CMakePresets.json`

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "default",
      "binaryDir": "${sourceDir}/build",
      "generator": "Ninja",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_COMPILER": "g++",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    }
  ],
  "buildPresets": [
    { "name": "default", "configurePreset": "default" }
  ]
}
```

### 4.3 `.clangd`

```yaml
CompileFlags:
  Compiler: clang++
  Remove: [-fmodules-ts, -fmodule-mapper*, -fdeps-*, -fmodule-only]
Diagnostics:
  Suppress: '*'
```

---

## 5. CMakeLists.txt 逐段精解

### ① 版本与工程
```cmake
cmake_minimum_required(VERSION 3.28)
project(ProjectName LANGUAGES CXX)
```
- `LANGUAGES CXX` 只启用 C++，避免无谓探测 C 编译器。
- C++ 模块需要 CMake ≥ 3.28。

### ② 启用 C++23 并强制 + 导出编译数据库
```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```
- `REQUIRED ON`：硬性要求，工具链不支持直接报错。
- `EXPORT_COMPILE_COMMANDS ON`：生成 `build/compile_commands.json`，**clangd 补全与跳转的唯一数据来源**，缺了它 clangd 只能靠 `.clangd` 猜。

### ③ 源码自动收录
```cmake
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/build/.*")
add_executable(ProjectName ${PROJECT_SOURCES})
```
- `GLOB_RECURSE` 递归 `*.cpp`，**实现单元 `.cpp` 只能当普通源文件加进来**，不能进 `FILE_SET CXX_MODULES`。
- 必须 `list(FILTER ... EXCLUDE build/)`，否则 `build/` 内中间产物被重新收录，构建进入自噬。
- **语法坑**：`CONFIGURE_DEPENDS` 必须写在 glob 表达式**之前**（`GLOB 变量 CONFIGURE_DEPENDS 模式`），写错顺序会被当成普通文件名而失效。

### ④ 模块接口自动收录 + 护栏
```cmake
file(GLOB_RECURSE PROJECT_MODULES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cppm)
list(FILTER PROJECT_MODULES EXCLUDE REGEX ".*/build/.*")
foreach(_mod IN LISTS PROJECT_MODULES)
    file(READ "${_mod}" _modc)
    string(REGEX MATCH "export[ \t]+module" _has_export "${_modc}")
    if(NOT _has_export)
        message(FATAL_ERROR "...")
    endif()
endforeach()
```
- CMake 把每个 `.cppm` 都当作**模块接口单元**，缺 `export module` 会让 `CXX_MODULES` 整个 dyndep 阶段失败。
- 护栏在**配置阶段**点名违规文件，避免构建到一半才炸。

### ⑤ 接口单元进模块文件集
```cmake
if(PROJECT_MODULES)
    target_sources(ProjectName PUBLIC FILE_SET CXX_MODULES BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules FILES ${PROJECT_MODULES})
endif()
```
- `FILE_SET CXX_MODULES` 是 CMake 收录“模块接口/分区单元”的专用文件集。
- `BASE_DIRS modules` 作为相对路径基准。
- 不这样会报：
  - `Output ... is of type 'CXX_MODULES' but does not provide a module interface unit or partition`（把 `.cpp` 也塞进来时）
  - `the '<module>' module is provided but it is not found in a 'FILE_SET' of type 'CXX_MODULES'`（实现单元完全没纳入图时）

### ⑥ include 目录
```cmake
target_include_directories(ProjectName PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```
- `modules/` 放自己的库，`include/` 放第三方。

---

## 6. 模块文件清单

| 文件 | 首行 | 角色 |
|---|---|---|
| `modules/mystl.cppm` | `export module mystl;` | 主模块接口，`export import :io; :test;` |
| `modules/mystl.io.cppm` | `export module mystl:io;` | 分区接口，`io::ls_println` 模板 + `io::test` |
| `modules/mystl.io.cpp` | `module mystl:io;` | 分区实现 |
| `modules/mystl.test.cppm` | `export module mystl:test;` | 空分区接口 |
| `modules/myclass.cppm` | `export module myclass;` | 主模块接口，`export import :inherit;` |
| `modules/myclass.inherit.cppm` | `export module myclass:inherit;` | 分区接口，`animal`/`dog`/`cat` |
| `modules/myclass.inherit.cpp` | `module myclass:inherit;` | 分区实现 |
| `modules/ptr.cppm` | `export module ptr;` | 主模块接口，`export import :constptr;` |
| `modules/ptr.constptr.cppm` | `export module ptr:constptr;` | 分区接口，`constptr::how_to_use_*` |
| `modules/ptr.constptr.cpp` | `module ptr:constptr;` | 分区实现 |
| `modules/test.cppm` | `export module test;` | 主模块接口，`export import :io; :tetete;` |
| `modules/test.io.cppm` | `export module test:io;` | 分区接口 |
| `modules/test.te.cppm` | `export module test:te;` | 分区接口 |
| `modules/testetststst.cppm` | `export module test:tetete;` | 分区接口（文件名与模块名不同，靠首行判定） |
| `modules/test.cpp` | `module test;` | 主模块实现 |

### 6.1 关键接口原文

`modules/mystl.cppm`：
```cpp
export module mystl;
export import :io;
export import :test;
```

`modules/mystl.io.cppm`：
```cpp
export module mystl:io;
import std;

export namespace io {
template <typename T> void ls_println(T &&ls) {
  for (auto &&i : ls) {
    std::print("{} ", i);
  }
  std::println();
}
void test(int a);

} // namespace io
```

`modules/test.cppm`：
```cpp
export module test;
export import :io;
export import :tetete;
import std;
```

`modules/myclass.cppm`：
```cpp
export module myclass;
export import :inherit;
```

`modules/ptr.cppm`：
```cpp
export module ptr;
export import :constptr;
```

### 6.2 `main.cpp`（12 行）

```cpp
import std;
import mystl;
import myclass;
import ptr;
import test;
int main() {
  std::println("test");
  std::vector<int> vec{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto v = vec | std::ranges::views::take(3);
  for (auto &x : vec)
    std::print("{}  ", x);
  return 0;
}
```

### 6.3 命名空间一致性规则

`.cppm` 里 `export namespace io { void hello(); }` 导出的符号是 `io::hello@test`；
`.cpp` 实现必须包进**同名** `namespace io`，否则链接报：
```
undefined reference to `io::hello@test()'
```
用 `nm` 验证：
```bash
nm -C build/CMakeFiles/ProjectName.dir/modules/test.cpp.o | grep -i hello
nm -u build/CMakeFiles/ProjectName.dir/main.cpp.o | grep -i hello
```
`T`（已定义）与 `U`（未定义引用）名字一致才链接成功。

### 6.4 分区规则

- **接口**：`export module 模块:分区;`（必须 `export`），文件 `.cppm`。
- **实现**：`module 模块:分区;`（不带 `export`），文件 `.cpp`。
- 接口与实现**必须分两个文件**。同文件写两份会报 `multiple definition of 'initializer for module ...'`。
- 主模块 `.cppm` 用 `export import :分区;` 把分区重新导出。

---

## 7. 补全配置

### 7.1 分工

| 能力 | 提供者 | 配置位置 |
|---|---|---|
| 补全 / 跳转 / 悬浮 / 引用 | `/usr/bin/clangd` 22.1.8 | `~/.config/nvim/lua/plugins/clangd.lua` + 工程根 `.clangd` |
| 补全菜单 UI 与键位 | `saghen/blink.cmp`（LazyVim 自带） | `~/.config/nvim/lua/plugins/blink.lua` |
| 报错红字 | `/usr/bin/g++` 16.2.1 | `~/.config/nvim/lua/config/autocmds.lua` |
| CMake 文件补全 | `neocmakelsp` | `~/.config/nvim/lua/plugins/cmake.lua` |

`.clangd` 的 `Diagnostics.Suppress: '*'` 关掉 clangd 自身诊断，避免 g++ 与 clangd 对模块工程给出两套互相矛盾的报错；clangd 仍照常解析并建索引，所以补全不受影响。

### 7.2 clangd 启动参数逐项（`~/.config/nvim/lua/plugins/clangd.lua` 全文）

```lua
return {
  {
    "neovim/nvim-lspconfig",
    opts = {
      servers = {
        clangd = {
          mason = false,
          cmd = (function()
            local cwd = vim.fn.getcwd()
            local cmd = {
              "/usr/bin/clangd",
              "--experimental-modules-support",
              "--query-driver=/usr/bin/g++",
              "--background-index",
            }
            local f = io.open(cwd .. "/build/compile_commands.json")
            if f then
              f:close()
              table.insert(cmd, "--compile-commands-dir=" .. cwd .. "/build")
            end
            return cmd
          end)(),
        },
      },
    },
  },
}
```

| 参数 | 作用 |
|---|---|
| `mason = false` | 用系统 `/usr/bin/clangd`，不用 mason 下载版 |
| `--experimental-modules-support` | 让 clangd 解析 `import` / `export module` 语法 |
| `--query-driver=/usr/bin/g++` | 允许 clangd 查询 g++ 的系统头路径，否则 `/usr/include/c++/16` 认不出 |
| `--background-index` | 后台建索引，否则首次跳转要等 |
| `--compile-commands-dir=<cwd>/build` | 指定 CDB 所在目录（clangd 默认在工程根找） |

- `cwd` 在插件加载时求值（`vim.fn.getcwd()`），**所以 nvim 必须从工程根启动**，否则 `--compile-commands-dir` 指错、补全退化。
- 未导入 `lazyvim.plugins.extras.lang.clangd`（`config/lazy.lua` 只 `import = "lazyvim.plugins"` 与 `import = "plugins"`），避免 LazyVim 的 clangd 默认项覆盖 `mason=false` 与上面的 `cmd`。

### 7.3 `.clangd` 过滤规则

```yaml
CompileFlags:
  Compiler: clang++
  Remove: [-fmodules-ts, -fmodule-mapper*, -fdeps-*, -fmodule-only]
Diagnostics:
  Suppress: '*'
```

| 项 | 作用 |
|---|---|
| `Compiler: clang++` | 把 CDB 里的 `g++` 换成 clang 前端参数集，clangd 才认 |
| `Remove: -fmodules-ts` | 去掉 GCC 专属模块开关 |
| `Remove: -fmodule-mapper*` | 去掉 GCC 专属 modmap，clangd 用自己的 PCM |
| `Remove: -fdeps-*` | 去掉 GCC 依赖扫描参数 |
| `Remove: -fmodule-only` | 去掉 GCC 模块专用编译标志 |
| `Diagnostics.Suppress: '*'` | 关 clangd 诊断，报错交给 g++ |

> 不加 `CompilationDatabase: build`：clangd 已由 `--compile-commands-dir` 指定，重复设置会让相对路径解析错位。

### 7.4 blink.cmp 键位（`~/.config/nvim/lua/plugins/blink.lua` 全文）

```lua
return {
  {
    "saghen/blink.cmp",
    opts = {
      keymap = {
        preset = "default",
        ["<Tab>"] = { "accept", "fallback" },
        ["<CR>"] = { "fallback" },
        ["<C-y>"] = { "select_and_accept" },
      },
    },
  },
}
```

| 键 | 行为 |
|---|---|
| `<Tab>` | 有候选则接受，无候选走原生缩进 |
| `<CR>` | 有候选则换行（`fallback` 保证不误选） |
| `<C-y>` | 强制接受当前选中项 |

`blink.cmp` 由 LazyVim 基座提供（`config/lazy.lua` 的 `{ "LazyVim/LazyVim", import = "lazyvim.plugins" }`），本文件只覆盖键位。

### 7.5 CMake 补全（`~/.config/nvim/lua/plugins/cmake.lua` 全文）

```lua
return {
  {
    "neovim/nvim-lspconfig",
    opts = {
      servers = {
        neocmake = {
          cmd = {
            vim.fn.stdpath("data")
              .. "/mason/packages/neocmakelsp/bin/neocmakelsp",
            "--stdio",
          },
        },
      },
    },
  },
}
```
- 可执行文件在 mason 的 `bin/` 下，必须显式给全路径。
- 必须带 `--stdio`，否则 neocmakelsp 不会以 LSP 模式启动。

### 7.6 索引与缓存

- 索引目录：`~/.cache/clangd`（XDG 默认）。
- 何时清索引重启：改了 `modules/` 新增/删除模块、改了 `CMakeLists.txt`、`build/` 被 `rm -rf` 重建之后。
- 无需清：日常改函数体（g++ hook 已即时反馈）。
- 一键操作：`<C-b>`（见 §9 与 §8.5）。

### 7.7 补全验证（照做必出菜单）

```bash
ls build/compile_commands.json   # 前置:CDB 必须存在
```

1. 从**工程根**启动：`nvim /home/ruri/code_file/study_for_cpp23/main.cpp`
2. 第 7 行后新起一行，输入 `std::` → 出补全菜单，`<Tab>` 接受
3. 输入 `std::vector<int> v; v.` → 出成员菜单
4. `import mystl;` 后输入 `io::` → 出 `ls_println` / `test`
5. 光标停在 `std::ranges::views::take` 上按 `gd` → 跳 `/usr/include/c++/16/ranges`（2612 行附近）
6. `K` → 悬浮显示签名

判据：
```vim
:lua =#vim.lsp.get_clients({name="clangd"})   " > 0
:LspInfo                                       " cmd 含 --compile-commands-dir=<root>/build
```

headless 自检：
```bash
nvim --headless -c 'lua vim.defer_fn(function() print(#vim.lsp.get_clients({name="clangd"})) vim.cmd("qa!") end, 5000)' main.cpp
```

### 7.8 补全失效四查

| 现象 | 原因 | 处理 |
|---|---|---|
| 无任何候选 | clangd 未启动 | `:LspInfo`；确认 `init.lua` 已 `require("config.lazy")` |
| `std::` 有、`std::ranges::` 无 | 未加 `--query-driver=/usr/bin/g++` | 检查 `plugins/clangd.lua` 的 `--query-driver` |
| 自建模块 `io::` 无候选 | 启动 cwd 不是工程根 | 退出，从工程根重启 nvim |
| 改了 `modules/` 后候选陈旧 | 索引未刷新 | 按 `<C-b>` |

---

## 8. 报错配置（保存即 g++ 红字）

### 8.1 原理

保存触发 → 由 `build/CMakeFiles/ProjectName.dir/<相对路径>.o.modmap` 拼出 g++ 命令 → `-fsyntax-only` → 解析 stderr → 写诊断。

真实 modmap 内容（`build/CMakeFiles/ProjectName.dir/main.cpp.o.modmap`）：
```text
$root .
test CMakeFiles/ProjectName.dir/test.gcm
ptr CMakeFiles/ProjectName.dir/ptr.gcm
myclass CMakeFiles/ProjectName.dir/myclass.gcm
std CMakeFiles/__cmake_cxx_std_23@synth_5e4d1538c243.dir/ee2887e60fcb.bmi
mystl CMakeFiles/ProjectName.dir/mystl.gcm
```
> modmap 按**模块名**映射，与源文件路径无关；cwd 固定为 `build/`，所以相对路径能解析。

### 8.2 `~/.config/nvim/lua/config/autocmds.lua` 全文

```lua
local Aug = vim.api.nvim_create_augroup("MakeOnSave", { clear = true })
local GCC_NS = vim.api.nvim_create_namespace("gcc_make")

local CXX = "/usr/bin/g++"
local STD = "-std=gnu++23"
local INCS = { "modules", "include" }
local TARGET = "ProjectName"

local building = false

local function find_project_root()
  local dir = vim.fn.fnamemodify(vim.api.nvim_buf_get_name(0), ":p:h")
  local prev = nil
  while dir ~= prev do
    if vim.fn.filereadable(dir .. "/CMakeLists.txt") == 1 then
      return dir
    end
    prev = dir
    dir = vim.fn.fnamemodify(dir, ":h")
  end
  return nil
end

local function normalize_lines(x)
  if type(x) == "string" then
    return vim.split(x, "\n")
  end
  if type(x) == "table" then
    local out = {}
    for _, l in ipairs(x) do
      table.insert(out, l)
    end
    return out
  end
  return {}
end

local function parse_gcc_output(text)
  local diags = {}
  for _, line in ipairs(normalize_lines(text)) do
    if line ~= "" then
      local file, line_no, col, lvl, msg =
        line:match("^(.-):(%d+):(%d+):%s*([%a ]+):%s*(.+)")
      if not file then
        file, line_no, lvl, msg = line:match("^(.-):(%d+):%s*([%a ]+):%s*(.+)")
        col = "1"
      end
      if file and line_no and lvl and msg then
        local sev = (lvl == "error" or lvl == "fatal error")
            and vim.diagnostic.severity.ERROR
          or (lvl == "warning") and vim.diagnostic.severity.WARN
          or vim.diagnostic.severity.INFO
        table.insert(diags, {
          filename = file,
          lnum = tonumber(line_no) - 1,
          col = tonumber(col and col or 1) - 1,
          severity = sev,
          message = msg,
        })
      end
    end
  end
  return diags
end

local function parse_build_errors(text)
  local msgs = {}
  for _, line in ipairs(normalize_lines(text)) do
    if line:match("^CMake Error")
      or line:match("^FAILED:")
      or line:match("^ninja: build stopped")
      or line:match("^make.*Error")
    then
      table.insert(msgs, line)
    end
  end
  return msgs
end

local function only_project(diags, root)
  local out, r = {}, vim.fn.fnamemodify(root, ":p")
  for _, d in ipairs(diags) do
    if vim.startswith(vim.fn.fnamemodify(d.filename, ":p"), r) then
      table.insert(out, d)
    end
  end
  return out
end

local function show_diagnostics(all, cmake_msgs)
  for _, b in ipairs(vim.api.nvim_list_bufs()) do
    if vim.api.nvim_buf_is_loaded(b) then
      vim.diagnostic.reset(GCC_NS, b)
    end
  end

  local by_file = {}
  for _, d in ipairs(all) do
    local key = vim.fn.fnamemodify(d.filename, ":p")
    by_file[key] = by_file[key] or {}
    table.insert(by_file[key], d)
  end

  for file, diags in pairs(by_file) do
    local b = vim.fn.bufnr(file)
    if b ~= -1 and vim.api.nvim_buf_is_loaded(b) then
      vim.diagnostic.set(GCC_NS, b, diags, { update = true })
    end
  end

  local qf = {}
  for _, d in ipairs(all) do
    table.insert(qf, {
      filename = d.filename,
      lnum = d.lnum + 1,
      col = d.col + 1,
      text = d.message,
      type = d.severity == vim.diagnostic.severity.ERROR and "E" or "W",
    })
  end
  for _, m in ipairs(cmake_msgs) do
    table.insert(qf, { text = m, type = "E" })
  end
  if #qf > 0 then
    vim.fn.setqflist(qf, "r")
  end
end

local function syntax_check()
  local bufnr = vim.api.nvim_get_current_buf()
  local abs = vim.api.nvim_buf_get_name(bufnr)
  if abs == "" then
    return false
  end
  local ext = vim.fn.fnamemodify(abs, ":e")
  if ext ~= "cpp" and ext ~= "cppm" and ext ~= "cxx" and ext ~= "cc" then
    return false
  end
  local root = find_project_root()
  if not root then
    return false
  end
  local bdir = root .. "/build"
  local rel = vim.fn.fnamemodify(abs, ":p"):sub(#root + 2)
  local modmap = bdir .. "/CMakeFiles/" .. TARGET .. ".dir/" .. rel .. ".o.modmap"
  if vim.fn.filereadable(modmap) ~= 1 then
    return false
  end

  local cmd = { CXX, STD, "-fmodules-ts", "-fmodule-mapper=" .. modmap,
    "-x", "c++", "-fsyntax-only", abs }
  for _, inc in ipairs(INCS) do
    table.insert(cmd, 2, "-I" .. root .. "/" .. inc)
  end

  vim.system(cmd, { cwd = bdir, text = true }, function(result)
    vim.schedule(function()
      building = false
      if not vim.api.nvim_buf_is_valid(bufnr) then
        return
      end
      local raw = (result.stderr or "") .. (result.stdout or "")
      show_diagnostics(only_project(parse_gcc_output(raw), root), {})
    end)
  end)
  return true
end

local function build_and_diagnose()
  local bufnr = vim.api.nvim_get_current_buf()
  local abs = vim.api.nvim_buf_get_name(bufnr)
  if abs == "" then
    return false
  end
  local root = find_project_root()
  if not root or vim.fn.isdirectory(root .. "/build") ~= 1 then
    return false
  end

  vim.system(
    { "cmake", "--build", root .. "/build" },
    { stdout = {}, stderr = {}, text = true },
    function(result)
      vim.schedule(function()
        building = false
        if not vim.api.nvim_buf_is_valid(bufnr) then
          return
        end
        local raw = (result.stderr or "") .. (result.stdout or "")
        local all = only_project(parse_gcc_output(raw), root)
        local cmake_msgs = parse_build_errors(raw)
        show_diagnostics(all, cmake_msgs)
        if #cmake_msgs > 0 and #all == 0 then
          vim.notify(table.concat(cmake_msgs, "\n"), vim.log.levels.ERROR)
        end
      end)
    end
  )
  return true
end

local function guarded(fn)
  return function()
    if building then
      return
    end
    building = true
    local ok, started = pcall(fn)
    if not ok or not started then
      building = false
    end
  end
end

vim.api.nvim_create_autocmd("BufWritePost", {
  group = Aug,
  pattern = { "*.cppm", "*.ixx", "*.cppmh", "*.cpp", "*.cxx", "*.cc", "*.h", "*.hpp" },
  callback = guarded(syntax_check),
})

vim.api.nvim_create_user_command("Make", guarded(build_and_diagnose), { desc = "g++: build & fill diagnostics" })
```

### 8.3 关键常量与机制

| 常量 | 值 | 说明 |
|---|---|---|
| `CXX` | `/usr/bin/g++` | 与 CMake preset 一致 |
| `STD` | `-std=gnu++23` | 与 `CMAKE_CXX_STANDARD 23` 对应（GCC 默认扩展） |
| `INCS` | `{ "modules", "include" }` | 与 `target_include_directories` 一致 |
| `TARGET` | `ProjectName` | 与 `project(ProjectName)` 一致 |

机制要点：
- `BufWritePost` 只对 `.cppm/.cpp/.cxx/.cc` 触发，`.h/.hpp` 在扩展名检查处提前 return（pattern 里保留无害）。
- `modmap` 不存在就 return，避免在非模块文件上误跑。
- `cwd = bdir` 是关键：modmap 内路径全是相对 `build/` 的。
- `guarded` 用 `building` 互斥锁防重入。
- `only_project` 过滤掉 `/usr/include/c++/16` 等系统头诊断，避免刷屏。
- `:Make` 走全量 `cmake --build`，用于手动触发或 cmake 级错误。

### 8.4 `~/.config/nvim/lua/config/autosave.lua` 全文

```lua
local timer = vim.uv.new_timer()
local function save()
  timer:stop()
  timer:start(500, 0, function()
    vim.schedule(function()
      if vim.bo.modified and not vim.bo.readonly and vim.fn.bufname("%") ~= "" then
        local has_err = #vim.diagnostic.get(0, {
          severity = vim.diagnostic.severity.ERROR,
        }) > 0
        if has_err then
          vim.b.autoformat = false
          vim.cmd("update")
          vim.b.autoformat = nil
        else
          vim.cmd("update")
        end
      end
    end)
  end)
end

local g = vim.api.nvim_create_augroup("AutoSave", { clear = true })
vim.api.nvim_create_autocmd({ "TextChanged", "TextChangedI" }, {
  group = g,
  callback = save,
})
```
- `TextChanged` / `TextChangedI` → 500ms 防抖 → `vim.cmd("update")`。
- 有 ERROR 时临时关 `autoformat`，避免“红字没存下来就被格式化改掉”。

### 8.5 触发红字的三条路径

| 路径 | 触发 | 命令 |
|---|---|---|
| 手动保存 | `:w` / `ZZ` | `BufWritePost` → `syntax_check` |
| 自动保存 | 停手 500ms | `TextChanged` → `autosave.lua` → `update` → `BufWritePost` |
| 手动命令 | `:Make` | `build_and_diagnose`（全量 cmake build） |

前置条件（缺一不触发）：
1. 文件在含 `CMakeLists.txt` 的工程内；
2. `build/` 目录存在；
3. 扩展名在 pattern 列表内；
4. `build/CMakeFiles/ProjectName.dir/<相对路径>.o.modmap` 存在。

### 8.6 诊断验证（照做必红）

```bash
grep -n 'undeclared_xyz' main.cpp || echo "main.cpp 干净"
```
1. 打开 `main.cpp`，在 `main()` 内插入 `int zzz = undeclared_xyz;`
2. `:w`
3. 预期：该行红色 ERROR，`:copen` 有对应条目
4. 删除该行，`:w`，红字消失

> 注意：`int zzz_undeclared_e2e;` 在文件作用域是**合法定义**，不会报错。必须用真错误（引用未声明标识符）。

### 8.7 没爆红排查

| 检查 | 命令 |
|---|---|
| 是否在工程内 | `:lua print(vim.fn.filereadable(vim.fn.getcwd().."/CMakeLists.txt"))` |
| `build/` 是否存在 | `ls build/` |
| modmap 是否存在 | `ls build/CMakeFiles/ProjectName.dir/main.cpp.o.modmap` |
| 扩展名是否在 pattern | `:set ft?` |
| lua 是否已加载 | `:lua print(package.loaded["config.autocmds"])` |
| 手动跑 g++ | 见 §8.2 的 cmd 拼法 |

---

## 9. 快捷键

| 键/命令 | 作用 | 出处 |
|---|---|---|
| `<C-b>` | 重建模块 + 清索引 + 重启 clangd | `keymaps.lua` 行 101–120 |
| `gd` | 跳定义（clangd） | LazyVim 默认 |
| `gr` / `gI` / `gy` | 引用 / 实现 / 类型定义 | LazyVim 默认 |
| `K` | 悬浮签名 | LazyVim 默认 |
| `<Tab>` / `<CR>` / `<C-y>` | 补全接受 / 换行 / 强制接受 | `blink.lua` |
| `:Make` | 手动 g++ 全量构建并填诊断 | `autocmds.lua` 末行 |
| `:LazyChineseCheck` | 列出未汉化键位 | `keymaps.lua` 行 73 |

`<C-b>` 原文（`config/keymaps.lua` 行 101–120）：
```lua
vim.keymap.set("n", "<C-b>", function()
  Snacks.terminal(
    "cmake --preset default && cmake --build build && "
      .. "pkill -u $USER clangd 2>/dev/null; rm -rf ~/.cache/clangd",
    {
      pos = "float",
      on_exit = function()
        vim.schedule(function()
          pcall(function()
            for _, c in ipairs(vim.lsp.get_clients({ name = "clangd" })) do
              c.stop()
            end
            vim.lsp.enable("clangd")
          end)
        end)
      end,
    }
  )
end, { desc = "一键重建模块+重启clangd", nowait = true, silent = true })
```

---

## 10. `init.lua` 与加载链

`~/.config/nvim/init.lua` 全文：
```lua
require("config.lazy")
require("config.autosave")
require("config.autocmds")
```
- `config.lazy` → LazyVim 基座 + `lua/plugins/*`。
- `config.autosave` → 注册自动保存。
- `config.autocmds` → 注册 g++ 保存即诊断 + `:Make`。

---

## 11. 从零构建

```bash
cmake --preset default
cmake --build build
./build/ProjectName; echo "EXIT=$?"
```

预期关键日志：
```
Scanning modules/mystl.io.cppm for CXX dependencies
Building CXX object CMakeFiles/ProjectName.dir/modules/mystl.io.cppm.o
Building CXX object CMakeFiles/ProjectName.dir/modules/mystl.io.cpp.o
Linking CXX executable ProjectName
```
二次构建：`ninja: no work to do`。

> 从 clang++ 路线切到 g++ 路线（或反之）**必须** `rm -rf build` 重配：`.gcm/.o.modmap` 与 `.pcm` 不兼容。

---

## 12. 补全 / 跳转实测

| 项 | 结果 |
|---|---|
| `std::` 补全 | 出菜单 |
| `io::`（`mystl`）补全 | 出 `ls_println` / `test` |
| `std::ranges::views::take` 跳定义 | `/usr/include/c++/16/ranges` 2612 行附近 |
| `std::ranges::views::take` 另一处 | `/usr/include/c++/16/bits/std.cc` 2687 行 |
| `K` 悬浮 | 出签名 |

> 早前结论「clangd 不认 GCC 模块、跳转可能不全」**错误**：clangd 靠自身解析 GCC 16 头文件建索引，与 `.gcm` 无关。

---

## 13. 延迟实测

| 场景 | `:write` → 诊断 | autosave 链路 → 诊断 |
|---|---|---|
| 14 行 `main.cpp` | 0.430s | 0.932s（含 500ms 防抖） |
| 1002 行 `main.cpp`（110 个模板函数 + 5 个 import） | 0.558s | 1.072s |

结论：行数从 14 增到 1002 只慢约 0.13s，耗时由 `import std` / 模块解析主导。

---

## 14. 验证脚本

```bash
# 1 工具链
g++ --version | head -1
clangd --version | head -1
cmake --version | head -1
ninja --version
nvim --version | head -1

# 2 配置 + 构建
cmake --preset default
cmake --build build
./build/ProjectName; echo "EXIT=$?"

# 3 模块产物：.gcm 数量 == .cppm 数量
ls build/CMakeFiles/ProjectName.dir/*.gcm | wc -l
find modules -name '*.cppm' | wc -l

# 4 modmap（决定保存即诊断能否命中）
cat build/CMakeFiles/ProjectName.dir/main.cpp.o.modmap

# 5 clangd 看到的编译命令
python3 -c "import json;d=json.load(open('build/compile_commands.json'));print(d[0]['command'])"

# 6 诊断自检：注入真错误后 :w，应出 ERROR，再还原
grep -n 'undeclared_xyz' main.cpp || echo "main.cpp 干净"

# 7 补全自检（headless 起 clangd 看是否起来）
nvim --headless -c 'lua vim.defer_fn(function() print(#vim.lsp.get_clients({name="clangd"})) vim.cmd("qa!") end, 5000)' main.cpp

# 8 排查用
nm -C build/CMakeFiles/ProjectName.dir/modules/mystl.io.cpp.o | grep -i ls_println
ninja -C build -t commands | grep -E "mystl.io.cppm"
python3 -c "import json;d=json.load(open('build/CMakeFiles/ProjectName.dir/CXXDependInfo.json'));[print(k) for k in d.get('cxx-modules',{})]"
```

---

## 15. 排障表

| 报错/现象 | 原因 | 处理 |
|---|---|---|
| `Output ... is of type 'CXX_MODULES' but does not provide a module interface unit or partition` | `.cpp` 塞进 `FILE_SET CXX_MODULES` | `.cppm` 进文件集，`.cpp` 当普通 `target_sources` |
| `the '<module>' module is provided but it is not found in a 'FILE_SET' of type 'CXX_MODULES'` | 实现单元未纳入模块图 | 加入 `target_sources` |
| `multiple definition of 'initializer for module ...'` | 分区接口 + 分区实现写在同一文件 | 拆成 `.cppm` + `.cpp` |
| `undefined reference to 'io::hello@test()'` | `.cpp` 命名空间与 `.cppm` 声明不一致 | 实现包进同名 `namespace` |
| `vim.diagnostic.set ... namespace: expected number` | namespace 传 nil | 传 numeric namespace |
| 保存不红 | ①非工程根 ②无 `build/` ③扩展名不在 pattern ④无 `.o.modmap` | 逐条查 §8.7 |
| `CONFIGURE_DEPENDS` 不生效 | 写在 glob 表达式之后 | 改成 `GLOB 变量 CONFIGURE_DEPENDS 模式` |
| 新增 `.cppm` 未编译 | 同上 | 补 `CONFIGURE_DEPENDS`，重跑 `cmake --preset default` |
| 补全无候选 | clangd 未启动 / cwd 错 / 未清索引 | 见 §7.8 |
| CMake 配置阶段 `FATAL_ERROR` 点名某 `.cppm` | 该文件缺 `export module` | 改成 `export module <模块:<分区>>;` 或删除 |

---

## 16. 已知限制

- 实际标准是 `gnu++23`（`autocmds.lua` 的 `-std=gnu++23`），非严格 `c++23`。改严格只需在 `CMakeLists.txt` 加 `set(CMAKE_CXX_EXTENSIONS OFF)`，并同步改 `autocmds.lua` 的 `STD`。
- `-fmodules-ts` 是 GCC experimental 特性，非标准。GCC 11→16 一直 experimental，升级 GCC 无法解决。
- nvim **必须从工程根启动**（`plugins/clangd.lua` 加载期求值 `vim.fn.getcwd()`）。
- 保存即诊断依赖 `build/` 与 `.o.modmap` 已存在，首次克隆仓库必须先 `cmake --preset default`。
- `autocmds.lua` 的 `building` 互斥锁在异常路径（如 `vim.system` 抛错）可能残留 `true`，导致后续静默不检查。
- `autosave.lua` 的 `has_err` 读取时机与诊断写入存在竞态，未验证。
- Linux 上没有 100% 标准 C++23 模块方案（最完整是 MSVC）；clang++ 22 + libc++ 22 是另一条可行但需重配路线。

---

## 17. 回滚

```bash
rm -rf build ~/.cache/clangd
cmake --preset default
cmake --build build
```

Neovim 侧回到默认（去掉本工程定制）：
1. 删 `~/.config/nvim/lua/plugins/clangd.lua`、`~/.config/nvim/lua/plugins/cmake.lua`
2. 从 `~/.config/nvim/init.lua` 移除 `require("config.autosave")`、`require("config.autocmds")`
3. `:Lazy restore`
4. `pkill clangd; rm -rf ~/.cache/clangd`

---

*本文档所有命令与文件内容均来自对 `/home/ruri/code_file/study_for_cpp23` 的实测。*