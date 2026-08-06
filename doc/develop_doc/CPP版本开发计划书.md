# SPICEUnion C++ 开发计划书

时间：2026-08-01（初稿）／修订：2026-08-03（补充 Python 功能覆盖契约）

## 声明

本计划书是 `SPICEUnion` 本仓库的主开发计划，用于指导从 M0 到 V1/V2 的实现、测试、验收与面试叙事。

- 行为基准来自 `~/my_lab/projects/spectre_materials/src/spectre_interactive/` 的现有 Python 主线。
- C++ 版 V1 的最低目标不是“另起一个漂亮架构”，而是**完整覆盖当前 Python 版执行层可用功能**，再在此基础上做性能、绑定与多仿真器扩展。
- `SPICEUnion` 是独立项目，不依赖 `spectre_materials` 的 Python 运行环境；`spectre_materials` 只作为行为对照与测试语义来源。
- 本计划书随实现进展迭代；当行为、结构、入口、配置或流程变化时，需同步更新根 `README.md`、根 `TODO` 与相关目录文档。

## 一、背景与现状（要移植什么）

现有 Python 主线共 4 个主功能模块，承担四层职责：

| 模块 | 职责 | C++ 版对应物 |
| --- | --- | --- |
| `spectre_daemon.py` | 常驻 Spectre 进程管理：启动、SKILL 握手、参数下发、运行、结果回收、优雅退出/强杀 | 仿真器会话层 `SimulatorSession` |
| `daemon_pool.py` | 守护进程池：并行预热、队列动态抢占调度、按输入顺序保序回收结果 | 进程池 + 线程池 |
| `task_library.py` | 基于 libpsf 的低层结果读取：`.raw` 目录定位、dcOp/ac/tran 提取、UGBW/相位裕度/建立时间、legacy sensitivity 解析 | 最小 ResultIR + 通用结果读取 helper + 清晰失败语义 |
| `generic_evaluator.py` | 对外门面：把"参数状态 → 并发仿真 → 解析回调"串成一条 API | C ABI / 绑定层之上的 Facade |

必须复刻的关键行为基准：

- 交互协议是 Spectre 的 **SKILL 交互模式**：stdin/stdout 收发 SKILL 表达式；握手标志为 `Entering Skill interactive front end`；参数设置用 `(sclSetAttribute (sclGetParameter top "key") "value" ...)`；运行用 `(sclRun "all")`；完成标志为资源统计行（`Peak resident memory used`）之后的 `t`；退出用 `(sclQuit)`。
- 对上游稳定暴露的调用模型是 `GenericEvaluator(netlist_path, num_workers, work_dir_base, workspace_namespace)` + `run(states, parse_func)`；`parse_func(work_dir)` 由上游提供，返回值类型不由执行层约束。
- 调度结果必须与输入 `states` 保序；单个任务失败时对应结果可为空/错误对象，但不能打乱其他任务顺序。
- 每个 evaluator 必须拥有独立 `workspace_namespace`，并发 evaluator 之间不能发生 worker 工作目录碰撞。
- 结果格式是 **PSF**（`dcOp.dc`、`ac.ac`、`tran.tran`、sensitivity 结果文件），现有解析依赖 libpsf。
- 异常路径是行为基准的一部分：启动失败要向上暴露并清理已启动 worker；运行超时要回收进程；BrokenPipe/OSError 等传输失败允许有限次重启；`nil` 表示失败。
- 现有测试用 FakeDaemon 在 Python 层验证调度正确性（工作目录无碰撞、启动失败传播、结果顺序保序、yield_opt 接入契约），这套测试思想要移植到 C++ 侧。

## 二、必要性：为什么值得用 C++ 重写

**1. 性能是可量化、可演示的卖点**

