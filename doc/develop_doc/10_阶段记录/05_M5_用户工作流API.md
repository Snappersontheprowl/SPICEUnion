# M5 用户工作流 API 开发文档

更新时间：2026-09-02

本文负责 SPICEUnion M5 阶段的开发背景、阶段边界和状态收口。

当前结论：

```text
M5.0 C++ 用户工作流 facade 已完成；
普通 C++ 用户主路径收敛到 Simulation / SimulationResult；
Python workflow binding 不进入 M5.0，已作为 M6 第一版完成；
MATLAB bridge 继续暂缓。
```

## 1. M5 目标

M5 的目标是把 SPICEUnion 的一次仿真使用路径从底层执行对象提升到用户工作流对象。

用户需要面对的概念应稳定为：

```text
合法网表
  -> 可变参数声明
  -> 参数组合 batch
  -> 仿真启动
  -> 结果对象
  -> 上层指标提取
```

内部的并行 worker、session、backend、`.raw` 目录、Ngspice wrdata 文件、PSF 解析和
`ResultFormat` 分派可以继续存在，但不应成为普通用户完成一次仿真的必要知识。

## 2. 已确认开发基线

| 事项 | M5.0 基线 |
|---|---|
| 第一实现语言 | C++ |
| 公开入口 | `include/su/workflow.hpp` |
| 实现目录 | `src/workflow/` |
| 用户主对象 | `Simulation` |
| 单点结果对象 | `SimulationResult` |
| 参数输入对象 | `SimulationCase` |
| 配置对象 | `SimulationOptions` |
| 支持 backend | Spectre / Ngspice 第一版分派 |
| 参数契约 | 显式声明参数，run 前校验整批结构 |
| 失败语义 | 构造或整批输入错误抛异常；单任务失败进入 `SimulationResult`；读取失败进入 `ReadResult<T>` |
| 测试入口 | `tests/unit/workflow/workflow_test.cpp`，纳入默认 CTest |

## 3. M5.0 范围

### 3.1 范围内

- 新增 C++ workflow facade。
- 复用现有 `Evaluator`、`TaskResult` 和 `result_reader`。
- 允许用户选择 `SimulatorKind::kSpectre` 或 `SimulatorKind::kNgspice`。
- 支持声明必填参数和带默认值参数。
- 在启动底层 evaluator 前拒绝结构性非法 batch。
- 保持 batch 输出与输入同序。
- 使用 `SimulationResult` 封装单任务执行状态。
- 使用 `SimulationResult.read_*()` 封装常用结果读取入口。
- 保留 `work_dir()` 和 `result_format()` 作为高级诊断字段。
- 增加 workflow 契约测试和目录 README。

### 3.2 范围外

- 不实现 Python workflow binding。
- 不实现 MATLAB binding。
- 不实现 C ABI。
- 不实现 optimizer、objective、penalty 或业务 metric 系统。
- 不实现 netlist template DSL、完整 netlist IR 或 PDK 管理。
- 不实现 GUI。
- 不实现分布式调度。
- 不把底层 `SimulatorSession`、`SessionFactory`、`SpectreSession`、`NgspiceSession`
  或 `OrderedConcurrentPool` 作为普通用户 API 暴露。

## 4. 已实现 API

M5.0 新增：

- `SimulatorKind`
- `SimulationOptions`
- `SimulationCase`
- `Simulation`
- `SimulationResult`
- `make_simulation_for_session_factory()`

普通调用方入口示例：

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

  auto ac = result.read_ac("out");
}
```

API 详细契约见 `../20_专题记录/02_用户工作流API设计.md`。

## 5. 内部映射

```text
SimulationOptions
  -> EvaluatorOptions

SimulationCase
  -> ParameterState

Simulation::run()
  -> 参数声明校验
  -> 默认参数补齐
  -> Evaluator::run()
  -> vector<TaskResult>
  -> vector<SimulationResult>

SimulationResult.read_*()
  -> TaskResult.work_dir
  -> TaskResult.result_format
  -> result_reader helper
  -> ResultIR / ReadResult<T>
```

M5.0 的关键取舍是只增加用户层 facade，不重写调度层、session 层或 parser 层。

## 6. 失败语义

M5.0 固定三类失败边界：

| 层级 | 场景 | 表达方式 |
|---|---|---|
| workflow 输入非法 | 无效 options、未声明参数、缺少必填参数、非 finite 参数 | 构造或 `run()` 抛 `std::invalid_argument` |
| 单个仿真任务失败 | startup、timeout、transport、simulation failure | `SimulationResult.ok() == false` |
| 单个结果读取失败 | 文件缺失、signal 缺失、格式不支持、解析失败 | `ReadResult<T>.ok() == false` |

原则：

- 结构性输入错误不启动仿真。
- 单点失败不污染同批其他点。
- 结果读取失败不伪装成零值、空数组或默认成功。

## 7. 测试策略

新增 `tests/unit/workflow/workflow_test.cpp`，使用 fake session factory 钉住 workflow 层契约。

当前覆盖：

- options 非法配置拒绝；
- 空参数名、重复参数名、非法默认值拒绝；
- 未声明参数、缺少必填参数、非 finite 参数在启动 worker 前拒绝；
- 空 batch 返回空结果且不启动 worker；
- 默认参数补齐；
- 并发完成顺序变化时仍按输入顺序返回；
- 单任务失败封装为 `SimulationResult`，不影响其他任务；
- Ngspice wrdata 可通过 `SimulationResult.read_ac()` facade 读取；
- 失败任务读取结果时返回 `ReadResult<T>` 失败。

最近验证：

```text
cmake --build --preset default
ctest --preset default --output-on-failure
```

结果：默认测试 97/97 通过，外部 Spectre / Ngspice 测试按现有门控 skipped。

## 8. 文档收口

M5.0 已同步：

- 根 `README.md`；
- `include/su/README.md`；
- `src/README.md`；
- `tests/README.md`；
- `bindings/README.md`；
- `00_项目总览/01_当前事实状态.md`；
- `00_项目总览/02_架构总览.md`；
- `00_项目总览/03_开发路线图.md`；
- `20_专题记录/02_用户工作流API设计.md`。

其中：

- 当前事实、测试数字和已实现边界以 `00_项目总览/01_当前事实状态.md` 为准；
- 当前架构关系以 `00_项目总览/02_架构总览.md` 为准；
- 后续施工顺序以 `00_项目总览/03_开发路线图.md` 和根 `TODO` 为准；
- workflow API 细节以 `20_专题记录/02_用户工作流API设计.md` 为准。

## 9. 后续阶段

M5.0 完成后，最自然的后续方向是 Python workflow binding。该方向已作为 M6 第一版完成。

后续进入条件：

- C++ workflow API 经过至少一次真实 Spectre / Ngspice 使用复核；
- Python `Simulation` 生命周期与 C++ `cleanup()` 关系收口；
- Python result 对象是否包装现有 `SimulationResult` 或复用现有读取层对象完成决策；
- Python API 不直接暴露底层 session、factory、pool。

责任文档：

- `06_M6_Python工作流Binding.md`；
- `../20_专题记录/03_Python工作流Binding设计.md`。

完成定义应保持和 M5.0 一致：

```text
Python 用户仍然只面对：
Simulation -> add_parameter -> run -> SimulationResult -> read_*
```
