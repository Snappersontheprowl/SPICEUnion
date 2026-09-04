# tests/external/spectre 说明

## 功能

本目录存放依赖 Cadence Spectre、Spectre license、私有基线网表与 PDK 的真实工具测试。

## 本级模块职责

- `spectre_session_lifecycle_test.cpp`：外部 Spectre 生命周期契约（interactive
  handshake、单任务 `(sclRun "all")`、multi-worker batch）。
- `spectre_capability_test.cpp`：多网表能力矩阵——真实跑不同电路/分析，按矩阵
  断言解析状态（支持路径断言结构，已知边界断言当前状态）。
- `spectre_real_result_test.cpp`：真实网表端到端——用真实 spectre 跑
  `AMP/dc/input.scs`，再用 libpsf 解析真实 dcOp.dc，断言 Vos / Idd 与
  外部材料中的 `amp_dc` 校准 profile 一致（校准数据私有，不随仓库公开）。
  需要 `SPICEUNION_ENABLE_LIBPSF_READER`。
- `spectre_external_env.hpp`：外部环境检查与材料路径共享 helper。

## 当前约定

- 基线网表与 PDK 统一来自 `SPICEUNION_SPECTRE_MATERIALS_DIR`（默认
  `<repo 上一级>/spectre_materials`，可用 `-D` 显式指定）的 `external/netlist` 与
  `external/pdk`，路径由 CMake 编译宏注入，不在本目录硬编码机器路径。
- 材料目录 `external/netlist` 下的网表均为项目所有者实测的合法仿真网表；这些材料
  私有、不随本仓库分发。当前外部测试仅消费 `AMP/dc/input.scs`，其余电路网表可
  作为后续契约测试材料扩展。
- 基线网表内部以绝对路径 include PDK，跨机器需要等价材料布局。
- 本机 `/dev/shm/pdk_cache` 已废弃，SPICEUnion 不再检查该路径。
- 真实仿真产物写入 `<项目根>/local/runtime/spectre_<场景>/`，不使用内存盘
  （实测 tmpfs 对当前产物规模加速不可测量）。
- 真实网表端到端测试需要 external 与 libpsf 同时开启，使用 `external-libpsf` 预设；
  仅开启 libpsf 时该用例会在测试体开头 `GTEST_SKIP()`。

## 能力矩阵（实测状态）

| 网表 | 分析 | 输出格式 | 解析 API | 当前状态 |
|---|---|---|---|---|
| `AMP/dc` | dcOp | BINPSF | `read_dc_value` | 支持（Vos/Idd 有校准断言） |
| `AMP/ac` | AC | BINPSF | `read_ac_response` | 支持（`net1`，51 频点） |
| `AMP/tran` | TRAN | PSFXL | `read_tran_waveform` | 边界：仿真可跑，解析 `unsupported_format` |
| `BGR_AMP/dc` | dcOp + temp sweep | PSFASCII | `read_dc_value` / `read_dc_sweep` | 支持（内置 PSFASCII parser） |
| `BGR_AMP/stb` | STB | PSFASCII | `read_ac_response` | 支持（内置 PSFASCII parser） |

说明：PSFASCII 由内置 parser 解析（`src/parse/psf_ascii_backend.cpp`），BINPSF
由可选 libpsf 解析，运行时按文件格式自动分发；`SPICEUNION_ENABLE_LIBPSF_READER`
只控制 BINPSF 能力。执行层通过 `TaskResult.result_format` 交付实际格式
（`ResultFormat`），解析层声明优先、`kUnknown` 才嗅探；矩阵用例同时断言
`result_format`（AMP/tran 由运行期产物特征确认为 `kPsfxl`）。

## 常用入口

- 外部测试：

```bash
cmake --preset external && cmake --build --preset external && ctest --preset external --output-on-failure
```

- 真实网表端到端（仿真 + 解析）：

```bash
cmake --preset external-libpsf && cmake --build --preset external-libpsf && ctest --preset external-libpsf --output-on-failure
```
