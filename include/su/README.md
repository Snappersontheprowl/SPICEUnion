# include/su

本目录存放 SPICEUnion 的公开 C++ 头文件。

命名空间为 `su`。

## 模块概览

### `core.hpp`

执行层共享核心数据类型。

该头文件当前定义：

- `ParameterState`：单个仿真状态的参数映射，当前建模为
  `std::map<std::string, double>`。
- `EvaluatorOptions`：evaluator/session 通用配置，包括 `netlist_path`、
  `num_workers`、`work_dir_base`、`workspace_namespace`、`timeout_seconds`
  与 `restart_attempts`。

当代码需要描述仿真器输入参数，或向 evaluator/session 对象传递配置时，应使用该头文件。

该模块必须保持仿真器无关，不应包含 Spectre 专用 protocol string、PSF parsing 类型或
业务 metric 定义。

### `task_result.hpp`

任务级状态与结果报告。

该头文件当前定义：

- `TaskStatus`：单个提交状态的标准化执行状态。
- `TaskResult`：有序 batch 输出项，包含 `status`、`work_dir`、`error_code`、
  `error_message` 以及可选 `detail`。
- `to_string(TaskStatus)`：用于生成稳定状态文本的小工具函数。

`TaskResult` 对应 Python 执行层中的核心行为：成功时把 worker directory 交给调用方，
失败时把失败限制在对应任务槽位内。

该模块只描述执行结果，不应承载已解析 PSF 值、circuit metrics、objective scores 或
optimizer decisions。

### `result.hpp`

仿真结果层的最小 ResultIR 与读取状态。

该头文件当前定义：

- `ResultStatus`：结果读取层的标准化状态，包括目录缺失、文件缺失、signal 缺失、
  格式不支持、解析失败与非法输入。
- `ResultError`：轻量错误描述。
- `ReadResult<T>`：带显式状态的读取结果容器，用于区分真实 `0.0` 与读取失败。
- `ResultDirectory`：`.raw` 结果目录定位结果。
- `ScalarResult`：单 signal 标量结果。
- `DcSweep`：单 sweep axis、单 signal 的实数 DC sweep 响应。
- `AcResponse`：AC 原始复数响应，保留 frequency、real、imag。
- `AcDerivedView`：AC 派生视图，包含 magnitude_db 与 phase_deg。
- `UgbwPhaseMarginResult`：UGBW / phase margin 通用数学结果。
- `TranWaveform`：tran time/value 波形。
- `SettlingTimeResult`：settling time 通用数学结果。
- `SensitivityEntry`：legacy sensitivity 原始条目结构。

该模块只表达仿真产物本身，不表达项目业务指标、objective、penalty 或 pass/fail。

### `result_reader.hpp`

结果读取与通用数学 helper 的公开 API 骨架。

该头文件当前声明：

- `find_result_directory(work_dir)`：定位 worker 目录下的 `.raw` 结果目录。
- `read_dc_value(result_dir, signal_name)`：读取 `dcOp.dc` 单 signal 标量。
- `read_ac_response(result_dir, signal_name, filename)`：读取 AC frequency 与复数响应。
- `read_tran_waveform(result_dir, signal_name, filename)`：读取 tran time/value 波形。
- `read_sensitivity_legacy(work_dir)`：读取 legacy sensitivity 条目。
- `derive_ac_view(response)`：从 AC 复数响应派生 magnitude / phase。
- `calculate_ugbw_and_phase_margin(response)`：计算 UGBW / phase margin。
- `calculate_settling_time(waveform, target_value, error_band)`：计算 settling time。

M2.2 已实现 `.raw` 目录定位、AC 派生视图、UGBW / phase margin 和 settling time。
dc/ac/tran/sens 文件读取在 M2.3 前返回 `kUnsupportedFormat`。该头文件不得暴露 libpsf 类型。

### `session.hpp`

仿真器 session 抽象。

该头文件定义：

- `SimulatorSession`：由具体仿真器 backend 实现的抽象生命周期接口。
- `SimulatorSessionPtr`：evaluator/pool factory 使用的 owning pointer 类型。

该接口刻意保持很小：

- `start()`：准备并启动一个 simulator worker。
- `run(state, timeout)`：运行一个参数状态并返回 `TaskResult`。
- `stop(graceful)`：释放 worker process/session。
- `worker_id()` / `work_dir()`：暴露稳定的 worker identity 与输出目录。

