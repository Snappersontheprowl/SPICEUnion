# Changelog

本项目遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 与
语义化版本（SemVer）。版本号同步位置：`CMakeLists.txt`、`src/core/version.cpp`、
`pyproject.toml`。

## [Unreleased]

（暂无）

## [0.2.0] - 2026-10-05

### Changed

- **行为变化**：`TaskResult.work_dir` 现在指向每 job 独立目录
  `worker_<id>/case_<输入下标>`（原为 `worker_<id>`）。`read_*` 行为不变，原始
  产物仍保留在该目录中；
- 同步 OrderedConcurrentPool 的 failure handler 契约变更：worker id 参数改为
  `std::optional<std::size_t>`。`std::nullopt` 表示该 job 从未分配到 worker，
  此时 `SimulatorPool` 的失败结果回落到 workspace 根目录，不再越界取
  `worker_work_dirs_`；`tests/unit/pool/ordered_concurrent_pool_test.cpp`
  同步上游契约测试（含 shutdown 并发与执行单元创建失败两项）。

### Added

- 指标提取主路径：`Simulation(..., metrics=[...])` / `SimulationResult.metrics()` /
  `metric()` / `metrics_ok()` / `metrics_message()`；C++ 侧为
  `include/su/metrics.hpp` 与 `SimulationOptions.metrics`。提取在 worker 归还池子前
  完成，覆盖 DC 标量、DC 扫描、AC（UGBW / phase margin）、TRAN（settling time）；
  产物保留策略不变（原始产物仍保留，`read_*` 继续可用）。

### Fixed

- 同一 worker 连续 job 复用产物目录导致前序结果被覆盖：现在每个 job 使用独立目录
  `worker_<id>/case_<输入下标>`；Ngspice 每次 run 直接写入该目录，Spectre 常驻
  会话在 job 结束后把产物快照到该目录。契约测试见
  `tests/unit/pool/job_work_dir_contract_test.cpp`。

## [0.1.0] - 2026-09-05

### Added

- Python 一键安装链路（P-a/P-b）：OrderedConcurrentPool FetchContent 回退、
  `pyproject.toml`（scikit-build-core）、`pip install .` 可构建 wheel；
  `spiceunion.doctor()` 与 `spiceunion doctor` CLI；云 CI `wheel-smoke` job。
- 仿真器自动适配与诊断 MVP：`su::find_simulator` / `SimulatorHandle`、
  `SPICEUNION_SPECTRE` env、`tests/manual/spiceunion_doctor.cpp`。
- C++ 用户工作流 facade（`Simulation` / `SimulationResult`）与 Python workflow
  binding。
- 测试目录分层（unit / integration / external / fixtures / support / manual）。
- 根 README 中英双语、CONTRIBUTING、Apache-2.0 LICENSE。

### Changed

- 归档目录编号 `90_归档备注` → `30_归档备注`，并写明编号约定。
- CMake / CI 材料命名收敛为 `SPECTRE_MATERIALS_DIR`（私有材料语义，不指向
  未公开项目）。
