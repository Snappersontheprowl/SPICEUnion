# local 说明

## 功能

- 本目录存放 SPICEUnion 在当前机器上的本地运行产物、外部依赖构建产物和临时工作数据。
- 本目录不存放正式源码、可复用测试 fixture、项目开发文档或需要跨机器共享的配置。

## 本级模块职责

- `external/`：存放本机外部依赖源码、构建目录或安装产物，例如用于验证的 `henjo/libpsf` 本地构建结果。
- `runtime/`：存放本机运行 SPICEUnion、Spectre、Ngspice 或相关验证流程时产生的临时输入、输出和工作目录。

## 命名规则

- 本目录下的一级目录应使用稳定职责名，例如 `external/`、`runtime/`。
- 外部依赖相关内容应放在 `external/<dependency_name>/` 下。
- 运行产物相关内容应放在 `runtime/<scenario_name>/` 下，场景名应能说明来源或用途。
- 不使用 `tmp`、`new`、`final`、`test2`、`v2` 这类阶段性目录名作为正式入口。

## 当前约定

- 除本 README 外，本目录下的内容不进入版本库。
- 可复用、可审查的小型测试数据应放入 `tests/fixtures/`，不要长期留在 `local/`。
- 构建 libpsf 等外部依赖时，可以把本机安装结果放在 `local/external/` 下，由 CMake 按项目配置查找。
- 运行失败排查完成后，可以清理 `runtime/` 下不再需要的临时目录。
