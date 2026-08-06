# src/pool

本目录存放 SPICEUnion simulator pool adapter。

## 文件

- `simulator_pool.hpp` / `simulator_pool.cpp`：SPICEUnion adapter，负责把
  `EvaluatorOptions`、`SessionFactory`、`ParameterState`、`TaskResult` 和
  `SimulatorSession` 映射到外部 `OrderedConcurrentPool`。

`OrderedConcurrentPool` 源码位于：

```text
~/my_lab/projects/OrderedConcurrentPool/include/ocp/ordered_concurrent_pool.hpp
```

## 边界

外部 `OrderedConcurrentPool` 负责：

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
