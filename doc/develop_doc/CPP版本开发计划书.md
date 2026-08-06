# SPICEUnion C++ 开发计划书

更新时间：2026-08-06

本文是 SPICEUnion 的主开发计划，只保留当前仍有效的项目范围、架构边界、已完成事实和后续开发目标。

## 1. 项目定位

SPICEUnion 是 C++17 仿真器执行与结果读取基础设施库，来源于
`~/my_lab/projects/spectre_materials/src/spectre_interactive/` 中已有 Python 执行路径的
C++ 化沉淀。

核心链路：

```text
ParameterState batch
  -> Evaluator
  -> ordered worker pool
  -> SimulatorSession
  -> worker work directory
  -> caller-owned result reading
  -> ordered TaskResult list
```

## 2. 范围

### 范围内

- batch 参数状态执行；
- worker 工作目录隔离；
- 输入顺序保序返回；
- 单任务失败隔离；
- Spectre interactive backend；
- Ngspice batch backend；
- 最小 ResultIR；
- 结果读取 helper；
- 可选 libpsf backend；
- GoogleTest 回归测试；
- fixture 驱动的结果读取验证。

### 范围外

- circuit metric / objective / penalty；
- optimizer；
- PDK 内容管理；
- GUI；
- 分布式调度；
- 完整 netlist IR；
- Hspice / Xyce backend；
- MATLAB binding。

## 3. 行为基线

行为基线来自：

- `~/my_lab/projects/spectre_materials/src/spectre_interactive/generic_evaluator.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/daemon_pool.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/spectre_daemon.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/task_library.py`

保留的行为契约：

- 输入 `states` 与输出 `TaskResult` 等长同序；
- 每个 worker 使用独立 work directory；
- 每个 evaluator 使用独立 namespace；
- startup failure 向上传播并清理已启动 worker；
- timeout / transport failure / simulation failure 有标准状态；
- 单任务失败不影响其他任务；
- 执行层只返回 work directory 与执行状态，业务解析由调用方负责。

不保留的 Python 习惯：

- 不用 `0.0`、空数组或 `None` 表达读取失败；
- 不把任意 Python parser 回调塞进 C++ core；
- 不把业务指标、目标函数或优化逻辑写进 SPICEUnion。

## 4. 当前架构

```text
include/su/
  core.hpp              ParameterState / EvaluatorOptions
  task_result.hpp       TaskStatus / TaskResult
  result.hpp            ResultIR / ReadResult
  result_reader.hpp     结果读取与数学 helper API
  session.hpp           SimulatorSession 抽象
  evaluator.hpp         Evaluator facade
  spectre_session.hpp   Spectre interactive backend
  ngspice_session.hpp   Ngspice batch backend

src/
  core/                 evaluator 实现
  pool/                 SimulatorPool adapter
  session/              Spectre / Ngspice backend 实现
  parse/                Result helper 与可选 libpsf backend

tests/
  fixtures/             小型固定结果样本
  support/              测试语义 helper
```

## 5. ResultIR

当前公开结果类型：

| 类型 | 含义 |
|---|---|
| `ScalarResult` | 单 signal 标量 |
| `DcSweep` | 单 sweep axis、单 signal 实数响应 |
| `AcResponse` | frequency + complex response |
| `AcDerivedView` | AC magnitude / phase 派生视图 |
| `TranWaveform` | time/value 波形 |
| `SensitivityEntry` | legacy sensitivity 原始条目 |

结果读取统一使用 `ReadResult<T>`，通过 `ResultStatus` 表达失败类型。

## 6. 当前已完成事实

