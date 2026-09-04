# psf_ascii fixtures

本目录存放 Spectre 23.1 `rawfmt=psfascii` 输出的真实小样本，用于验证 SPICEUnion
内置 PSFASCII parser（`src/parse/psf_ascii_backend.cpp`）。

## 样本来源

来源网表：私有材料中的 `BGR_AMP/{dc,stb}`（gpdk045），由真实 Spectre 23.1 仿真
生成，2026-08-22 固化（网表与原始路径不随仓库公开）。

| fixture | 内容 | 用途 |
|---|---|---|
| `bgr_amp_dc_op.raw/dcOp.dc` | dcOp 标量（`V_BGR`、`V7:p`） | `read_dc_value` 标量解析 |
| `bgr_amp_dc_sweep.raw/dc.dc` | temp 从 -40°C 到 120°C 的 17 点 DC sweep | `read_dc_sweep` 扫描解析 |
| `bgr_amp_stb.raw/stb.stb` | 1 Hz ~ 1 GHz STB 复频响（`loopGain`） | `read_ac_response` 复频响解析 |

## 当前约定

- 样本体积小、可重复，不含 PDK 模型正文；新增 ASCII 样本前先在真实仿真中验证。
- 普通 ASCII `tran.tran` 暂缺真实样本，波形路径由测试内的合成 ASCII 数据覆盖。