- 调度路径：Python 版每次任务经过 GIL 下的线程池 + 队列 + 动态类型分发；C++ 版用轻锁队列和直接调度，任务往返延迟有 2–5 倍的现实提升空间。
- 解析路径：Python 主线依赖 libpsf 读取 PSF；C++ 版先用最小 ResultIR 隔离公开 API
  与具体 parser backend，再通过可选 libpsf backend、fixture 和 benchmark 判断是否需要
  自研 native PSF parser。native parser 一旦落地，单文件解析吞吐有提升空间，但必须以实测为准。

**2. 部署形态更接近"底层服务"**

Python 版需要 conda 环境与 libpsf 等依赖；C++ 版可产出静态库 + 单头 C ABI + 单二进制，一条命令安装，天然满足原稿"一键安装 / 一键配置"的诉求。

**3. 多语言支持必须以 C/C++ 为核心**

原稿要求 C++、Python、MATLAB 都能调用。多语言绑定的工业标准形态是 C 核心 + C ABI，各语言做薄封装。C++ 是实现这个形态的前提，而不是可选项。

**4. 面试叙事价值**

"多仿真器抽象 + 可替换结果读取 backend + 底层调度"是一组可以讲深讲透、可量化验证的工程故事，比 Python 胶水层有更强的技术纵深。

**结论：需求成立。** 前提是范围收敛——先做"单仿真器 C++ 核心 + 绑定 + 性能验证"，再做多仿真器拓展。

## 三、目标与范围

### 3.1 目标

把本仓库的"仿真器常驻进程管理 + 并发任务调度 + 结果解析"用 C++ 实现为一套**可嵌入、可绑定、可扩展的多仿真器底层服务**，并按面试项目标准交付：清晰架构、完整测试、可复现基准、良好文档。

V1 的功能目标必须先对齐当前 Python 版：同一批参数状态、同一份 Spectre netlist、同一个上游解析回调语义下，C++ 版能替代 `GenericEvaluator.run(states, parse_func)` 完成批量仿真执行，并保持工作目录隔离、输入保序、失败可定位。

### 3.2 版本范围

- **V1（必做，核心）**：完整覆盖当前 Python 执行层功能 + C++ 核心 + Spectre 单仿真器 + 进程池/线程池 + 最小 ResultIR + 通用结果读取 helper + 可选 libpsf backend / native parser 评估 + C ABI + Python 绑定 + 性能基准。V1 是总目标，实际开发按 3.5 的启动切片推进。
- **V2（按时间弹性扩展）**：多仿真器抽象（网表 IR / 结果 IR）+ Ngspice 全量适配 + Xyce/Hspice 注册式适配 + MATLAB MEX + 一键安装。
- **明确不做**：GUI、Windows 优先支持（Linux 优先，符合 EDA 场景）、分布式调度。

### 3.3 交付形态

- V1 以"库 + C ABI + Python 绑定"交付（嵌入形态）。
- V2 可选提供本地服务形态（Unix socket，承载在现有调度核心之上），用于支撑"底层服务"叙事。

### 3.4 Python 版功能覆盖契约

V1 必须覆盖下面这些当前 Python 版已稳定使用的能力。

**主契约**：

- 初始化 evaluator：`netlist_path`、`num_workers`、`work_dir_base`、`workspace_namespace`。
- 批量运行：输入 `states: List[Dict[str, float]]`，输出与输入等长、同序的结果列表。
- 上游解析回调：执行层把每个 worker 的 `work_dir` 交给回调；业务指标、目标函数和优化策略留在上游。
- 空输入返回空结果，不启动额外任务。

**执行层行为**：

- 每个 worker 使用独立工作目录；每个 evaluator 使用独立命名空间。
- `start_all` 并行预热所有 worker；任一 worker 启动失败时停止已启动 worker 并向上报告错误。
- 任务调度使用空闲 worker 队列，避免静态 round-robin 的队头阻塞。
- 任务完成顺序可以不同，但结果必须按提交顺序返回。
- 单任务仿真失败或解析失败不能影响其他任务；对应结果应返回空值或带状态的失败对象。
- 支持显式 `cleanup()` / `shutdown_all()`，并尽量优雅退出；必要时强杀。
- 支持长跑后的 worker reload，用于进程状态清理。

