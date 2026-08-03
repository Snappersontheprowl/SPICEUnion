# C++ 标准开发配置

时间：2026-08-03

## 这类问题说明什么

在 VSCode 中看到：

```text
'su/evaluator.hpp' file not found
```

通常不等于项目真的编译失败。更常见的原因是：

```text
构建系统知道 include 路径
编辑器 / 语言服务器不知道 include 路径
```

在 SPICEUnion 中，真实 CMake target 已经把公共头目录配置为：

```text
include/
```

因此源码中写：

```cpp
#include "su/evaluator.hpp"
```

是正确的。`su/evaluator.hpp` 对应的真实文件是：

```text
include/su/evaluator.hpp
```

如果 VSCode 报找不到，优先修开发配置，不要把 include 写成相对路径：

```cpp
#include "../../include/su/evaluator.hpp"  // 不推荐
```

源码 include 应该表达库的公共头文件结构，而不是编辑器当前的扫描状态。

## 一套标准 C++ 项目配置应该包含什么

一套舒服、稳定的 C++ 开发配置通常至少包含下面几层：

```text
CMakeLists.txt
  -> 定义真实构建目标、include path、compile options、test target

CMakePresets.json
  -> 固化 configure/build/test 配置，避免每个人手写不同命令

compile_commands.json
  -> 给 clangd / IntelliSense 提供真实编译命令数据库

VSCode settings
  -> 让编辑器读取 CMake 配置，而不是自己猜 include path

.clang-format
  -> 统一格式化风格

.clang-tidy
  -> 统一静态检查策略

.gitignore
  -> 排除 build、运行产物、仿真结果
```

这里最关键的是 `compile_commands.json`。它记录每个源文件真实的编译命令，包括：

- 使用哪个编译器；
- 使用哪个 C++ 标准；
- include path；
- macro definitions；
- warning flags；
- 目标文件路径。

语言服务器有了它，才能像编译器一样理解项目。

## CMakeLists.txt 的职责

`CMakeLists.txt` 是真实构建规则，不是编辑器配置。

它应该负责：

- 声明项目名称和 C++ 标准；
- 定义 library / executable target；
- 用 target 级别声明 include 路径；
- 用 target 级别声明编译选项；
- 链接依赖库；
- 定义测试 target；
- 注册 CTest。

推荐使用 target-based CMake：

```cmake
add_library(spiceunion_core STATIC
  src/core/evaluator.cpp
)

target_compile_features(spiceunion_core PUBLIC cxx_std_17)

target_include_directories(spiceunion_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
```

这里的含义是：

- `PUBLIC include/`：使用 `spiceunion_core` 的目标也能看到公共头文件。
- `PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}`：项目内部可以 include 内部头，例如
  `src/pool/simulator_pool.hpp`。

不要优先使用全局 `include_directories()`。它会让 include path 变成全局状态，
后期项目一大，很难判断某个头文件为什么能被找到。

## CMakePresets.json 的职责

`CMakePresets.json` 用来固化标准 configure/build/test 配置。

没有 presets 时，每次可能手写：

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

时间一久，不同 build 目录、不同选项会混在一起。presets 可以把这些选择写进仓库。

推荐至少有两个 configure preset：

- `default`：普通开发，外部 Spectre 测试关闭。
- `external`：显式打开真实 Spectre 测试。

核心选项：

```json
"CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
```

这个选项会在 build 目录生成：

```text
build/compile_commands.json
```

这就是 VSCode/clangd 识别 include path 的关键文件。

## VSCode 配置的职责

VSCode 不应该自己维护一份手写 include path。最稳的方式是让 VSCode 读取 CMake。

如果使用 Microsoft C/C++ 插件，推荐：

```json
{
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

含义是：C/C++ 插件不自己猜配置，而是向 CMake Tools 询问每个文件应该怎么编译。

如果使用 clangd 插件，推荐让 clangd 读取 build 目录中的编译数据库：

```json
{
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/build"
  ]
}
```

注意：Microsoft C/C++ IntelliSense 和 clangd 都能做诊断。实际开发中最好选择一个作为
主要语言服务器，否则可能出现两个插件同时报错、诊断信息互相打架的情况。

## 推荐的日常开发流程

标准流程应该是：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

需要真实 Spectre 测试时：

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external
```

在 VSCode 中，对应流程是：

```text
CMake: Select Configure Preset
CMake: Configure
CMake: Build
CMake: Run Tests
```

如果 include 报错，优先检查：

```bash
test -f build/compile_commands.json
```

如果文件不存在，说明编辑器没有编译数据库可读。

## 格式化与静态检查

`compile_commands.json` 解决的是“编辑器理解项目”的问题。标准 C++ 项目还应该逐步补：

- `.clang-format`：统一格式化；
- `.clang-tidy`：统一静态检查；
- CI / 本地脚本：统一执行 configure、build、test、format check、tidy check。

推荐顺序：

```text
1. 先补 CMakePresets.json 和 compile_commands.json
2. 再补 VSCode settings
3. 再补 .clang-format
4. 再补 .clang-tidy
5. 最后补 CI 或本地 check 脚本
```

不要一开始就把所有检查开满。项目早期接口变化快，静态检查应该服务开发节奏，
不应该变成每天都要绕开的门槛。

## SPICEUnion 当前应该怎么做

SPICEUnion 当前已经具备：

- target-based CMake；
- C++17；
- GoogleTest；
- 默认测试与 external Spectre 测试分离；
- `.gitignore` 排除构建和 Spectre 产物。
- `CMakePresets.json` 固化 default / external 配置；
- `build/compile_commands.json` 可由 default preset 生成；
- `.vscode/settings.json` 让 clangd 读取 CMake 编译数据库；
- `.clang-format` 统一 C++ 格式化风格。

后续可选补充：

- `.clang-tidy`；
- 本地 `scripts/check.sh`；
- CI 配置。

因此，针对 VSCode 找不到 `su/evaluator.hpp`，优先检查：

```text
1. 用 cmake --preset default 重新 configure
2. 确认 build/compile_commands.json 存在
3. 重启 clangd 或 reload VSCode window
4. 确认当前 VSCode workspace 是项目根目录
```

这套配置完成后，VSCode 报找不到 `su/...` 公共头的概率会大幅降低。

## 判断优先级

遇到 VSCode 报错时，按下面顺序判断：

```text
1. cmake --build 是否通过？
   - 通过：大概率是编辑器配置问题
   - 不通过：是真实构建问题

2. compile_commands.json 是否存在？
   - 不存在：先导出
   - 存在：看 VSCode/clangd 是否指向正确目录

3. include path 是否来自 target？
   - 是：不要改源码 include
   - 否：修 CMake target_include_directories

4. 是否多个语言服务器同时工作？
   - 是：保留一个主要诊断源
```

一句话总结：

```text
C++ 标准开发配置的核心，是让编辑器读取真实构建系统，而不是让编辑器和 CMake 各猜一套世界。
```
