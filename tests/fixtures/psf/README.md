# tests/fixtures/psf

本目录存放 Spectre PSF 结果读取测试用 fixture。

这些 fixture 只用于测试 `src/parse` 中的结果读取逻辑，不用于测试 Spectre
interactive session、PDK、license 或 netlist 生成流程。

## 已固化 fixture

| 目录 | 文件 | 来源 | 用途 | 已知期望值 |
|---|---|---|---|---|
| `dc_op_minimal.raw/` | `dcOp.dc` | `henjo/libpsf` 上游 `test/data/dcOp.dc` | 最小 DC scalar 读取 | `vout = 2.5` |
| `spectre_materials_dc_op.raw/` | `dcOp.dc` | `spectre_materials/local/runtime/sim_result/input_C11/input_C11.raw/dcOp.dc` | 当前 Cadence Spectre 23.1 输出的 DC scalar 读取 | `net6 = 0.8` |
| `spectre_materials_stb_loop_gain.raw/` | `stb.stb` | `spectre_materials/local/runtime/sim_result/input_C11/input_C11.raw/stb.stb` | STB / 频域 swept complex vector 后续读取验证 | `freq[0] = 1`，`loopGain` 共 201 点 |
| `tran_time_sweep.raw/` | `tran.tran` | `henjo/libpsf` 上游 `examples/data/timeSweep` | 普通 time-sweep transient waveform 读取 | `INN` 共 323 点，`time[0] = 0`，`INN[0] = 0.6` |
| `spectre_materials_psfxl_tran.raw/` | `tran.tran.tran` + `.psfxl` + `.sig` | `spectre_materials/local/runtime/amp_tran_input_20260804_1435/input.raw` | Spectre 23.1 PSFXL transient 边界样本 | 当前 `henjo/libpsf` backend 明确返回 `kUnsupportedFormat` |

## 暂缺 fixture

以下样本暂缺，M2.3 后续需要补齐：

- 标准 `ac.ac`：用于验证 `read_ac_response()` 的 frequency 与 complex response 读取。
- legacy sensitivity 结果：用于验证 `read_sensitivity_legacy()`。

以下样本已有，但仍需后续支持：

- Spectre 23.1 PSFXL transient：`spectre_materials_psfxl_tran.raw/` 已保留样本，但当前
  `henjo/libpsf` backend 无法解析其 `.psfxl` 数据。

## 命名规则

- `.raw/` 目录使用稳定业务语义命名，例如 `dc_op_minimal.raw/`。
- 不使用 `new`、`tmp`、`test2`、`v2` 这类阶段性名称。
- 若 fixture 来自外部项目，需要记录原始路径和用途。
- 若 fixture 由脚本生成，需要记录生成脚本、关键 netlist 和期望值。
