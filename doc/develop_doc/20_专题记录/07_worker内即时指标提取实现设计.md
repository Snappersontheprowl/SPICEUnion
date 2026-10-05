# worker 内即时指标提取实现设计（方案一落地）

状态：`active`（设计定稿，代码待启动）
最后更新：`2026-10-05`
适用范围：`执行层 worker 产物消费时机 / 提取声明 / C++ 与 Python 接口`
关联文档：

- `06_产物生命周期与指标提取设计.md`（方案对比与选型；本文件是其方案一的落地设计）
- `../00_项目总览/01_当前事实状态.md`（事实与已知缺陷总账）
- `../00_项目总览/02_架构总览.md`（执行链路与模块责任）
- `../00_项目总览/03_开发路线图.md`（路线 3.5）
- `04_仿真器自动适配与诊断设计.md`（能力/格式边界）

## 1. 目标与非目标

目标：

- 让**每个 job 的指标在 worker 归还池子之前**完成提取，从根上消除“产物被下一个
  job 覆盖”的正确性风险；
- 用户以“数据声明”描述要什么（信号 + 派生指标），库负责在 worker 内调用既有
  reader 提取，并随结果回传；
- C++ 与 Python 都能用同一套语义，且默认不破坏现有代码。

非目标：

- 库不定义业务指标（objective、penalty、优化目标）；
- 不实现 netlist DSL / 模板注入（见专题 06 的 P3）；
- 不在本期支持 Python 回调在 worker 内执行（列为后续进阶项，见第 7 节）。

## 2. 关键设计约束

1. **时机**：`session.run()` 返回后、worker 归还池子前完成提取；这是方案一唯一能
   消除覆盖的时序保证（参考 Python 版参考实现的 `_worker_task` 实践）。
2. **状态分离**：仿真状态（`TaskStatus`）与提取状态（`ExtractionStatus`）分开，
   不允许“提取失败”把仿真判为失败，也不允许静默丢弃。
3. **边界**：提取内容必须由用户声明；库只做“读信号 + 已定义的派生计算”。
4. **默认兼容**：不声明提取时行为与今天一致（覆盖缺陷由正确性修复单独兜底，
   见第 8 节）。

## 3. 数据模型（C++）

```cpp
enum class ExtractionKind { kDcValue, kDcSweep, kAcResponse, kTranWaveform };

enum class ExtractedMetric { kUgbwHz, kPhaseMarginDeg, kSettlingTimeS };

struct ExtractionRequest {
  ExtractionKind kind = ExtractionKind::kDcValue;
  std::string signal;           // 必填，例如 "vout" / "out"
  std::string filename;         // 可选：默认按格式约定
  std::vector<ExtractedMetric> derived;   // 可选派生指标
};

struct MetricValue {
  std::string name;             // "vout" / "ugbw_hz" ...
  double value = 0.0;           // SI 单位
  std::string unit;             // "V" / "A" / "Hz" / "s" / "deg"
  ExtractionStatus status = ExtractionStatus::kOk;
  std::string message;
};

struct ExtractionOutcome {
  ExtractionStatus status = ExtractionStatus::kOk;
  std::string message;
  std::vector<MetricValue> metrics;
};
```

`ExtractionStatus` 取值（与专题 04 的错误分类保持一致）：

```text
kOk / kNotRequested / kSignalNotFound / kUnsupportedFormat / kReadFailed / kInternalError
```

单位约定：数值统一 SI，`unit` 仅用于展示；派生指标命名固定（如 `ugbw_hz`、
`phase_margin_deg`、`settling_time_s`），避免上层猜名字。

## 4. 用户可见接口

### 4.1 C++（兼容增量）

```cpp
struct SimulationOptions {
  // ...既有字段保持不变
  std::vector<ExtractionRequest> extraction;                  // 新增，默认空
  ArtifactRetention retention = ArtifactRetention::kKeepOnFailure;  // 新增
};

class SimulationResult {
 public:
  // ...既有 ok()/status_text()/read_* 保持不变
  bool extraction_ok() const noexcept;                 // 新增
  const std::string& extraction_message() const;        // 新增
  const std::vector<MetricValue>& metrics() const;      // 新增
};
```

### 4.2 Python（同一语义）