**低层结果 helper**：

- `.raw` 结果目录定位。
- `dcOp.dc` 单信号读取。
- `ac.ac` 频率、幅度、相位读取。
- UGBW / phase margin 通用计算。
- `tran.tran` 建立时间计算。
- legacy sensitivity 结果读取与结构化整理。
- Python `task_library.py` 只作为历史参考和 fixture 来源；C++ 核心不为强行兼容
  Python 的 `0.0` / 空数组 / `None` 失败返回而牺牲类型安全。

**边界**：

- V1 核心只负责执行层与低层结果读取，不承载项目级 parser 语义。
- 不负责优化器策略、目标函数、评分逻辑、实验编排、PDK 内容管理。
- V1 不做完整网表语法 IR；只保留现有 Python 版所依赖的 SKILL 参数覆盖语义。

### 3.5 启动切片与跨语言边界

为了让项目可以稳定开工，V1 不直接从真实 Spectre 子进程、PSF parser 或 pybind11 全量绑定开始，而是按下面切片推进。

**M0.1：仓库骨架**

- 建立最小 CMake 工程。
- 接入 GoogleTest。
- 建立 `include/`、`src/`、`tests/`、`bindings/`、`bench/`、`scripts/` 等目录及目录 README。
- 本地验证命令至少覆盖 configure、build、test。

**M0.2：执行层核心类型**

先定义不依赖真实仿真器的核心类型：

- `EvaluatorOptions`：`netlist_path`、`num_workers`、`work_dir_base`、`workspace_namespace`、timeout / restart 策略。
- `ParameterState`：一次仿真的参数键值集合，对应 Python `Dict[str, float]`。
- `TaskResult`：单个任务的执行结果，至少包含 `status`、`work_dir`、`error_code`、`error_message`。
- `BatchResult`：与输入 `states` 等长、同序的结果集合。
- `SimulatorSession`：仿真器会话接口。

**M0.3：Fake / Scripted session 契约测试**

在没有真实 Spectre 的情况下先验证：

- evaluator namespace 隔离；
- worker 目录不碰撞；
- `start_all` 并行预热与启动失败传播；
- 空闲 worker 队列调度；
- 任务完成乱序、结果返回保序；
- 单任务失败不影响其他任务；
- cleanup / reload 语义。

**M1.1：真实 Spectre session 最小闭环**

只接入：

- start；
- SKILL handshake；
- stop / kill；
- stdout 录制与错误信息保留。

确认进程生命周期稳定后，再进入参数写入、`sclRun`、完成等待和 transport failure 恢复。

#### C++ 核心 / C ABI / Python binding 边界

Python 版的 `parse_func(work_dir) -> Any` 不能原样下沉到 C ABI。SPICEUnion 的 V1 边界按下面规则处理：

- C++ 核心只保证“执行仿真并交付 worker `work_dir`”，核心 `TaskResult` 不承载上游任意业务对象。
- C ABI 暴露稳定的执行结果结构：状态码、工作目录、错误码、错误文本；内存由 SPICEUnion 分配时，必须提供对应 `su_free_*` 释放函数。
- Python binding 在拿到 `TaskResult.work_dir` 后调用 Python 侧 `parse_func(work_dir)`，再组装 Python 结果列表。因此 Python 层可以保留 `Any` 语义。
- 若仿真失败，C++ 结果层使用明确状态与错误文本表达失败；Python binding 可在语言层选择
  转换为 `None` 或失败对象，但不要求 C++ core 兼容 Python 版“对应位置为空”的失败返回习惯。
- 若 Python `parse_func` 抛异常，只影响对应任务结果，不影响其他任务；异常文本应进入日志或失败对象。
- C++ 原生用户若需要业务解析，应直接读取 `TaskResult.work_dir` 或调用结果 helper，不通过 C ABI 模拟 Python 的任意回调对象。

