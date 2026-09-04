# 用户工作流 API 设计

状态：`active`
最后验证：`2026-09-02`
适用范围：`C++ workflow facade / Python workflow binding / future MATLAB bridge`
单一事实来源：

- `include/su/workflow.hpp`
- `src/workflow/`
- `tests/unit/workflow/workflow_test.cpp`

阶段收口记录：`../10_阶段记录/05_M5_用户工作流API.md`

## 1. 要解决的问题

SPICEUnion 当前已经具备底层执行层、结果读取层、多 backend 基础能力和 Python 结果读取
binding，但普通用户主路径仍偏底层。用户需要理解 `Evaluator`、`TaskResult.work_dir`、
`.raw` 目录、`find_result_directory()`、`ResultFormat` 和具体 reader helper 才能串起一次
完整仿真。

用户工作流 API 要解决的是：为 C++、Python 和未来 MATLAB 入口提供同一套低负担使用逻辑。

```text
用户提供合法网表
  -> 用户声明可变参数
  -> 用户提交参数组合并启动仿真
  -> 用户基于结果对象读取信号
  -> 用户自行搭建上层指标提取
```

普通用户不应直接面对并行调度、多 backend class、worker directory、`.raw` 目录、PSF /
wrdata 分派或 libpsf 后端细节。

## 2. 不解决的问题

第一版 C++ workflow API 不做以下事情：

- 不实现 optimizer、搜索算法、objective、penalty 或业务 metric 系统。
- 不实现完整 netlist IR、netlist template DSL 或 PDK 内容管理。
- 不实现 GUI。
- 不实现分布式调度。
- 不实现完整 Ngspice、Hspice 或 Xyce 支持。
- 不直接暴露 `SimulatorSession`、`SessionFactory`、`SpectreSession`、`NgspiceSession`
  或 `OrderedConcurrentPool`。
- 不在第一版实现 MATLAB binding。
- 不改变现有 `Evaluator`、`TaskResult`、`ReadResult<T>` 和 `result_reader` 的底层职责。

`Evaluator`、`TaskResult` 和 `result_reader` 继续作为高级 API 或内部实现层存在；workflow
API 是用户层 facade，不是替代底层模块的重写。

## 3. 用户主路径

### 3.1 C++ 目标形态

```cpp
#include "su/workflow.hpp"

su::SimulationOptions options;
options.simulator = su::SimulatorKind::kSpectre;
options.netlist_path = "input.scs";
options.workers = 4;

su::Simulation simulation(options);
simulation.add_parameter("wp");
simulation.add_parameter("wn");

const auto results = simulation.run({
    {{"wp", 14e-6}, {"wn", 10e-6}},
    {{"wp", 16e-6}, {"wn", 11e-6}},
});

for (const auto& result : results) {
  if (!result.ok()) {
    continue;
  }

  auto dc = result.read_dc("out");
  auto ac = result.read_ac("out");
  auto tran = result.read_tran("out");
}
```

### 3.2 Python 后续目标形态

```python
import spiceunion as su

simulation = su.Simulation(
    simulator="spectre",
    netlist_path="input.scs",
    workers=4,
)

simulation.add_parameter("wp")
simulation.add_parameter("wn")

results = simulation.run([
    {"wp": 14e-6, "wn": 10e-6},
    {"wp": 16e-6, "wn": 11e-6},
])

for result in results:
    if not result.ok():
        continue

    dc = result.read_dc("out")
    ac = result.read_ac("out")
    tran = result.read_tran("out")
```

### 3.3 MATLAB 后续目标形态

MATLAB 第一版暂缓。长期目标是在语义上保持同一套用户模型：

```matlab
simulation = spiceunion.Simulation( ...
    "simulator", "spectre", ...
    "netlistPath", "input.scs", ...
    "workers", 4);

simulation.addParameter("wp");
simulation.addParameter("wn");

results = simulation.run([
    struct("wp", 14e-6, "wn", 10e-6)
    struct("wp", 16e-6, "wn", 11e-6)
]);

ac = results(1).readAc("out");
```