实现新的 simulator backend，或构造 fake/scripted session 这类 test double 时，应使用该头文件。

该模块保持仿真器无关。具体 process protocol 细节应放在 backend 专用头文件中，例如
`spectre_session.hpp` 或 `ngspice_session.hpp`。

### `evaluator.hpp`

批量执行门面。

该头文件定义：

- `SessionFactory`：为每个 worker 创建一个 session 的 factory callback。
- `Evaluator`：高层 batch runner，拥有 simulator pool，并在 C++ core 层保留 Python
  `GenericEvaluator.run(states, parse_func)` 的执行语义。
- `make_spectre_session_factory()`：Spectre session 默认 factory。
- `make_spectre_evaluator(options)`：标准 Spectre-backed evaluator 的便捷构造函数。
- `make_ngspice_session_factory()`：Ngspice session 默认 factory，默认运行内置 RC AC。
- `make_ngspice_session_factory(task)`：按 `NgspiceBuiltinTask` 创建 Ngspice session
  factory。
- `make_ngspice_evaluator(options)`：标准 Ngspice-backed evaluator 的便捷构造函数，默认
  运行内置 RC AC。
- `make_ngspice_evaluator(options, task)`：按 `NgspiceBuiltinTask` 创建 Ngspice-backed
  evaluator。
- `generate_workspace_namespace()` 与 `join_path()`：evaluator/pool setup 使用的小工具函数。

`Evaluator::run(states)` 返回与输入 states 等长且同序的结果向量。内部任务完成顺序可以不同，
但结果放置顺序必须按提交 index 排列。

C++ evaluator 返回 `TaskResult` 对象，而不是任意业务值。未来 Python binding 这类语言绑定层
可以在收到成功的 `TaskResult` 后，再调用语言层 `parse_func(work_dir)`。

### `spectre_protocol.hpp`

Spectre interactive SKILL protocol 的格式化与完成状态分类 helper，特点是小而可测试。

该头文件定义：

- `SpectreCompletion`：对 Spectre stdout 行进行增量完成状态分类。
- `format_spectre_run_command(state)`：把 `ParameterState` 转换为设置参数并执行
  `(sclRun "all")` 的 SKILL 命令。
- `classify_spectre_completion_line(line, seen_resource_stats)`：识别 success/failure
  marker，例如 resource statistics 后的 `t`、`nil`，或经典
  `spectre completes ... 0 errors` 行。

凡是应该在不启动真实 Spectre 进程时独立单测的 protocol logic，都应放在该模块。

该模块不应拥有 process handle、pipe、worker directory 或 retry policy；这些职责属于
`spectre_session.hpp` / `SpectreSession`。

### `spectre_session.hpp`

具体的 Spectre interactive session backend。

该头文件定义：

- `SpectreSession`：基于 `spectre +interactive` 的 `SimulatorSession` 实现。

当前职责包括：

- 准备 worker directory；
- 使用 `+interactive -64 -o <work_dir>` 启动 Spectre；
- 等待 SKILL interactive handshake；
- 使用 `(setq top (sclGetCircuit ""))` 初始化 circuit handle；
- 使用 `spectre_protocol.hpp` 生成的命令派发单个参数状态；
- 等待 completion、timeout、`nil`、EOF 或 transport failure；
- 使用 `(sclQuit)` 优雅停止，必要时 force-kill；
- 保留最近 stdout 行用于错误诊断。

这是唯一应该暴露具体 Spectre session class 的公开 C++ 头文件。通用 scheduling code 应依赖
`SimulatorSession`，而不是直接依赖该类。

### `ngspice_session.hpp`

具体的 Ngspice batch session backend。

该头文件当前定义：

- `NgspiceBuiltinTask`：Ngspice 内置任务选择枚举，当前支持 `kRcAc`、`kRcTran`
  与 `kResistorDividerDc`。
- `NgspiceRcAcConfig`：内置 RC low-pass AC 示例配置。
- `NgspiceRcTranConfig`：内置 RC charging TRAN 示例配置。
- `NgspiceResistorDividerDcConfig`：内置电阻分压 DC sweep 示例配置。
- `ngspice_rc_ac_config_from_state(state)`：从 `ParameterState` 读取
  `resistance_ohm`、`capacitance_f`、`ac_start_hz`、`ac_stop_hz` 与
  `points_per_decade`。
