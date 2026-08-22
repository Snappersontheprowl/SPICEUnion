# SPICEUnion

SPICEUnion 是一个 C++17 的**仿真器执行与结果读取基础设施库**：把一批参数化仿真任务
交给真实仿真器（Spectre / Ngspice）批量执行，并把仿真结果统一读取成结构化数据，
供上层算法、优化器或工具链直接使用。

## 项目是什么、解决什么问题

电路设计、优化和参数扫描类项目经常需要成百上千次仿真：改参数 → 跑仿真 → 读结果 →
再改参数。如果每次都手工调用仿真器、手工解析输出，流程既慢又脆弱。SPICEUnion 把这套
重复劳动抽象成一个可嵌入的库：

- 一次提交一批参数状态（`ParameterState` batch）；
- 库负责拉起 / 复用真实仿真器、并发执行、按输入顺序返回结果；
- 仿真产物（`.raw`）留在每个 worker 自己的工作目录，由调用方用统一 reader 读取；
- 单个任务失败、超时或仿真器崩溃不会影响其他任务。

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

## 核心能力

### 执行层

- batch 执行 facade（`Evaluator`），一次提交一批参数状态；
- worker 工作目录隔离，多 worker 并行；
- 输入顺序保序返回，不依赖任务完成顺序；
- 单任务失败隔离；启动失败 / 超时 / 传输失败有标准 `TaskStatus`；
- Spectre interactive backend：SKILL handshake、参数写入、`(sclRun "all")`、完成判定；
- Ngspice batch backend。

### 结果读取层

- 统一 ResultIR：`ScalarResult`、`DcSweep`、`AcResponse`、`AcDerivedView`、
  `TranWaveform`；
- `.raw` 目录定位与 PSF 文件读取（可选 libpsf backend，默认关闭）；
- Ngspice `wrdata` AC / TRAN / DC sweep 文本读取；
- AC 数学 helper：magnitude/phase、UGBW、phase margin、settling time。

### 多语言接入

- C++17 公开 API（`include/su/`）；
- 可选 pybind11 Python 绑定（结果读取 helper 与结果类型），模块名 `spiceunion`；
- C ABI 目前为草案 / 暂缓状态。

## 优势（为什么用它）

- **面向嵌入**：是库不是工具，可放进优化器、参数搜索、设计空间探索等自己的流程里；
- **批量化与保序**：并行执行，结果仍按提交顺序返回，上层无需关心调度细节；
- **统一结果模型**：不同仿真器（Spectre / Ngspice）的输出归一为同一套 ResultIR；
- **失败可控**：单任务失败 / 超时 / 仿真器崩溃被隔离并标准映射，不污染整批结果；
- **依赖可控**：默认构建不依赖任何 EDA 工具；libpsf、外部测试、Python 绑定均为可选开关；
- **可扩展**：新增仿真器只需实现 `SimulatorSession` 适配器。

## 边界与不足

- 不做 circuit metric / objective / penalty、optimizer、PDK 内容管理、GUI、
  完整 netlist IR；
- 解析边界（经真实网表实测）：PSFXL transient 明确返回 `unsupported_format`；
  libpsf backend 对 Spectre 23.1 的 PSFASCII 输出存在兼容缺口（既有 fixture
  均为 BINPSF，ASCII 路径未覆盖）；legacy sensitivity 未实现；原生 PSF parser
  未实现；
- Python 侧当前只能读取结果，不能发起仿真（执行层绑定暂缓）；
- 尚未发布 wheel / package；性能数字未系统实测。

能力边界由 `external-libpsf` 预设的多网表矩阵测试持续钉住，详细事实与验证数字见
`doc/develop_doc/当前事实状态.md`。

## 与 spectre_materials 的关系

SPICEUnion 最初参照 `~/my_lab/projects/spectre_materials`（Python 版 Spectre 执行与
解析路径）的设计，但现在是**独立演进的项目**：

- 运行时：SPICEUnion 不依赖 spectre_materials；
- 验证期材料：外部测试可选消费 `spectre_materials/external/` 下的网表与 PDK，通过
  `SPICEUNION_SPECTRE_MATERIALS_DIR` 注入；
- 职责不同：spectre_materials 是共享材料与参考实现（Python），SPICEUnion 是 C++17
  基础设施库。

