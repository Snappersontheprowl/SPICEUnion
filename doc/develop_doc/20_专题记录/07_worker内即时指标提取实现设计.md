# worker 内即时指标提取实现设计（无兼容包袱版）

状态：`active`（设计定稿，代码待启动）
最后更新：`2026-10-05`
适用范围：`产物生命周期 / worker 内指标提取 / C++ 与 Python 主路径`
关联文档：

- `06_产物生命周期与指标提取设计.md`（方案对比与实验记录；本文件是方案一的落地设计）
- `../00_项目总览/01_当前事实状态.md`（事实与已知缺陷总账）
- `../00_项目总览/02_架构总览.md`（执行链路与模块责任）
- `../00_项目总览/03_开发路线图.md`（路线 3.5）

## 1. 前提与决策

前提：项目**没有真实用户**。PyPI 上的 `spiceunion 0.1.0` 只是公开产物，不代表
存在需要兼容的使用方；按 SemVer 0.x 约定，本次按**破坏性变更**处理，版本升
`0.2.0` 并在 CHANGELOG 说明。

由此确定三个决策：

1. **不做兼容矩阵**：指标提取直接作为**主路径**，`read_*` 降级为“高级/调试”
   接口；不为旧行为保留双路径、废弃标记或默认值妥协。
2. **产物生命周期一步到位**：每 job 独立产物目录（Ngspice 直接支持，Spectre 用
   快照），不再走“先快照过渡、以后再改”的两步走。
3. **产物保留策略不在本方案范围**：保持现状（产物保留在 job 目录、`read_*`
   继续可用）；如未来需要“成功丢弃/限量保留”等策略，另立专题评估。

## 2. 目标与非目标

目标：

- 每个 job 的指标在 **worker 归还池子之前**完成提取，彻底消除产物覆盖风险；
- 用户以数据声明描述要什么（信号 + 派生指标），库在 worker 内调用既有 reader；
- C++ 与 Python 语义一致，且默认路径就是“拿指标”，不再让用户先想“怎么读文件”。

非目标：

- 库不定义业务指标（objective/penalty/优化目标）；
- 不做 netlist DSL / 模板注入（专题 06 的 P3）；
- 不做**方案二：仿真器原生测量直出**（Ngspice `.meas`、Spectre `measure` /
  `mdl*`）——它是独立的产物生成方式改造，见专题 06 第 4 节，不在本文件范围；
- 不暴露 Python 回调在 worker 内执行（GIL 风险与收益不匹配，列为远期）。

## 3. 产物生命周期（先决条件）

| 后端 | 机制 | 依据 |
|---|---|---|
| Ngspice | 每 job 独立子目录，作为 `ngspice -b` 子进程工作目录 | 专题 06 实验 C 已验证（三 job 隔离、读数与理论一致） |
| Spectre | 常驻会话无法切换 raw 目录；改用**每 job 快照**（产物保留） | 专题 06 实验 A/B（`sclSetResultDir` 不改变 raw 位置） |

目录命名（保序、可追溯）：

```text
workspace_root/worker_<id>/case_<输入下标>/
```

产物策略：保持现状——产物保留在 job 目录，`read_*` 继续可用。

## 4. 用户可见接口（主路径）

### 4.1 C++

```cpp
struct SimulationOptions {
  // ...既有字段保持不变
  std::vector<MetricRequest> metrics;   // 新增，默认空 = 不提取
};

class SimulationResult {
 public:
  bool metrics_ok() const noexcept;                  // 提取是否成功
  const std::string& metrics_message() const;        // 失败原因
  const std::vector<MetricValue>& metrics() const;   // 提取结果（SI 单位）
  const MetricValue* metric(const std::string& name) const;  // 便捷访问
  // read_* 保留为高级/调试接口：仅在产物被保留时可用
};
```

### 4.2 Python

