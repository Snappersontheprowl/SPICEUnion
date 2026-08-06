# src/pool

本目录存放有序 worker 调度核心与 SPICEUnion simulator pool adapter。

## 文件

- `ordered_concurrent_pool.hpp`：领域无关的 C++17 有序并发池核心，位于 `ocp`
  命名空间，只依赖 C++ 标准库。
- `simulator_pool.hpp` / `simulator_pool.cpp`：SPICEUnion adapter，负责把
  `EvaluatorOptions`、`SessionFactory`、`ParameterState`、`TaskResult` 和
  `SimulatorSession` 映射到 `OrderedConcurrentPool`。

## 边界

`OrderedConcurrentPool` 负责：

- 固定数量 worker 生命周期；
- idle-worker dispatch；
- batch result ordering；
- per-job exception 到调用方失败结果的转换；
- startup failure cleanup；
- repeatable shutdown。

`SimulatorPool` 负责：

- worker work directory 命名；
- simulator session 创建；
- timeout 传递；
- `TaskResult` 失败状态映射。

pool 层不应知道 Spectre protocol、Ngspice netlist、PSF/raw 结果读取或电路指标。
