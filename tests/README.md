# tests 说明

## 功能

- 存放 SPICEUnion 基于 GoogleTest 的自动化测试，覆盖执行层契约、ResultIR 与结果读取、
  跨 backend 结果语义对照，以及真实 Spectre / Ngspice 生命周期验证。
- 不负责存放真实仿真历史产物；除 `fixtures/` 固定样本外，运行期产物由各用例写入临时
  目录并自行清理。

## 本级模块职责

- `smoke_test.cpp`：验证构建与链接路径（版本符号可用）。
- `evaluator_contract_test.cpp`：验证 `Evaluator` batch facade 契约（保序返回、失败
  隔离、worker 目录隔离、清理可重复）。
- `ordered_concurrent_pool_test.cpp`：通过外部 `ocp::ordered_concurrent_pool` target
  验证领域无关池语义（构造校验、启动清理、保序返回、异常转换、重复 shutdown、
  stress batch）。
- `simulator_pool_contract_test.cpp`：验证 `SimulatorPool` 作为 SPICEUnion adapter 的
  契约（worker work directory、`TaskResult` 映射、保序与失败隔离）。
- `result_test.cpp`：验证 ResultIR、`ReadResult` 状态与公开 result_reader API 骨架。
- `result_reader_test.cpp`：验证 `.raw` 目录定位、AC 数学 helper、settling time 与
  文件读取行为；真实 PSF 读取由 libpsf 开关控制。
- `simulation_semantics_test.cpp`：使用 `support/rc_semantics.hpp` 的公共语义 helper，
  验证不同 backend 读出的 ResultIR 满足同一类电路的物理语义（AC -3 dB、TRAN `τ`、
  DC 分压比例）。
- `spectre_protocol_test.cpp`：纯单元测试，验证 SKILL 命令格式与 completion 行分类，
  不依赖真实 spectre。
- `ngspice_session_test.cpp`：默认验证 Ngspice 配置、netlist 渲染与 `wrdata` 输出
  解析；启用外部测试后真实调用 `ngspice -b` 跑 AC / TRAN / DC batch。
- `support/`：测试共享 helper（如 `rc_semantics.hpp`），不含被测逻辑。
- `fixtures/`：已提交的小体积固定样本（Spectre 源 netlist 与 PSF 结果），详见
  `fixtures/README.md`。
- `external/`：外部 Spectre 测试（生命周期契约 + 真实网表端到端仿真/解析），依赖
  `spectre_materials/external/` 网表与 PDK，默认 skip，详见 `external/README.md`。
- `manual/`：人工验证工具，不进入默认 ctest，详见 `manual/README.md`。

## 命名规则

- 测试文件按被测模块命名，统一 `snake_case` 并以 `_test` 结尾，例如
  `evaluator_contract_test.cpp`、`simulator_pool_contract_test.cpp`。
- 目录按职责命名：`fixtures/` 固定样本、`support/` 共享 helper、`external/` 外部
  材料依赖测试、`manual/` 人工工具。

## 当前约定

- 默认构建（`SPICEUNION_ENABLE_EXTERNAL_TESTS=OFF`）不调用任何外部 EDA 工具：
  Spectre / Ngspice 外部用例在测试体开头 `GTEST_SKIP()`，不产生真实仿真产物。
- 仿真产物统一写入 `<项目根>/local/runtime/<场景>/`（场景名说明来源与用途）；
  `Evaluator::cleanup()` 只停止仿真进程，不删除产物目录，是否清理由用例决定。
- 测试注册与构建开关的单一事实来源为根 `CMakeLists.txt`
  （`SPICEUNION_BUILD_TESTS`、`SPICEUNION_ENABLE_EXTERNAL_TESTS`、libpsf / python 开关）。
- 各预设的验证数字不在本 README 维护，统一见 `doc/develop_doc/当前事实状态.md`。

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