```python
sim = su.Simulation(
    netlist_path="input.scs", simulator="spectre", workers=4,
    extraction=[
        {"kind": "dc",   "signal": "vout"},
        {"kind": "ac",   "signal": "out", "derived": ["ugbw", "phase_margin"]},
        {"kind": "tran", "signal": "out", "derived": ["settling_time"]},
    ],
    retention="keep_on_failure",     # keep_all / keep_on_failure / discard
)
results = sim.run(cases)

r = results[0]
r.ok()                    # 仿真是否成功（既有语义）
r.extraction_ok()         # 提取是否成功（新增）
r.metrics()               # dict[str, float]：{"vout": 0.8, "ugbw_hz": 1.2e8}
r.metric("ugbw_hz")       # 便捷访问；不存在返回 None
```

兼容性：不传 `extraction` 时 `metrics()` 返回空、`extraction_ok()` 返回
`False`（`kNotRequested`），老代码无需改动。

## 5. 执行时序与线程模型

```text
worker 线程：
  session.run(state)                  -> TaskResult（仿真状态 + work_dir）
  extract(task, options.extraction)   -> ExtractionOutcome（在 worker 归还前完成）
  apply_retention(task, outcome, policy)
  返回 {TaskResult, ExtractionOutcome} -> 池子归还 worker
```

- 提取在 **worker 线程**内执行，天然跨 worker 并行，并与其它 worker 的仿真重叠；
- 单 worker 内串行；不提供单文件内部并行解析；
- 提取异常必须在 worker 内捕获并转成 `kInternalError`，不得跨线程抛出；
- 可选 `extraction_timeout_seconds`：超时判 `kReadFailed`，避免慢性解析拖住 worker。

## 6. 失败语义矩阵

| 场景 | 仿真状态 | 提取状态 | 说明 |
|---|---|---|---|
| 仿真成功 + 全部指标可读 | success | ok | 正常路径 |
| 仿真成功 + 信号不存在 | success | signal_not_found | 逐指标记录，不否定仿真 |
| 仿真成功 + 格式不支持（PSFXL / 无 libpsf 的 BINPSF） | success | unsupported_format | 与专题 04 分类一致 |
| 仿真成功 + reader 抛异常 | success | internal_error | 带 message，不跨线程抛出 |
| 仿真失败 | 失败状态 | not_requested | 无产物可提取 |
| 未声明提取 | success | not_requested | 老行为 |

## 7. Python 绑定实现要点

1. **数据结构注册**（`bindings/python/spiceunion_py.cpp`）：`ExtractionKind` /
   `ExtractionStatus` 用 `py::enum_`；`MetricValue` 用只读 `py::class_`；
   `extraction=` 接收 `list[dict]`，在绑定层做参数校验并给出明确异常；
2. **无回调路径（本期）**：提取全部在 C++ 内完成，绑定层只在**主线程**把结果转成
   Python 对象——不涉及 GIL 跨线程问题；
3. **不暴露 Python 回调**：`on_job_result=` 列为后续进阶项；若将来支持，必须
   `py::gil_scoped_acquire` + 异常映射 + 回调串行化说明（GIL 下 Python 解析无法
   并行），并在文档中明确“解析重时改用声明式路径”；
4. `Simulation.run()` 现有的 `gil_scoped_release` 保持不变：仿真与提取期间不持
   GIL，仅返回结果时持 GIL。

## 8. 与产物保留策略、既有 API 的关系

| retention | 成功 job 的 raw | 失败 job 的 raw | `read_*` 可用性 |
|---|---|---|---|
| `keep_all` | 保留 | 保留 | 始终可用（与今天一致） |
| `keep_on_failure`（默认建议） | 丢弃 | 保留 | 成功样本调用 `read_*` 返回 `artifacts_not_retained` |
| `discard` | 丢弃 | 丢弃 | 一律不可用（仅供只取指标的批处理） |

引入 `artifacts_not_retained` 需要新增一枚 `ResultStatus`（增量枚举）。
`keep_on_failure` 是“性能与可调试性”的折中；完全依赖 `read_*` 的用户显式选
`keep_all` 即可，行为与今天一致。

注意：**方案一本身不修复“未声明提取用户”的覆盖问题**；该正确性由专题 06 的
Ngspice 每 job 子目录 / Spectre 快照兜底（P0）独立完成。

## 9. 分步实施与完成定义

### P1.1 数据模型与契约（0.5 天）

- 新增公开头（建议 `include/su/extraction.hpp`）：`ExtractionKind` /
  `ExtractionRequest` / `MetricValue` / `ExtractionOutcome` / `ExtractionStatus` /
  `ArtifactRetention`；