MATLAB 具体接入方式后续再评估 C ABI + MEX 或 JSON/CLI bridge。

## 4. 第一版 C++ API

```cpp
namespace su {

enum class SimulatorKind {
  kSpectre,
  kNgspice,
};

struct SimulationOptions {
  SimulatorKind simulator = SimulatorKind::kSpectre;
  std::string netlist_path;
  int workers = 1;
  std::string work_dir_base = "local/runtime/simulations";
  std::string workspace_namespace;
  int timeout_seconds = 60;
  int restart_attempts = 1;
  ResultFormat result_format = ResultFormat::kUnknown;
  NgspiceBuiltinTask ngspice_task = NgspiceBuiltinTask::kRcAc;
};

using SimulationCase = std::map<std::string, double>;

class SimulationResult {
 public:
  bool ok() const noexcept;
  TaskStatus status() const noexcept;
  std::string status_text() const;
  const std::string& message() const noexcept;

  const std::string& work_dir() const noexcept;
  ResultFormat result_format() const noexcept;

  ReadResult<ResultDirectory> result_directory() const;
  ReadResult<ScalarResult> read_dc(const std::string& signal_name) const;
  ReadResult<DcSweep> read_dc_sweep(const std::string& sweep_name,
                                    const std::string& signal_name,
                                    const std::string& filename = "dc.dc") const;
  ReadResult<AcResponse> read_ac(const std::string& signal_name,
                                 const std::string& filename = "ac.ac") const;
  ReadResult<TranWaveform> read_tran(const std::string& signal_name,
                                     const std::string& filename = "tran.tran") const;
};

class Simulation {
 public:
  explicit Simulation(SimulationOptions options);

  void add_parameter(std::string name);
  void add_parameter(std::string name, double default_value);

  std::vector<SimulationResult> run(const std::vector<SimulationCase>& cases);

  void cleanup() noexcept;
  const SimulationOptions& options() const noexcept;
  const std::string& workspace_root() const noexcept;
};

}  // namespace su
```

说明：

- `Simulation` 是普通用户主入口。
- `SimulationResult` 是任务结果和结果读取的统一 facade。
- `work_dir()` 与 `result_format()` 可以保留为高级诊断字段，但不作为普通读取主路径。
- 文件名参数保留默认值，便于高级用户读取非默认 analysis 文件，例如 `stb.stb`。

## 5. 内部映射

```text
SimulationOptions
  -> EvaluatorOptions

Simulation
  -> make_spectre_evaluator(options)
  -> make_ngspice_evaluator(options, task)
  -> Evaluator::run(states)

SimulationCase
  -> ParameterState

SimulationResult
  -> TaskResult
  -> find_result_directory(task.work_dir)
  -> read_dc_value / read_dc_sweep / read_ac_response / read_tran_waveform
```

第一版优先复用现有实现，不复制调度、session、parser 或结果数学 helper。

## 6. 参数契约

workflow API 增加“声明可变参数”的用户层契约。

当前规则：

- `add_parameter(name)` 声明一个必须由每个 case 提供的参数。
- `add_parameter(name, default_value)` 声明一个可由默认值补齐的参数。
- 参数名不能为空。
- 重复声明同名参数应拒绝。
- case 中出现未声明参数应拒绝整批。
- case 缺少无默认值参数应拒绝整批。
- case 缺少有默认值参数时由 workflow 层补齐。
- 参数值必须是 finite double。

这种校验发生在启动仿真前，避免把结构性输入错误推迟到 simulator 日志里才暴露。

## 7. 失败语义

workflow API 应保持三层失败语义：

