# build 说明

## 功能

- 本目录是 SPICEUnion 的本地 CMake 构建产物根目录。
- 本目录不存放源码、手写测试 fixture、正式开发文档或长期实验结论。

## 本级模块职责

- `default/`：默认开发构建产物，对应 `cmake --preset default`。
- `external/`：启用真实 Spectre / Ngspice 外部测试的构建产物，对应 `cmake --preset external`。
- `external-libpsf/`：同时启用外部 Spectre / Ngspice 测试与 `henjo/libpsf` reader 的构建产物，
  对应 `cmake --preset external-libpsf`（真实网表端到端验证用）。
- `libpsf/`：启用可选 `henjo/libpsf` PSF reader backend 的构建产物，对应 `cmake --preset libpsf`。
- `python/`：启用 pybind11 Python binding 的构建产物，对应 `cmake --preset python`。
- `python-libpsf-pic/`：同时启用 Python binding 与 libpsf backend 的构建产物，对应 `cmake --preset python-libpsf-pic`。

## 命名规则

- 本目录下的一级构建目录名应与 `CMakePresets.json` 中的 preset 名保持一致。
- 新增构建类型时，应优先新增稳定语义的 CMake preset，再生成同名构建目录。
- 不使用 `tmp`、`new`、`final`、`test2`、`v2` 这类阶段性目录名作为正式构建入口。

## 当前约定

- 除本 README 外，本目录下的构建产物不进入版本库。
- 构建入口以 `CMakePresets.json` 为单一事实来源，不在本目录维护独立构建参数。
- VSCode / clangd 默认读取 `build/default/compile_commands.json`。
- 如果构建缓存异常，可以删除对应 preset 目录后重新运行 `cmake --preset <preset>`。