| 阶段 | 当前事实 |
|---|---|
| M0 | CMake / GoogleTest 项目骨架已可用 |
| M1.0 | fake-session evaluator 契约测试已覆盖核心调度行为 |
| M1.1 | `SpectreSession` 生命周期闭环已实现 |
| M1.2 | Spectre 参数写入、单任务运行、失败状态已实现 |
| M1.3 | Spectre evaluator multi-worker batch 已实现 |
| M2.1 | ResultIR、`ReadResult<T>`、读取 API 骨架已实现 |
| M2.2 | `.raw` 定位、AC helper、UGBW/PM、settling time 已实现 |
| M2.3 | 可选 libpsf backend 已能读取 DC scalar、AC complex response、普通 TRAN waveform |
| M3.0 | Ngspice RC AC batch backend 已实现 |
| M3.1 | Ngspice RC TRAN batch backend 已实现 |
| M3.2 | Spectre / Ngspice 同类 AC 语义对照已完成 |
| M3.3 | Spectre / Ngspice 同类 TRAN 语义对照已完成 |
| M3.4 | Ngspice 电阻分压 DC sweep 与 `DcSweep` 已实现 |
| M3.5.0 | `SimulatorPool` 行为契约测试已补齐 |
| M3.5.1 | SPICEUnion 内部领域无关 `OrderedConcurrentPool` 核心已实现 |
| M3.5.2 | `SimulatorPool` 已改为 `OrderedConcurrentPool` adapter |
| M3.5.3 | default / external / libpsf 验证与边界检查已完成 |
| M3.5.4 | 独立 `OrderedConcurrentPool` 项目已创建并通过独立验证 |
| M3.5.5 | SPICEUnion 已装配外部 `OrderedConcurrentPool` 项目并移除内部副本 |

详细事实状态见：

- `doc/develop_doc/当前事实状态.md`

## 7. 当前验证入口

默认测试：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

外部 Spectre / Ngspice 测试：

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external --output-on-failure
```

libpsf backend 测试：

```bash
cmake -S . -B cmake-build-libpsf \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSPICEUNION_BUILD_TESTS=ON \
  -DSPICEUNION_ENABLE_EXTERNAL_TESTS=OFF \
  -DSPICEUNION_ENABLE_LIBPSF_READER=ON
cmake --build cmake-build-libpsf
ctest --test-dir cmake-build-libpsf --output-on-failure
```

最近已记录结果：

| 配置 | 结果 |
|---|---|
| default | `100% tests passed, 0 tests failed out of 80` |
| external | `100% tests passed, 0 tests failed out of 80` |
| libpsf full | `100% tests passed, 0 tests failed out of 90` |

## 8. 后续开发目标

### 8.1 M3 收敛

- 评估是否需要 Spectre 侧同类电阻分压 DC sweep fixture。
- 若进入 MOS I-V / gm/Id 场景，再评估 `DcSweep` 是否扩展为多曲线结构。
- 只有两个真实 backend 共同需要时，再扩展 `EvaluatorOptions` 或引入 netlist template / probe abstraction。

### 8.2 M3.5 OrderedConcurrentPool 提取

已完成：

- `SimulatorPool` 行为契约测试；
- 领域无关 `OrderedConcurrentPool` 核心抽出；
- `SimulatorPool` adapter；
- default / external / libpsf 验证；
- `OrderedConcurrentPool` 头文件边界检查；
- 独立 `OrderedConcurrentPool` 项目；
- 独立 CMake package；
- SPICEUnion 通过外部 `OrderedConcurrentPool` 项目装配。

未完成：

- `OrderedConcurrentPool` 明确开源许可证、benchmark 与发布准备。

详细阶段路线见：

- `doc/develop_doc/OrderedConcurrentPool开发路线图.md`

### 8.3 M4 C ABI / Python binding

进入条件：

- C++ ResultIR 与 evaluator API 继续稳定；
- C ABI ownership 规则明确；
- Python binding 只做薄封装，不重新承载调度逻辑。

### 8.4 M5 性能基准

进入条件：

- 有稳定 baseline；
- benchmark 数据可复现；
- 未实测的性能数字不得写成完成事实。

## 9. 当前未实现事项

- legacy sensitivity 读取；
- Spectre 23.1 PSFXL transient 解析或转换；
- 原生 PSF parser；
- C ABI 稳定化；
- Python / pybind11 binding；
- Ngspice `.op` operating point；
- MOS I-V / gm/Id 多曲线 DC 结果；
- Spectre 与 Ngspice 同类 DC sweep 语义对照；
- `OrderedConcurrentPool` 明确开源许可证、benchmark 与发布准备；
- Xyce / Hspice backend。