- `ngspice_rc_tran_config_from_state(state)`：从 `ParameterState` 读取
  `resistance_ohm`、`capacitance_f`、`input_voltage_v`、`tran_step_s` 与
  `tran_stop_s`。
- `ngspice_resistor_divider_dc_config_from_state(state)`：从 `ParameterState`
  读取 `top_resistance_ohm`、`bottom_resistance_ohm`、`dc_start_v`、`dc_stop_v`
  与 `dc_step_v`。
- `render_ngspice_rc_ac_netlist(config, output_filename)`：生成内置 RC AC netlist。
- `render_ngspice_rc_tran_netlist(config, output_filename)`：生成内置 RC charging TRAN
  netlist。
- `render_ngspice_resistor_divider_dc_netlist(config, output_filename)`：生成内置
  电阻分压 `.dc Vin start stop step` netlist。
- `read_ngspice_wrdata_ac_response(data_path, signal_name)`：读取 Ngspice
  `wrdata v(out)` 三列文本，映射为 `AcResponse`。
- `read_ngspice_wrdata_tran_waveform(data_path, signal_name)`：读取 Ngspice
  `wrdata v(out)` 两列 transient 文本，映射为 `TranWaveform`。
- `read_ngspice_wrdata_dc_sweep(data_path, sweep_name, signal_name)`：读取 Ngspice
  `wrdata v(out)` 两列 DC sweep 文本，映射为 `DcSweep`。
- `NgspiceSession`：基于 `ngspice -b` 的 `SimulatorSession` 实现。

当前职责包括：

- 准备 worker directory；
- 查找 `SPICEUNION_NGSPICE`、`ngspice_con` 或 `ngspice`；
- 按 `NgspiceBuiltinTask` 为每次 run 生成 `rc_ac.cir`、`rc_tran.cir` 或
  `resistor_divider_dc.cir`；
- 调用 `ngspice -b`；
- 生成 `rc_ac.out` / `rc_tran.out` / `resistor_divider_dc.out` 与 `ngspice.log`；
- 将三列 AC 输出验证为 M2 `AcResponse`；
- 将两列 TRAN 输出验证为 M2 `TranWaveform`；
- 将两列 DC sweep 输出验证为 `DcSweep`；
- 按 timeout、启动失败、仿真失败和 parse failure 返回 `TaskResult`。

该模块是 M3 的第二仿真器 backend，当前只支持三个内置教学级 batch 任务，不表示完整
Ngspice 支持。不要在这里提前引入完整 netlist IR、binary raw parser 或业务指标。

### `version.hpp`

小型版本 helper。

该头文件定义：

- `version()`：返回当前 SPICEUnion library version string。

它主要用于 smoke test 与轻量集成检查。

## 分层规则

当前公开头文件分层如下：

```text
core.hpp
task_result.hpp
result.hpp
  -> result_reader.hpp

session.hpp
  -> ngspice_session.hpp
  -> spectre_session.hpp
  -> evaluator.hpp

spectre_protocol.hpp
  -> spectre_session.hpp
```

约定：

- 共享的仿真器无关类型放在 `core.hpp` 或 `task_result.hpp`。
- 结果层通用类型放在 `result.hpp`。
- 结果读取 API 放在 `result_reader.hpp`。
- 抽象 simulator lifecycle 放在 `session.hpp`。
- batch execution facade 放在 `evaluator.hpp`。
- Ngspice batch backend 放在 `ngspice_session.hpp`。
- Spectre 专用 protocol helper 放在 `spectre_protocol.hpp`。
- Spectre process/session 实现放在 `spectre_session.hpp`。
- 不要把 parsed waveform 或 metric type 塞进 `TaskResult`。
- 不要在公开头文件中暴露 `PSFDataSet`、`PSFVector`、`PSFScalar` 或 `psf.h`。

## 命名规则

- 使用稳定领域名，例如 `evaluator.hpp`、`session.hpp` 与 `task_result.hpp`。
- 结果层使用稳定职责名，例如 `result.hpp` 与 `result_reader.hpp`。
- 不使用 `new`、`tmp`、`final`、`test2`、`v2` 这类阶段性名称。
