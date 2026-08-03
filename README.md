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

当前仓库已完成 M0、M1.0、M1.1、M1.2、M1.3、M2.1、M2.2，并已推进
M2.3 的 libpsf 可选 DC 读取链路：

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
- M2.3：`henjo/libpsf` 已完成本地 CMake spike；`read_dc_value()` 在显式启用
  `SPICEUNION_ENABLE_LIBPSF_READER` 时可读取 `dcOp.dc` 单信号 scalar。默认构建
  仍不依赖 libpsf。第一批 PSF fixture 已固化到 `tests/fixtures/psf/`。

下一阶段继续 M2.3：补齐标准 ac/tran/sens fixture 与最小文件读取能力。
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
- `doc/develop_doc/M2-libpsf评估与接入策略.md`：libpsf 在 M2 中作为可选
  backend、fixture oracle 与 native parser 决策参考的策略。
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

外部测试当前覆盖 Spectre handshake、single-task run 与 multi-worker evaluator
batch execution。

## 备注

本仓库使用 Git 管理。按照项目协作约定，代码、结构或流程发生变化后，应及时进行本地
commit。