```python
sim = su.Simulation(
    netlist_path="input.scs", simulator="spectre", workers=4,
    metrics=[
        {"kind": "dc",   "signal": "vout"},
        {"kind": "ac",   "signal": "out", "derived": ["ugbw", "phase_margin"]},
        {"kind": "tran", "signal": "out", "derived": ["settling_time"]},
    ],
)
results = sim.run(cases)

r = results[0]
r.ok()               # 仿真是否成功（既有语义）
r.metrics_ok()       # 指标提取是否成功（新语义，与仿真状态分离）
r.metrics()          # {"vout": 0.8, "ugbw_hz": 1.2e8, "settling_time_s": 4.6e-9}
r.metric("ugbw_hz")  # 便捷访问，不存在返回 None
```

`read_*` 的行为保持不变（产物仍保留）；提取结果通过 `metrics_ok()` /
`metrics_message()` / `metrics()` 读取，两者互不影响。

## 5. 数据模型

```cpp
enum class MetricKind { kDcValue, kDcSweep, kAcResponse, kTranWaveform };
enum class DerivedMetric { kUgbwHz, kPhaseMarginDeg, kSettlingTimeS };

struct MetricRequest {
  MetricKind kind = MetricKind::kDcValue;
  std::string signal;                    // 必填，如 "vout" / "out"
  std::string filename;                  // 可选，默认按格式约定
  std::vector<DerivedMetric> derived;    // 可选派生指标
};

struct MetricValue {
  std::string name;                      // "vout" / "ugbw_hz" ...
  double value = 0.0;                    // SI
  std::string unit;                      // "V" / "A" / "Hz" / "s" / "deg"
  MetricsStatus status = MetricsStatus::kOk;
  std::string message;
};

struct MetricsOutcome {
  MetricsStatus status = MetricsStatus::kOk;
  std::string message;
  std::vector<MetricValue> values;
};
```

`MetricsStatus`：`kOk` / `kSignalNotFound` / `kUnsupportedFormat` / `kReadFailed` /
`kInternalError`。空请求不产生状态（`metrics()` 为空、`metrics_ok()` 为真）。

## 6. 执行时序与线程模型

```text
worker 线程：
  session.run(state)                  -> TaskResult（仿真状态 + job 目录）
  extract_metrics(job_dir, requests)  -> MetricsOutcome（worker 归还前完成）
  返回 {TaskResult, MetricsOutcome}
```

- 提取在 worker 线程内完成：跨 worker 并行，并与其它 worker 的仿真重叠；
- 单 worker 内串行；不做单文件内部并行；
- 提取异常必须在 worker 内捕获并映射为 `kInternalError`，不跨线程抛出。

## 7. 失败语义

| 场景 | 仿真状态 | 指标状态 | 说明 |
|---|---|---|---|
| 仿真成功 + 指标可读 | success | ok | 主路径 |
| 仿真成功 + 信号不存在 | success | signal_not_found | 逐指标记录，不否定仿真 |
| 仿真成功 + 格式不支持（PSFXL / 无 libpsf 的 BINPSF） | success | unsupported_format | 与专题 04 分类一致 |
| 仿真成功 + reader 异常 | success | internal_error | 带 message |
| 仿真失败 | 失败状态 | read_failed | 无产物可提取，raw 保留 |
| 未声明指标 | success | ok（空集） | 不做特殊状态 |

## 8. Python 绑定要点

1. `metrics=` 接收 `list[dict]`，绑定层做字段校验（缺 `signal`、未知 `kind` 直接
   抛 `ValueError`）；
2. 提取在 C++ 内完成，绑定层只在**主线程**转换结果——不涉及跨线程 GIL；
3. 不暴露 Python 回调（远期若要做，必须 `gil_scoped_acquire` + 异常映射 +
   说明 GIL 串行化代价）；
4. `Simulation.run()` 现有 `gil_scoped_release` 保持不变。

## 9. 分阶段实施与完成定义

### P0 产物正确性（0.5–1 天）

- [x] Ngspice：`NgspiceSession::set_job_work_dir()` 直接写入每 job 目录；
  Spectre：池子在 job 结束后把固定 worker 目录快照到每 job 目录；
- [x] **回归测试**：`tests/unit/pool/job_work_dir_contract_test.cpp`
  （fake session，默认预设可跑）：三个 job 的 `work_dir` 互不相同，且各自产物
  内容对应自己的输入参数；