- 单元测试：默认构造、枚举文本、字段校验。

### P1.2 worker 内提取（1 天）

- `SimulatorPoolWorker::run()` 在 `session_->run()` 之后调用提取器；
- 提取器复用 `result_reader` 的 `read_dc_value` / `read_dc_sweep` /
  `read_ac_response` / `read_tran_waveform` 与派生 helper；
- 失败矩阵（第 6 节）逐项覆盖；`extraction_timeout_seconds` 生效；
- **回归测试（先失败后通过）**：`workers=1` + 3 个不同参数 case，断言三者
  `metrics()` 互不相同且与理论一致（Ngspice RC AC 用 -3 dB 频率即可）。

### P1.3 保留策略（0.5 天）

- `ArtifactRetention` 三档落地；`artifacts_not_retained` 状态接入 `read_*`；
- 测试：`keep_on_failure` 下失败样本可读、成功样本返回明确状态。

### P1.4 Python 绑定（1 天）

- `extraction=` / `retention=` 关键字 + `metrics()` / `metric()` /
  `extraction_ok()` / `extraction_message()`；
- 契约测试：参数校验、空提取、缺信号、格式不支持、与 `read_*` 的组合；
- 示例：`bindings/python/examples/` 增加“只取指标不读波形”的最小示例。

### P1.5 文档与回归（0.5 天）

- 更新 `01_当前事实状态.md`（能力 + 已知缺陷状态）、`usage/README.md`（用户用法）、
  `tests/README.md`、根 README（边界措辞）、`CHANGELOG.md`；
- 回归：`default` / `python` / `python-libpsf-pic` / `external-libpsf` 全绿。

合计约 3.5 天（不含 P0 正确性修复）。

## 10. 测试矩阵

| 层 | 用例 | 预期 |
|---|---|---|
| 单元 | 数据结构/枚举/参数校验 | 通过 |
| 契约（fake session） | worker 内提取时序、失败矩阵、超时 | 提取在归还前完成；状态分离 |
| 真实 Ngspice | `workers=1`，3 个不同 C 的 RC AC | 每个 case 指标不同且与理论一致 |
| 真实 Ngspice | `workers=4`，8 个 case | 指标与输入一一对应，无覆盖 |
| 真实 Spectre | 单 case + 多 case（复用既有外部材料） | 提取成功；多 case 指标不同 |
| Python | 契约 + 最小示例 | 与 C++ 语义一致；无 GIL 崩溃 |
| 保留策略 | `keep_all` / `keep_on_failure` / `discard` | 与第 8 节表格一致 |

性能不做承诺；如测量，需记录机器、case 数、解析/仿真时间比。

## 11. 风险与缓解

| 风险 | 缓解 |
|---|---|
| 解析占用 worker 时间导致吞吐下降 | 只取标量/派生；提供 timeout；记录解析/仿真比 |
| 批量指标常驻内存 | 只回传标量与派生值；波形类不内联返回 |
| 用户误以为 `read_*` 永远可用 | 文档 + `artifacts_not_retained` 明确失败状态 |
| Python 回调（未来）GIL 串行/崩溃 | 本期不暴露回调；未来用 `gil_scoped_acquire` + 异常映射并文档化 |
| 与 P0 正确性修复重叠/冲突 | 先落 P0（快照或每 job 目录），再做 P1；共用同一回归测试 |

## 12. 待决策项

- 默认 `retention`：`keep_on_failure`（建议）还是 `keep_all`？
- 返回形态：`dict[str, float]`（建议 MVP）还是类型化 `MetricValue` 列表？
- `extraction_timeout_seconds` 默认值（建议 0 = 不额外限制，只受 job 超时约束）；
- 派生指标首期集合：仅 `ugbw` / `phase_margin` / `settling_time`，是否加 `overshoot`？

## 13. 文档同步清单（实施时执行）

- 事实：`01_当前事实状态.md`（新增能力；已知缺陷状态改为“已修复/部分修复”）；
- 架构：`02_架构总览.md`（worker 内“仿真 → 提取 → 保留策略”链路）；
- 路线图/待办：`03_开发路线图.md` 3.5 与根 `TODO` 的勾选与收口；
- 用户：`usage/README.md` 增加“只取指标”的最小示例与保留策略说明；
- 测试：`tests/README.md` / `tests/unit/README.md`（新增提取测试目录）；
- 变更记录：`CHANGELOG.md`（新增 `extraction` / `retention` / 新状态）。