| 层级 | 场景 | 表达方式 |
|---|---|---|
| 批量输入结构非法 | 未声明参数、缺少必填参数、非 finite 参数、无效 options | `Simulation` 构造或 `run()` 拒绝整批 |
| 单个仿真任务失败 | startup、timeout、transport、simulation failure | `SimulationResult.ok() == false` |
| 单个结果读取失败 | 文件缺失、signal 缺失、格式不支持、解析失败 | `ReadResult<T>.ok() == false` |

原则：

- 用户输入批量本身非法时，不启动仿真。
- 单个仿真点失败时，不污染同批其他结果。
- 单个 signal 读取失败时，不伪装成 `0.0`、空数组或 `None`。

## 8. 测试策略

`tests/unit/workflow/workflow_test.cpp` 覆盖用户层契约：

- `SimulationOptions` 非法配置拒绝。
- 参数声明拒绝空名和重复名。
- `run({})` 返回空结果。
- 未声明参数拒绝整批。
- 缺少无默认值参数拒绝整批。
- 缺少有默认值参数时补齐。
- 非 finite 参数拒绝整批。
- batch 结果保持输入顺序。
- 单任务失败封装为 `SimulationResult`。
- `SimulationResult.read_*()` 自动使用 `TaskResult.work_dir` 与 `TaskResult.result_format`
  调用结果读取层。

测试设计需要处理一个问题：普通 workflow API 不应暴露底层 `SessionFactory`，但测试需要 fake
session。候选方案：

1. 增加高级测试/嵌入入口 `make_simulation_for_session_factory(options, factory)`。
2. 将注入 factory 的构造函数放在非主路径 API 中，并在 README 中不作为普通用户入口宣传。
3. 只通过真实 Ngspice/Spectre 测 workflow run，不推荐作为第一版唯一测试路径，因为默认测试不应依赖外部 EDA 工具。

推荐采用方案 1。

## 9. 开发切分

当前按两轮落地。

第一轮：C++ workflow facade，已完成。

- 新增 `include/su/workflow.hpp`。
- 新增 `src/workflow/simulation.cpp` 与 `src/workflow/README.md`。
- 更新 `CMakeLists.txt`。
- 新增 `tests/unit/workflow/workflow_test.cpp`。
- 更新根 README 主示例。
- 更新 `../00_项目总览/01_当前事实状态.md`、`../00_项目总览/02_架构总览.md`、
  `../00_项目总览/03_开发路线图.md`。
- 更新 `include/su/README.md`、`src/README.md`、`tests/README.md`。

第二轮：Python workflow binding，已作为 M6 第一版完成。

- 在 C++ workflow API 稳定后，将 Python 执行层 binding 绑定到 workflow 层。
- 不直接暴露 `SimulatorSession`、`SessionFactory` 或 `OrderedConcurrentPool`。
- 继续复用 Python 结果对象当前的 `ok()`、`status_text()`、`message` 失败语义。
- 阶段记录见 `../10_阶段记录/06_M6_Python工作流Binding.md`。
- Python API 草案见 `03_Python工作流Binding设计.md`。

## 10. 完成定义

第一轮完成事实：

- C++ 用户可以通过 `Simulation` 提供网表、声明参数、提交 batch 并获得 `SimulationResult`。
- 成功任务可通过 `SimulationResult.read_dc()`、`read_dc_sweep()`、`read_ac()`、
  `read_tran()` 读取结果。
- 普通用户主路径不需要直接调用 `find_result_directory()`。
- workflow API 契约已有默认测试覆盖。
- 默认预设构建与测试通过。
- 根 README 主示例已改为 workflow API。
- 当前事实、架构总览、路线图和相关目录 README 已完成收口。

## 11. 后续维护规则

本文件作为 workflow 设计说明保留，状态为 `active`。

后续 Python workflow binding 或 MATLAB bridge 若改变本文件中的用户模型、失败语义或内部映射，
应同步更新本文；若本文内容完全被当前事实、架构总览和 README 吸收，可移动到
`../30_归档备注/` 或删除。