这意味着 V1 的主契约不是“C ABI 也支持任意 parse callback”，而是：

```text
C++ core / C ABI: states -> ordered TaskResult(work_dir/status/error)
Python binding:  TaskResult.work_dir -> parse_func(work_dir) -> Python Any
```

### 3.6 仓库组织（建议）

```text
SPICEUnion/
├── CMakeLists.txt
├── include/su/              # 公共头文件 + C ABI
├── src/
│   ├── session/             # SimulatorSession 与各仿真器适配器
│   ├── pool/                # 进程池 / 线程池
│   ├── parse/               # PSF / raw 原生解析器
│   └── core/                # 任务队列、网表 IR / 结果 IR
├── bindings/
│   ├── python/              # pybind11
│   └── matlab/              # MEX（V2）
├── tests/                   # GoogleTest
├── bench/                   # Google Benchmark
└── scripts/                 # install.sh 等
```

命名约定：仓库/项目名 `SPICEUnion`，库名 `libspiceunion`，C ABI 前缀 `su_`，Python 包名 `spiceunion`。池库：仓库/项目名 `OrderedConcurrentPool`，库名 `liborderedconcurrentpool`，C ABI 前缀 `ocp_`。

> 池组件（`src/pool/`）M1–M3 内嵌于 SPICEUnion 验证，M3.5 提取为独立仓库 `OrderedConcurrentPool`，SPICEUnion 改为依赖该库，形成"基础设施库 + 领域应用"的上下游关系。提取原则见 4.1。

## 四、总体架构

```text
┌───────────────────────────────────────────────────────┐
│ 绑定层：C ABI │ pybind11 (Python) │ MEX (MATLAB, V2)  │
├───────────────────────────────────────────────────────┤
│ Facade：Evaluator / 批量任务 / parse 回调 / 结果保序 │
├───────────────────────────────────────────────────────┤
│ 调度层：进程池 / 线程池 / 任务队列                     │
├───────────────────────────────────────────────────────┤
│ 协议层：SimulatorSession（各仿真器的交互实现）         │
├───────────────────────────────────────────────────────┤
│ 数据层：网表 IR / 结果 IR / 原生格式解析器 (PSF/raw)  │
└───────────────────────────────────────────────────────┘
```

### 4.1 调度核心：两个池

不做多池堆砌，架构上只保留两个真正必要的池；会话状态、内存/对象复用都不单独成池。

1. **仿真器进程池（SimulatorPool）**：常驻仿真器进程集合，对应现有 `DaemonPool`。池内每个成员是一个**仿真会话**——进程句柄 + 协议状态机（启动/握手/就绪/运行/回收）+ 工作目录。职责：并行预热（映射现有 `start_all`）、空闲会话队列调度、会话崩溃/超时后的回收重建、evaluator 级工作目录命名空间隔离。
2. **线程池（WorkerPool）**：任务执行与结果回收的并发载体，负责"参数状态 → 空闲会话 → 仿真 → parse 回调/结果解析"流水线。与进程池解耦：进程池决定"有多少仿真器可用"，线程池决定"任务如何被并发执行"。

刻意不做的"池"：

- **会话状态并入进程池**：会话不是独立资源，而是池成员的固有状态，单独建池只是重复抽象。
- **内存/对象复用不单独成池**：属于解析器层面的性能优化，M2 按需用预分配缓冲/复用即可，不作为架构组成部分。

设计原则：两池职责单一、可单独开关与基准，为面试讲述提供素材。

**提取设计（M3.5）**：两池的设计目标是"域无关但语义丰富"——不依赖仿真器领域，但保留三个核心语义：worker 生命周期抽象（会话状态机）、输入保序回收、崩溃/超时重建。提取时机放在 M3 之后而非项目初期：API 必须先经 SPICEUnion 的真实使用定型，避免无消费者的投机式泛化。提取后 SPICEUnion 成为该库的第一个真实使用者。

