# tests 说明

## 功能

- 存放 SPICEUnion 基于 GoogleTest 的自动化测试，覆盖执行层契约、ResultIR 与结果读取、
  跨 backend 结果语义对照，以及真实 Spectre / Ngspice 生命周期验证。
- 不负责存放真实仿真历史产物；除 `fixtures/` 固定样本外，运行期产物由各用例写入临时
  目录并自行清理。

## 本级模块职责

- `CMakeLists.txt`：测试 target 注册入口；根 `CMakeLists.txt` 只负责
  `add_subdirectory(tests)`。
- `unit/`：默认可跑的 C++ 单元/契约测试，不调用真实 EDA 工具，详见
  `unit/README.md`。
- `integration/`：默认可跑的跨模块语义测试，使用仓库内固定 fixture，详见
  `integration/README.md`。
- `external/`：依赖真实 Spectre / Ngspice 或私有材料的外部测试，默认 skip，详见
  `external/README.md`。
- `support/`：测试共享 helper（如 `rc_semantics.hpp`、`ngspice_external_env.hpp`），
  不含被测逻辑。
- `fixtures/`：已提交的小体积固定样本（Spectre 源 netlist 与 PSF 结果），详见
  `fixtures/README.md`。
- `manual/`：人工验证工具，不进入默认 ctest，详见 `manual/README.md`。

## 命名规则

- 测试文件按被测模块命名，统一 `snake_case` 并以 `_test` 结尾，例如
  `evaluator_contract_test.cpp`、`simulator_pool_contract_test.cpp`。
- 顶层目录按测试性质分层：`unit/` 默认单元/契约测试，`integration/` 默认集成语义
  测试，`external/` 真实工具/私有材料依赖测试，`fixtures/` 固定样本，
  `support/` 共享 helper，`manual/` 人工工具。
- `unit/` 下按源码模块或公共职责继续分层，例如 `core/`、`parse/`、`pool/`、
  `session/`、`workflow/`、`toolchain/`；仿真器探测测试用 fake 可执行文件，
  不调用真实 EDA 工具。
- `external/` 下按真实工具或外部系统分层，例如 `spectre/`、`ngspice/`。

## 当前约定

- 默认构建（`SPICEUNION_ENABLE_EXTERNAL_TESTS=OFF`）不调用任何外部 EDA 工具：
  Spectre / Ngspice 外部用例在测试体开头 `GTEST_SKIP()`，不产生真实仿真产物。
- 仿真产物统一写入 `<项目根>/local/runtime/<场景>/`（场景名说明来源与用途）；
  `Evaluator::cleanup()` 只停止仿真进程，不删除产物目录，是否清理由用例决定。
- C++ 测试注册的单一事实来源为 `tests/CMakeLists.txt`；根 `CMakeLists.txt` 只保留
  测试开关和子目录入口。
- 测试构建开关仍由根 `CMakeLists.txt` 定义（`SPICEUNION_BUILD_TESTS`、
  `SPICEUNION_ENABLE_EXTERNAL_TESTS`、libpsf / python 开关）。
- 各预设的验证数字不在本 README 维护，统一见 `doc/develop_doc/00_项目总览/01_当前事实状态.md`。

## 常用入口

- 默认测试（无需外部工具）：

```bash
cmake --preset default && cmake --build --preset default && ctest --preset default --output-on-failure
```

- 外部 Spectre / Ngspice：

```bash
cmake --preset external && cmake --build --preset external && ctest --preset external --output-on-failure
```

- 真实网表端到端（外部仿真 + libpsf 解析）：

```bash
cmake --preset external-libpsf && cmake --build --preset external-libpsf && ctest --preset external-libpsf --output-on-failure
```

- libpsf / python / python-libpsf-pic 等其他预设及外部依赖说明见根 `README.md`。