- [x] 真实验证（2026-10-05）：
  - Ngspice `workers=1`，3 个不同电容：目录 `worker_0/case_00000{0,1,2}`，
    -3 dB 频率 159.14 / 79.57 / 39.79 MHz（理论 159.15 / 79.58 / 39.79 MHz）；
  - Spectre `workers=1`，2 个不同电容：目录 `worker_0/case_00000{0,1}`，快照文件
    各自持有且 `ac.ac` 哈希不同（修复过程中发现并修掉“把上一个 job 目录也复制进
    新 job”的缺陷）；
- [x] `01_当前事实状态.md` 的缺陷状态已更新为「P0 修复记录」。

### P1 metrics 主路径（1–1.5 天）

- [x] 数据模型（第 5 节）与 `include/su/metrics.hpp`；
- [x] worker 内提取（DC / DC sweep / AC 派生 / TRAN 派生；PSF 与 Ngspice wrdata
  分派）；
- [x] 提取与仿真状态分离（`metrics_ok()` / `metrics_message()`）；**产物保留策略
  不在本方案范围**（保持现状：产物保留，`read_*` 可用），后续如需再单独立项；
- [x] Python 绑定（`metrics=` / `metrics()` / `metric()` / `metrics_ok()` /
  `metrics_message()` / `metric_values()`）；
- [x] 示例：`bindings/python/examples/metrics_only.py`；
- [x] 文档：`usage/README.md`、`include/su/README.md`、事实状态、CHANGELOG。

验证（2026-10-05）：default 112/112、python 120/120、python-libpsf-pic 139/139；
Ngspice 三 case 的 settling 时间各异且 `read_tran` 仍可读；Spectre + libpsf 的
AC 派生指标同样通过。

完成定义：`workers=4`、8 个不同 case 的批量跑完，指标与输入一一对应；成功样本
不再产生可读 raw（默认），失败样本可排查。

### 后续（不属于本文件范围）

方案二“仿真器原生测量直出”见专题 06 第 4 节；它改变的是“指标由谁计算、是否产生
大波形”，与方案一（提取时机）正交，可组合但需独立设计、独立实施。

## 10. 测试矩阵

| 层 | 用例 | 预期 |
|---|---|---|
| 单元 | 数据模型/枚举/请求校验 | 通过 |
| 契约（fake session） | 提取时序、失败矩阵、成功丢产物 | 提取在归还前完成；状态分离 |
| 真实 Ngspice | `workers=1`，3 个不同 C | 指标不同且与理论一致 |
| 真实 Ngspice | `workers=4`，8 个 case | 指标与输入一一对应 |
| 真实 Spectre | 单/多 case（复用私有材料） | 提取成功；多 case 指标不同 |
| Python | 契约 + 最小示例 | 语义与 C++ 一致，无 GIL 问题 |

性能不做承诺；若测量需记录机器、case 数与解析/仿真时间比。

## 11. 风险与缓解

| 风险 | 缓解 |
|---|---|
| 解析占用 worker 时间 | 只取标量/派生；记录解析/仿真比；必要时加提取超时 |
| 批量指标常驻内存 | 只回传标量与派生值，波形不内联 |
| 成功丢产物导致无法复查 | 失败必留 raw；单任务调试场景后续提供开关 |
| Spectre 每 job 快照的拷贝成本 | 产物 12–48KB 量级；如遇大 tran 再评估即时提取 |

## 12. 版本与变更记录

- 版本：`0.2.0`：新增 `metrics=` 指标主路径；既有 `read_*` 行为不变；
- `CHANGELOG.md` 记录上述破坏性变化与迁移说明（旧用户升级到 0.2.0 的注意事项）。

## 13. 文档同步清单（实施时执行）

- 事实：`01_当前事实状态.md`（新能力 + 缺陷状态）；
- 架构：`02_架构总览.md`（worker 内“仿真 → 提取 → 产物策略”链路）；
- 路线图/TODO：`03_开发路线图.md` 3.5 与根 `TODO` 勾选收口；
- 用户：`usage/README.md`（`metrics=` 最小示例与 `read_*` 新语义）；
- 测试：`tests/README.md` / `tests/unit/README.md`；
- 变更：`CHANGELOG.md`、根 `README.md` 边界措辞。