### 4.2 协议层：多仿真器的关键

`SimulatorSession` 是唯一允许接触具体仿真器进程的层：

```cpp
class SimulatorSession {
public:
  virtual bool start();                   // 启动 + 握手
  virtual bool set_parameter(name, value);
  virtual bool run_all();
  virtual bool wait_completion(timeout);  // 解析完成标志
  virtual bool stop(bool graceful);
};
```

- Spectre 适配器：复刻 SKILL 协议（见"背景与现状"）。
- 其余仿真器适配器：实现同一接口，V2 注册式接入。
- 无真实仿真器的环境：用 `ScriptedSession`（播放录制的协议日志）做行为测试——继承现有 FakeDaemon 的测试思想。

### 4.3 数据层：两套 IR

- **网表 IR**：统一的电路描述（元件、参数、分析语句）。V1 沿用现有机制（SKILL 参数覆盖），不做完整网表语法解析；V2 做完整解析，让各仿真器网表语法编译到 IR，再生成目标网表。
- **结果 IR**：统一的信号数据模型（工作点、`freq/mag/phase`、时域波形、sensitivity 条目）。各格式解析器（PSF/raw/tr0）输出同一 IR，上层计算（UGBW、相位裕度、建立时间）只依赖 IR。

## 五、技术栈

| 项 | 选择 | 理由 |
| --- | --- | --- |
| 语言标准 | C++17 | 生态成熟、编译器友好，不追求 C++20 炫技 |
| 构建 | CMake ≥ 3.20 + FetchContent | 评审最容易上手 |
| 进程管理 | POSIX fork/exec + pipe | Linux 优先，实现简洁可讲 |
| 并发 | `std::thread` + 自实现队列 | 线程池/队列亲手实现，是面试亮点 |
| 测试 | GoogleTest | 生态主流 |
| 基准 | Google Benchmark | 数据可复现、图表现成 |
| Python 绑定 | pybind11 | 成熟、文档多 |
| MATLAB 绑定 | MEX 包装 C ABI（V2） | 绑定层只认 C ABI，MEX 是薄壳 |
| 格式解析 | 自写 PSF/raw 解析器 | 最大性能与叙事卖点 |

## 六、里程碑与验收

总体节奏：V1 约 4 周，V2 弹性 3–4 周（含池库提取），总计 7–8 周业余时间（按面试节奏，V1 优先）。

| 里程碑 | 内容 | 验收标准 |
| --- | --- | --- |
| M0 设计定型（1 周） | 本计划书定稿；根 README/TODO 完成；独立仓库初始化；CMake / CI 骨架（编译 + 单测）；Fake / Scripted session 契约测试草案 | 仓库可配置、可编译、hello 单测通过；执行层核心类型与跨语言边界写入文档；M1.1 开发任务清楚 |
| M1 单仿真器核心（2 周） | `SimulatorSession`(Spectre) + 进程池/线程池 + C ABI + pybind11 + Python `GenericEvaluator` 等价门面 | 覆盖 Python 版主契约：并行预热、命名空间隔离、空闲 worker 调度、输入保序、启动失败传播、timeout/transport failure 回收；单 worker 连续 1000 次运行无泄漏 |
| M2 结果层（1 周） | 最小 ResultIR + 通用结果读取 helper + UGBW/PM/建立时间 + `.raw` 目录定位 + 固定 fixture | C++ API 具备清晰失败语义；参考 libpsf / Python 数值但不强行复制 Python 失败返回习惯；benchmark 入口可运行 |
| M3 多仿真器抽象（1–2 周） | 先用 Ngspice 最小 batch adapter 验证第二真实仿真器，再用 RC AC / RC TRAN / 电阻分压 DC sweep 这类真实小任务收敛结果 IR；Xyce/Hspice 不提前占位 | Ngspice AC / TRAN / DC sweep 能映射到 `AcResponse` / `TranWaveform` / `DcSweep`；Spectre PSF 与 Ngspice `wrdata` 能复用同类 AC / TRAN 语义检查；IR 扩展有真实消费者；文档记录保留与拒绝的抽象 |
| M3.5 池库提取（1 周） | 两池组件提取为独立仓库 `OrderedConcurrentPool`：独立测试/基准/README；SPICEUnion 改为依赖该库 | `OrderedConcurrentPool` 可独立构建运行；SPICEUnion 依赖后全部测试通过；API 冻结 |
| M4 服务化与部署（1 周） | 一键安装脚本；单二进制 demo；性能基准报告 | `./install.sh && demo` 一条命令跑通；基准文档含对比图 |
| M5 面试打磨（1 周） | demo 场景、演讲故事线、README | 30 分钟 demo 脚本可完整演示 |