## 快速开始

### 环境要求

- CMake 3.20+ 与 C++17 编译器；
- sibling 项目 `~/my_lab/projects/OrderedConcurrentPool`（可通过
  `SPICEUNION_ORDERED_POOL_SOURCE_DIR` 覆盖）；
- 真实仿真（可选）：`spectre` 位于 PATH，并具备网表与 PDK 材料。

### 构建与测试

默认构建不依赖外部 EDA 工具：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

各预设用途：

| 预设 | 用途 |
|---|---|
| `default` | 默认开发测试，不调用外部 EDA 工具 |
| `external` | 启用真实 Spectre / Ngspice 外部测试 |
| `libpsf` | 启用 libpsf PSF 结果读取 |
| `external-libpsf` | 真实网表端到端：外部仿真 + libpsf 结果解析 |
| `python` | 启用 pybind11 Python 绑定 |
| `python-libpsf-pic` | Python 绑定 + libpsf（PIC 静态链接） |

### C++ 最小示例

```cpp
#include "su/evaluator.hpp"
#include "su/result_reader.hpp"

su::EvaluatorOptions options;
options.netlist_path = "input.scs";   // 参数化 Spectre 网表
options.num_workers = 4;

auto evaluator = su::make_spectre_evaluator(options);
auto results = evaluator.run({{"wp", 14e-6}, {"wn", 10e-6}});

// results[i].work_dir 即该任务的真实仿真产物目录（.raw）
auto sweep = su::read_dc_sweep(results[0].work_dir, "vin_dc", "out");
```

读取 PSF 文件需要启用 libpsf 的构建；参数名需与网表中的参数一致。

### Python 示例（构建 python 预设后）

```bash
PYTHONPATH=build/python/bindings/python python3.10 -c \
  "import spiceunion; print(spiceunion.version())"
```

更多示例见 `bindings/python/examples/`。

## 仓库结构

```text
include/su/      公开 C++ API
src/core/        evaluator 与通用执行逻辑
src/pool/        SimulatorPool adapter
src/session/     Spectre / Ngspice backend
src/parse/       ResultIR helper 与可选 libpsf backend
bindings/python/ 可选 pybind11 Python binding
tests/           GoogleTest 测试与 fixture
doc/             开发文档、学习笔记与简历材料
local/           本机运行产物与外部依赖构建产物（不入库）
build/           CMake 构建产物（不入库）
```

## 文档入口

- `doc/develop_doc/README.md`：开发文档地图与维护规则；
- `doc/develop_doc/CPP版本开发计划书.md`：项目章程（定位、范围、设计原则、里程碑）；
- `doc/develop_doc/架构总览.md`：分层架构、执行链路与结果读取链路图；
- `doc/develop_doc/当前事实状态.md`：当前能力、验证数字与边界总账；
- `doc/develop_doc/开发路线图.md`：后续施工路线；
- `doc/resume/`：简历与面试表达材料。

## 外部依赖

外部测试材料来源：

| 资源 | 来源 | 覆盖变量 |
|---|---|---|
| Spectre 基线网表 | `<spectre_materials>/external/netlist/AMP/dc/input.scs` | `SPICEUNION_SPECTRE_MATERIALS_DIR` |
| PDK（tsmcN65 toplevel） | `<spectre_materials>/external/pdk/tsmcN65/toplevel.scs` | 同上 |
| libpsf | `local/external/libpsf/install[-pic]` | `SPICEUNION_LIBPSF_INCLUDE_DIR` / `SPICEUNION_LIBPSF_LIBRARY` |
| OrderedConcurrentPool | sibling 源树 | `SPICEUNION_ORDERED_POOL_SOURCE_DIR` |

`SPICEUNION_SPECTRE_MATERIALS_DIR` 默认指向 `~/my_lab/projects/spectre_materials`；
外部测试还需要 `spectre` 位于 `PATH`，Ngspice 位于 `PATH`（或通过
`SPICEUNION_NGSPICE` 指定）。基线网表内部以绝对路径 include PDK，跨机器需要
等价材料布局；本机 `/dev/shm/pdk_cache` 缓存已废弃，不再作为检查条件。
`spectre_materials/external/netlist` 下的网表均为项目所有者实测的合法仿真网表，SPICEUnion
只作为消费方读取，不修改网表内容。
