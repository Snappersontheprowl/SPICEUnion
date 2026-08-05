# M3.0 Ngspice 最小接入记录

时间：2026-08-05

本文记录 M3.0 中第二个真实仿真器 Ngspice 的最小接入状态。

后续 M3.1 已在此基础上增加 RC charging TRAN，并记录于
`M3.1-Ngspice瞬态与跨后端AC语义对照.md`。本文只保留 M3.0 的历史事实。

## 当前结论

Ngspice 已作为第二个真实 simulator backend 接入 SPICEUnion。

M3.0 接入范围很窄：

- 只实现 batch adapter，不实现 interactive Ngspice session。
- 只支持内置 RC low-pass AC 示例。
- 只解析 Ngspice `wrdata v(out)` 生成的三列文本输出。
- 只将输出映射到 M2 `AcResponse`。
- 不引入完整 netlist IR。
- 不引入 Ngspice binary raw parser。
- 不声称完整支持 Ngspice。

这一步的目标是验证现有 `SimulatorSession` / evaluator / ResultIR 抽象可以承载第二个
真实仿真器，而不是一次性完成多仿真器框架。

## 代码入口

公开头文件：

```text
include/su/ngspice_session.hpp
```

实现文件：

```text
src/session/ngspice_session.cpp
```

测试文件：

```text
tests/ngspice_session_test.cpp
```

新增 evaluator factory：

```cpp
SessionFactory make_ngspice_session_factory();
Evaluator make_ngspice_evaluator(EvaluatorOptions options);
```

## 当前执行路径

```text
Evaluator
  -> SimulatorPool
  -> NgspiceSession
  -> 生成 rc_ac.cir
  -> ngspice -b rc_ac.cir
  -> wrdata rc_ac.out v(out)
  -> read_ngspice_wrdata_ac_response()
  -> AcResponse
```

`TaskResult` 仍然只承载任务级执行状态、worker directory、错误文本与轻量 detail；
解析后的 `AcResponse` 不塞进 `TaskResult`。

## 参数约定

第一版使用 `ParameterState` 覆盖内置 RC AC 模板参数：

| 参数名 | 默认值 | 含义 |
|---|---:|---|
| `resistance_ohm` | `1000.0` | RC 低通电阻 |
| `capacitance_f` | `1.0e-12` | RC 低通电容 |
| `ac_start_hz` | `1.0e6` | AC 起始频率 |
| `ac_stop_hz` | `1.0e10` | AC 终止频率 |
| `points_per_decade` | `20` | 每 decade 点数 |

当前内置 netlist 形态：

```spice
Vin in 0 dc 0 ac 1
R1 in out <resistance_ohm>
C1 out 0 <capacitance_f>
.control
set filetype=ascii
ac dec <points_per_decade> <ac_start_hz> <ac_stop_hz>
wrdata rc_ac.out v(out)
quit
.endc
.end
```

## Ngspice 输出格式

本机 Ngspice revision 27 下，`wrdata rc_ac.out v(out)` 输出三列：

```text
frequency real(v(out)) imag(v(out))
```

示例：

```text
1.00000000e+06  9.99960523e-01 -6.28293727e-03
1.12201845e+06  9.99950302e-01 -7.04949950e-03
```

M3.0 parser：

```cpp
ReadResult<AcResponse> read_ngspice_wrdata_ac_response(data_path, signal_name);
```

映射关系：

```text
column 1 -> AcResponse.frequency_hz
column 2 -> AcResponse.real
column 3 -> AcResponse.imag
```

## 外部依赖

当前机器已确认存在：

```text
/usr/local/bin/ngspice
```

版本输出：

```text
ngspice compiled from ngspice revision 27
```

`NgspiceSession::start()` 查找顺序：

1. `SPICEUNION_NGSPICE` 环境变量指定的可执行文件；
2. `PATH` 中的 `ngspice_con`；
3. `PATH` 中的 `ngspice`。

默认测试不要求 Ngspice 存在。真实 Ngspice 运行测试只在
`SPICEUNION_ENABLE_EXTERNAL_TESTS=ON` 时执行。

## 测试状态

默认测试覆盖：

- `ParameterState` 到 RC AC config 的映射。
- RC AC netlist 渲染。
- 三列 `wrdata` AC 输出解析。
- malformed `wrdata` 输出的 parse error。
- 外部 Ngspice 测试在默认配置下 skip。

外部测试覆盖：

- `NgspiceSession` 真实调用 `ngspice -b`。
- `make_ngspice_evaluator()` 通过 evaluator / pool 跑 2 worker batch。
- 读取 `rc_ac.out` 并映射到 `AcResponse`。
- 使用 M2 AC helper 验证 RC 低通 -3 dB 频率接近理论值：

```text
fc = 1 / (2πRC)
```

## 已验证命令

默认测试：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

M3.0 当时结果：

```text
100% tests passed, 0 tests failed out of 47
```

外部测试：

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external --output-on-failure
```

M3.0 当时结果：

```text
100% tests passed, 0 tests failed out of 47
```

## 当前边界

M3.0 不做：

- 完整 netlist IR。
- Ngspice binary raw parser。
- Ngspice `.op` / `.dc` / `.tran` 通用读取。
- MOS / PDK 模型接入。
- Xyce / Hspice 占位。
- C ABI / Python binding。

M3.0 的事实结论是：

```text
SPICEUnion 已经不是 Spectre-only 执行框架；
现有 SimulatorSession / evaluator / ResultIR 可以承载第二个真实仿真器。
```

## 后续候选方向

后续若继续推进 M3，应优先从真实消费者出发，而不是提前扩展全量抽象：

- 增加 Ngspice `.dc` 简单 sweep 示例，评估是否需要新的 ResultIR。
- 比较 Spectre 与 Ngspice 在同类 AC/TRAN 输出上的 IR 复用情况。
- 只有两个真实 backend 共同需要时，再扩展 `EvaluatorOptions` 或引入 netlist 模板接口。