## 七、全局验收与性能基准

1. **功能等价**：V1 用 `spectre_materials` 现有主线测试思想与常用网表做参考，优先使用 `~/my_lab/projects/spectre_materials/netlist/AMP/dc/input.scs` 与 `~/my_lab/projects/spectre_materials/netlist/AMP/sens/input.scs`；执行层契约、worker 工作目录约定、失败隔离、保序语义均通过测试。结果读取层参考 Python / libpsf 数值，但 C++ API 以清晰状态和类型安全为优先。
2. **性能**（M1/M2 实测后回填数字）：
   - 调度往返延迟：C++ / Python ≥ 2x；
   - 解析吞吐：C++ / Python ≥ 10x；
   - 批量场景：100 状态 × 16 worker 的总耗时、CPU/内存峰值对比。
3. **稳定性**：ASan/valgrind 下无泄漏、无崩溃；仿真失败、超时、进程被杀等异常路径均有覆盖测试。
4. **可移植**：绑定层只依赖 C ABI；Python、MATLAB 示例各一个。

## 八、风险与对策

| 风险 | 对策 |
| --- | --- |
| 开发环境无 Spectre | `ScriptedSession` 录制-回放；对照 Python 版测试数据 |
| Hspice/Xyce 无 license | 用公开网表样例 + 记录数据做适配层测试，语法解析用 IR 侧单测 |
| SKILL 协议版本差异 | 握手/完成标志做成可配置字符串，不硬编码 |
| C ABI 无法表达 Python `Any` 回调 | C ABI 只返回 `TaskResult(work_dir/status/error)`；Python binding 在语言层调用 `parse_func(work_dir)` |
| 时间不足 | V1 严格收敛；V2 按 Ngspice > Xyce > Hspice 优先级裁剪 |
| 范围蔓延（例如顺手做 GUI） | 明确不做项写进根 `README.md` |
| 执行层边界向业务层泄漏 | V1 保持 `parse_func(work_dir)` / 结果 IR 边界；业务 parser、优化器、PDK 管理留给上游 |

## 九、面试叙事建议（三个闪光点 + 两项目叙事）

1. **原生 PSF 解析器**：一个格式、一个数据模型、一个数量级的性能差距，最适合现场演示。
2. **多仿真器 IR 抽象**：网表 IR + 结果 IR 让"新增一个仿真器"变成注册一个适配器，体现抽象设计能力。
3. **C ABI 多语言绑定**：同一核心同时服务 C++/Python/MATLAB，体现"底层服务"的产品思维。
4. **两项目上下游叙事**：`OrderedConcurrentPool` 池库是基础设施、`SPICEUnion` 是领域应用；用"提取式开发"串起两者——先有真实消费者再通用化，而不是先造通用组件。

建议准备一张对比图（Python vs C++：往返延迟、解析吞吐、批量吞吐）和一份 30 分钟 demo 脚本，这两件是面试中最有说服力的交付物；另准备 `OrderedConcurrentPool` 独立仓库的一页 README 作为第二项目名片。
