# SPICEUnion C++ 开发计划书

更新时间：2026-08-11

本文是 SPICEUnion 的项目章程，只记录项目定位、范围边界、设计原则、总体架构和阶段摘要。当前已实现细节、测试结果和 fixture 清单以 `当前事实状态.md` 为准；后续施工步骤以 `开发路线图.md` 为准。

## 1. 项目定位

SPICEUnion 是 C++17 仿真器执行与结果读取基础设施库，来源于
`~/my_lab/projects/spectre_materials/src/spectre_interactive/` 中已有 Python 执行路径的
C++ 化沉淀。

主线收敛链路：

```text
netlist
  -> simulator selection
  -> simulator execution
  -> result directory
  -> result reader
  -> typed result
```

## 2. 范围边界

### 范围内

- 外部 netlist 执行入口；
- 仿真器选择；
- batch 参数状态执行；
- worker 工作目录隔离；
- 输入顺序保序返回；
- 单任务失败隔离；
- Spectre interactive backend；
- Ngspice batch backend；
- 最小 ResultIR；
- 结果读取 helper；
- 可选 libpsf backend；
- fixture 驱动的回归测试。

### 范围外

- circuit metric / objective / penalty；
- optimizer；
- PDK 内容管理；
- GUI；
- 分布式调度；
- 完整 netlist IR；
- SPICE 方言转换；
- analysis / probe DSL；
- MOS I-V / gm/Id 设计方法；
- C ABI / Python / MATLAB binding；
- Xyce / Hspice backend；

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

明确不继承的 Python 习惯：

- 不用 `0.0`、空数组或 `None` 表达读取失败；
- 不把任意 Python parser 回调塞进 C++ core；
- 不把业务指标、目标函数或优化逻辑写进 SPICEUnion。
- 不为假想场景提前设计完整输入 IR。

## 4. 架构边界

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

`SimulatorPool` 当前是 SPICEUnion 到外部 `OrderedConcurrentPool` 的 adapter。外部池项目的专项边界见 `OrderedConcurrentPool开发路线图.md`。

## 5. ResultIR 原则

ResultIR 只表达仿真结果的最小公共结构，不承载业务指标、优化目标或电路设计策略。

当前公开结果类型包括：

- `ScalarResult`
- `DcSweep`
- `AcResponse`
- `AcDerivedView`
- `TranWaveform`
- `SensitivityEntry`
- `ReadResult<T>`

具体字段、读取 helper 与验证状态见 `当前事实状态.md`。

## 6. 里程碑摘要

| 阶段 | 状态 | 摘要 |
|---|---|---|
| M0 | 已完成 | CMake / GoogleTest 项目骨架 |
| M1 | 已完成 | Spectre evaluator 生命周期、参数写入、multi-worker batch |
| M2 | 已完成 | ResultIR、结果读取 helper、可选 libpsf backend |
| M3 | 已完成基础闭环 | Ngspice AC / TRAN / DC sweep 与 Spectre / Ngspice AC、TRAN 对照 |
| M3.5 | 已完成 | OrderedConcurrentPool 抽离、独立化、MIT 发布、CI |
| 主线收敛 | 待评估 | 最小外部 netlist 执行入口、必要结果读取 fixture |

当前阶段事实见 `当前事实状态.md`。下一步施工路线见 `开发路线图.md`。

## 7. 后续方向

近期只保留一条方向：

1. 主线收敛：支持最小外部 netlist 执行入口，并按真实输出补充必要结果读取 fixture。

当前不推进完整 netlist IR、SPICE 方言转换、analysis / probe DSL、MOS I-V / gm/Id、optimizer、C ABI / Python binding、Xyce / Hspice backend 或 native PSF parser。
