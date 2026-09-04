# Changelog

本项目遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 与
语义化版本（SemVer）。版本号同步位置：`CMakeLists.txt`、`src/core/version.cpp`、
`pyproject.toml`。

## [Unreleased]

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
