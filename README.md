# SPICEUnion

SPICEUnion 是一个 C++17 项目，用于重写并沉淀当前位于
`~/my_lab/projects/spectre_materials/src/spectre_interactive/` 的
Spectre 交互式执行层。

项目目标是把已有 Python 执行路径整理为一个紧凑、可测试、可嵌入的仿真器基础设施库：

```text
states
  -> evaluator facade
  -> ordered concurrent worker pool
  -> simulator sessions
  -> worker work directories
  -> caller-owned parsing / result handling
  -> ordered results
```

## 当前状态

当前仓库已完成 M0、M1.0、M1.1、M1.2、M1.3、M2.1、M2.2、M2.3、M3.0、M3.1、M3.2、
M3.3，并已完成 M3.4 的 Ngspice `.dc` 简单 sweep 最小接入：

- M0：CMake / GoogleTest 项目骨架已可用。
- M1.0：核心 evaluator 契约已有 fake-session 测试覆盖。
- M1.1：`SpectreSession` 可以启动 Spectre interactive 模式，完成 SKILL
  握手，初始化 circuit handle，并停止进程。
- M1.2：`SpectreSession` 可以格式化 SKILL 参数写入命令，触发
  `(sclRun "all")`，等待完成，并报告 timeout / transport / simulation
  失败。
- M1.3：默认 Spectre evaluator factory 可以通过 evaluator / pool 路径运行真实
  multi-worker batch。
- M2.1：最小 ResultIR、`ReadResult<T>`、结果读取 API 骨架与基础单元测试已可用。
- M2.2：`.raw` 目录定位、AC magnitude / phase、UGBW / phase margin、waveform
  settling time 等纯路径 / 纯数学 helper 已实现。
- M2.3：`henjo/libpsf` 已完成本地 CMake spike；显式启用
  `SPICEUNION_ENABLE_LIBPSF_READER` 时，`read_dc_value()` 可读取 `dcOp.dc`
  单信号 scalar，`read_ac_response()` 可读取 swept complex response，
  `read_tran_waveform()` 可读取普通 time-sweep `tran.tran`。默认构建仍不依赖
  libpsf。第一批 PSF fixture 已固化到 `tests/fixtures/psf/`。
- M3.0：`NgspiceSession` 已作为第二个真实 simulator backend 接入；当前支持内置
  RC low-pass AC batch 示例，调用 `ngspice -b` 生成三列 `wrdata v(out)` 文本输出，
  并映射到 M2 `AcResponse`。默认测试不依赖 Ngspice，外部测试显式启用后会真实运行
  Ngspice 并验证 -3 dB 频率。
- M3.1：`NgspiceSession` 增加 `NgspiceBuiltinTask`，当前可选择内置 RC low-pass
  AC 或 RC charging TRAN。TRAN 路径调用 `ngspice -b` 生成两列
  `wrdata v(out)` 文本输出，并映射到 M2 `TranWaveform`；external 测试会验证
  RC 充电曲线在 `τ = RC`、`5τ` 与终点处符合理论预期。
- M3.2：新增 Spectre RC low-pass AC 源 fixture，并用本机 Spectre 23.1 生成普通
  PSF fixture `spectre_rc_lowpass_ac.raw/ac.ac`。新增 `tests/support/rc_semantics.hpp`
  与 `spiceunion_simulation_semantics_test`，让 Spectre PSF 读取结果和 Ngspice external
  AC 结果复用同一套 RC low-pass AC 语义检查。
- M3.3：新增 Spectre RC charging TRAN 源 fixture，并用本机 Spectre 23.1 生成可由
  libpsf backend 读取的普通 transient PSF fixture
  `spectre_rc_charging_tran.raw/tran.tran.tran`。Spectre PSF 读取结果和 Ngspice
  external TRAN 结果复用同一套 RC charging `TranWaveform` 语义检查。
- M3.4：新增最小 `DcSweep` ResultIR，并为 `NgspiceSession` 增加电阻分压
  `kResistorDividerDc` 内置任务。该任务每次 run 调用一次 `ngspice -b`，执行
  `.dc Vin start stop step`，生成两列 `wrdata v(out)` 文本输出，并映射为
  `DcSweep`。当前选择纯电阻分压，避免把 MOS 模型依赖混入 `.dc` 基础能力验证。

下一阶段若继续 M3，应优先判断是否需要 Spectre 侧同类 DC sweep fixture，或是否等到
MOS I-V / gm/Id 这类真实消费者出现后再扩展更复杂的 DC 结果形态；legacy sensitivity
与 Spectre 23.1 PSFXL transient 仍保留为边界事项。
Python `task_library.py` 作为历史参考和 fixture 来源，但 C++ API 不为强行兼容 Python
返回习惯而牺牲类型安全。

## 范围

V1 聚焦执行层：

