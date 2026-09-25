# 环境搭建清单 (SETUP)

换新机器时按此清单配置,项目本身零重配(所有项目级配置已随仓库携带),snippet 零拷贝(Settings Sync 自动同步)。

## 1. 软件包 (Fedora)

```bash
sudo dnf install cmake ninja-build clang libcxx libcxx-devel clang-tools-extra
```

版本要求:
- CMake ≥ 3.28(推荐 4.3+,模块支持要求)
- clang ≥ 18(`import std` + libc++ 模块)
- `libcxx-devel` 提供 libc++ 的 `std` 模块源文件(`/usr/share/libc++/v1/std.cppm`),**最容易漏装**
- `clang-tools-extra` 提供 clangd

## 2. VS Code 扩展

- `ms-vscode.cmake-tools`(CMake Tools)
- `llvm-vs-code-extensions.vscode-clangd`(clangd,不要用微软 C/C++ 扩展)

## 3. 用户级配置 (零拷贝,靠 Settings Sync)

用户片段(snippet)通过 VS Code 内置的 Settings Sync 自动同步,新机器**无需手动拷贝任何文件**:

1. VS Code 左下角齿轮 → "Backup and Sync Settings..."(用 GitHub 账号登录)
2. 确认同步项中勾选了 "User Snippets"
3. 登录后输入 `cmake` / `cmakepresets` 即可补全

> 本机已开启并验证:snippet 修改会被自动上传(日志 `Snippets: Updated remote snippets`)。

包含两个片段:
- `cmake` — 补全生成 CMakeLists.txt 模板(clang++/libc++/C++23/模块支持已内置)
- `cmakepresets` — 补全生成 CMakePresets.json(Ninja 生成器,模块编译必需)

## 4. 项目自带文件 (跟仓库走,无需重配)

| 文件 | 作用 |
|------|------|
| `CMakeLists.txt` | 编译器/flags 已内置,**必须在 `project()` 之前 set** |
| `CMakePresets.json` | Ninja 生成器 + clang++/libc++/Debug(模块只能用 Ninja) |
| `.vscode/settings.json` | clangd 开启 `--experimental-modules-support` |

## 5. 新项目快速流程

```bash
mkdir -p 新项目/{modules,include}   # modules/ 放自己的模块,include/ 放第三方
```

1. 新建 `CMakeLists.txt` → 输入 `cmake` 回车补全 → Tab 改项目名
2. 新建 `CMakePresets.json` → 输入 `cmakepresets` 回车补全
3. 从旧项目拷 `.vscode/settings.json`
4. 配置 + 编译 + 运行:

```bash
cmake --preset default && cmake --build --preset default && ./build/项目名
```

VS Code 里直接点 CMake Tools 的 Build 按钮也可(自动识别 presets)。

## 6. 常见坑速查

| 报错/现象 | 原因 |
|-----------|------|
| `bits/stdc++.h file not found` (扫描 std.cc 时) | `-stdlib=libc++` 设在 `project()` 之后,CMake 检测阶段选错了 libstdc++ 的 std 模块 |
| `unrecognized command-line option '-stdlib=libc++'` | 默认用了 g++,需在 `project()` 前 `set(CMAKE_CXX_COMPILER clang++)` |
| `modules are not supported by this generator` | 用了 Makefiles 生成器,必须 Ninja(走 presets) |
| `import std;` 标红且报 `... older format that is no longer supported` | clangd 与 clang++ **版本不一致**(如 clangd 23 vs clang++ 22)。让 clangd==clang++ 大版本并开 `--experimental-modules-support` 即可(详见 §7) |
| `export models` / `models math_utils` 之类语法错 | 关键字是 `module`,拼错会导致整个模块失效 |

## 7. 为什么 `import std;` 会报错,以及修复原理

一句话:不是代码错,是 **clangd 和编译用的 clang++ 版本不一致**,导致 clangd 读不懂 clang++ 生成的模块缓存(.bmi/.pcm)。

1. `import std;` 的 std 不是普通头文件,而是**编译期预编译模块缓存**:`.bmi`(std)/`.pcm`(自定义模块),由 clang++ 构建时生成。
2. 这些缓存文件带 **clang++ 22.x 的格式戳**。
3. 若编辑器/服务器用 **clangd 23**,去读 v22 格式缓存 → `module file '...bmi' uses an older format that is no longer supported` → 无法理解 `std::vector` 等 → `import std;` 满屏红线。
4. 修复:让 clangd 与 clang++ **同一版本**(都用系统 22),并开模块参数:
   - `--experimental-modules-support` — 开启 C++20/23 模块解析
   - `--query-driver=/usr/bin/clang++` — 与编译同一编译器族,匹配编译参数
   - `--background-index` / `--header-insertion=never` — 加载/误报更稳
5. **原则:全链路(clang++/libc++/clangd,调试则加 LLDB)同 LLVM 家族,clangd==clang++ 大版本。**
6. 更换 clang++ 大版本后,删除 build/ 重编(旧 .bmi/.pcm 失效);clangd 版本对齐后 `import std;` 即不再报错。(同法适用于 nvim 的 clangd 配置)

