# SPICEUnion 开发环境配置

时间：2026-08-03

## 当前采用的路线

SPICEUnion 采用下面这条 VSCode / C++ 开发链路：

```text
CMake Tools
  -> 负责 configure / build / test

CMakePresets.json
  -> 固化 default / external 两套构建配置

compile_commands.json
  -> 由 CMake preset 生成，提供真实编译命令数据库

clangd
  -> 读取 build/compile_commands.json，负责 C++ 补全、跳转和诊断

cpptools
  -> 保留调试等辅助能力，不负责 IntelliSense 诊断
```

这条路线的目标是避免 VSCode 自己猜 include path，从而减少：

```text
'su/evaluator.hpp' file not found
```

这类编辑器误报。

## 已新增配置文件

### `CMakePresets.json`

定义两套 configure preset：

- `default`：普通开发配置，外部 Spectre 测试关闭。
- `external`：打开真实 Spectre 外部测试。

两套 preset 都打开：

```text
CMAKE_EXPORT_COMPILE_COMMANDS=ON
```

因此 configure 后会生成：

```text
build/compile_commands.json
cmake-build-external/compile_commands.json
```

### `.vscode/settings.json`

项目级 VSCode 设置采用：

```json
{
  "cmake.useCMakePresets": "always",
  "cmake.configureOnOpen": true,
  "C_Cpp.intelliSenseEngine": "disabled",
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/build",
    "--background-index",
    "--header-insertion=iwyu"
  ]
}
```

含义：

- VSCode CMake Tools 始终使用 `CMakePresets.json`。
- 打开工程时自动 configure。
- 关闭 cpptools IntelliSense，避免和 clangd 重复诊断。
- clangd 读取 `build/compile_commands.json`。
- clangd 后台建立索引，提升跳转和查找引用体验。
- clangd 按 include-what-you-use 风格辅助插入头文件。

### `.vscode/extensions.json`

记录本项目推荐插件：

- `llvm-vs-code-extensions.vscode-clangd`
- `ms-vscode.cmake-tools`
- `twxs.cmake`
- `ms-vscode.cpptools`

### `.clang-format`

使用 Google 风格作为基础，并按项目需要固定：

- C++17；
- 2 空格缩进；
- 100 字符行宽；
- 指针星号靠类型；
- 保留 include block；
- 不自动把短 if / loop 压成单行。

## 推荐命令

普通开发：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

真实 Spectre 外部测试：

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external
```

确认 clangd 编译数据库：

```bash
test -f build/compile_commands.json
```

## VSCode 操作方式

打开项目后，推荐通过命令面板执行：

```text
CMake: Select Configure Preset
CMake: Configure
CMake: Build
CMake: Run Tests
```

默认选择：

```text
default
```

需要真实 Spectre 验证时再选择：

```text
external
```

## 为什么不手写 includePath

不推荐在 `.vscode/c_cpp_properties.json` 里手写：

```json
{
  "includePath": [
    "${workspaceFolder}/include"
  ]
}
```

原因是这会让 VSCode 有一套独立于 CMake 的世界观。项目变大后，宏定义、编译选项、
第三方依赖路径、生成文件路径都可能和真实构建不一致。

本项目的原则是：

```text
CMake 是事实来源
compile_commands.json 是语言服务器入口
VSCode 不维护第二套 include path
```

## 如果 VSCode 仍然报 include 找不到

按下面顺序排查：

```bash
cmake --preset default
test -f build/compile_commands.json
```

然后在 VSCode 中执行：

```text
Developer: Reload Window
clangd: Restart language server
CMake: Configure
```

如果仍然有问题，检查 clangd 是否读取了正确目录：

```text
--compile-commands-dir=${workspaceFolder}/build
```

以及当前打开的 VSCode workspace 是否就是：

```text
~/my_lab/projects/SPICEUnion
```

## 注意事项

- `.vscode/settings.json` 是项目级设置，会影响打开本仓库的所有 VSCode 会话。
- 本项目选择 clangd 作为主 C++ 语言服务器。
- cpptools 没有卸载，只是不负责 IntelliSense；后续调试仍可使用它。
- external preset 会真实启动 Spectre，默认开发不要选它作为常驻配置。