- evaluator options 与 batch execution facade；
- evaluator namespace 与 worker directory 隔离；
- 与任务完成顺序无关的 ordered result collection；
- 使用 idle-worker queue scheduling，而不是静态 round-robin dispatch；
- worker 启动失败传播与清理；
- per-task failure isolation；
- 显式 cleanup 与 worker reload；
- fake-session 契约测试稳定后支持 Spectre interactive session；
- 执行核心稳定后补充底层 result helper 与 native PSF parsing。

V1 不负责：

- 项目特定 circuit metrics；
- optimization strategies；
- objective functions 或 scoring logic；
- experiment orchestration；
- PDK content management；
- GUI 工作；
- distributed scheduling。

这些职责应由上层项目承担。

## 行为基线

行为基线来自当前 Python package：

- `~/my_lab/projects/spectre_materials/src/spectre_interactive/generic_evaluator.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/daemon_pool.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/spectre_daemon.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/task_library.py`

SPICEUnion 应保留 `GenericEvaluator.run(states, parse_func)` 中真正有价值的行为契约，
但不复制 Python 的具体模块布局。

## 仓库结构

当前文档：

- `TODO`：当前未完成任务、暂缓池与监控项。
- `doc/README.md`：文档目录职责与命名规则。
- `doc/develop_doc/README.md`：开发文档目录职责与命名规则。
- `doc/develop_doc/CPP版本开发计划书.md`：架构、里程碑、契约与验收标准。
- `doc/develop_doc/M2结果层职责边界与契约.md`：M2 ResultIR、结果读取 helper、
  失败语义与职责边界。
- `doc/develop_doc/M2-libpsf-spike记录.md`：libpsf 可选 backend 的当前状态、
  fixture、测试结果、license 风险与 native parser 决策口径。
- `doc/develop_doc/M3-Ngspice最小接入记录.md`：Ngspice 最小 batch adapter、
  RC AC 示例、输出解析、测试状态与当前边界。
- `doc/develop_doc/M3.1-Ngspice瞬态与跨后端AC语义对照.md`：Ngspice RC
  charging TRAN 接入、`NgspiceBuiltinTask`、TRAN 输出解析与 AC/TRAN 语义检查。
- `doc/develop_doc/M3.2-Spectre与Ngspice同类AC语义对照.md`：Spectre RC
  low-pass AC 源 fixture、固化 PSF 结果、公共 RC AC 语义测试 helper 与当前结论。
- `doc/develop_doc/M3.3-Spectre与Ngspice同类TRAN语义对照.md`：Spectre RC
  charging TRAN 源 fixture、普通 transient PSF 读取、公共 RC TRAN 语义测试 helper
  与当前结论。
- `doc/develop_doc/M3.4-Ngspice直流扫描最小接入记录.md`：Ngspice `.dc`
  电阻分压 sweep、`DcSweep` ResultIR、两列 `wrdata` 解析与当前边界。
- `doc/develop_doc/开发路线图.md`：分阶段实现任务、文件产出、测试产出、完成定义与
  commit 边界。
- `doc/develop_doc/简历亮点解析.md`：面向面试的项目叙事与简历定位。
- `doc/study_notes/README.md`：可复用学习笔记目录职责与命名规则。

计划中的实现结构：

```text
SPICEUnion/
├── CMakeLists.txt
├── include/su/
├── src/
│   ├── core/
│   ├── pool/
│   ├── session/
│   └── parse/
├── bindings/
│   └── python/
├── tests/
│   └── fixtures/
├── bench/
└── scripts/
```

## M0 启动条件

M0 满足以下条件时视为完成：

- 根 README 和 TODO 能清楚描述项目状态；
- 开发计划书不再使用从 `spectre_materials` 迁移过来的模糊表述；
- C++ core、C ABI 与 Python binding 的边界已明确；
- 最小 CMake 项目可以 configure、build，并运行 smoke test；
- fake 或 scripted session 测试覆盖 namespace isolation、startup failure、
  ordered result collection、idle-worker scheduling 与 per-task failure
  isolation。

## 构建与测试

默认测试不需要外部 EDA 工具：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

外部 Spectre 生命周期测试需要显式启用：

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external
```

外部测试依赖：

- `spectre` 位于 `PATH` 中；
- `/dev/shm/pdk_cache/toplevel.scs`;
- `~/my_lab/projects/spectre_materials/netlist/AMP/dc/input.scs`.
- `ngspice_con` 或 `ngspice` 位于 `PATH` 中；也可通过 `SPICEUNION_NGSPICE`
  指向可执行文件。

外部测试当前覆盖 Spectre handshake、single-task run 与 multi-worker evaluator
batch execution，也覆盖 Ngspice RC AC / RC TRAN / resistor-divider DC session 与
evaluator batch。

## 备注

本仓库使用 Git 管理。按照项目协作约定，代码、结构或流程发生变化后，应及时进行本地
commit。
