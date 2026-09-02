# workflow

本目录实现普通用户工作流 facade。

## 本级模块职责

- `simulation.cpp`：实现 `Simulation` 与 `SimulationResult`，内部复用 `Evaluator`、
  `TaskResult` 和 `result_reader`。

## 当前约定

- 本目录不实现 optimizer、objective、penalty 或业务 metric。
- 本目录不实现 netlist DSL、PDK 管理或 GUI。
- 本目录不直接暴露 simulator process/protocol、worker pool 或 parser backend 细节。
- `work_dir` 和 `result_format` 只作为高级诊断信息保留，不作为普通结果读取主路径。
