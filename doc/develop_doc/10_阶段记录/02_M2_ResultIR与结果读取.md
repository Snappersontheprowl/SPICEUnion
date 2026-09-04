# M2 ResultIR 与结果读取开发文档

更新时间：2026-08-13

本文负责 SPICEUnion M2 阶段的现状总结、责任边界和收口事实。

当前结论：

```text
M2 已完成；
ResultIR、ReadResult 失败语义、result_reader API、可选 libpsf backend 与 PSF fixture 测试已建立；
后续 M3 在同一结果层上补齐了 DcSweep 与 Spectre / Ngspice DC sweep 对照；
legacy sensitivity、Spectre PSFXL transient、原生 PSF parser 继续暂缓。
```

## 1. M2 目标

M2 的目标是把 SPICEUnion 从“只执行仿真并返回 work directory”的执行层，推进到“能以稳定 C++ 类型读取常见仿真结果”的结果层。

M2 关注：

- 最小 ResultIR；
- 显式读取状态；
- 结果读取 helper；
- PSF fixture 驱动测试；
- 可选 libpsf backend；
- AC / TRAN 常见数学 helper；
- 读取失败不能和合法数值混淆。

M2 不关注：

- 业务 metric / objective / penalty；
- optimizer；
- 完整 netlist IR；
- 多仿真器 backend 对照；
- MOS I-V / gm/Id lookup table；
- Python binding；
- C ABI；
- 原生 PSF parser 产品化。

## 2. M2 范围边界

### 2.1 范围内

- 定义结果层状态 `ResultStatus`；
- 定义 `ReadResult<T>`，区分成功值、合法 `0.0` 与读取失败；
- 定义最小 ResultIR 类型；
- 提供 `result_reader.hpp` 作为公开读取 helper 入口；
- 默认构建不依赖 libpsf；
- 启用 `SPICEUNION_ENABLE_LIBPSF_READER=ON` 后读取已验证普通 PSF fixture；
- 用小型 fixture 覆盖已支持格式；
- 对 unsupported 格式给出明确状态。

### 2.2 范围外

- 不把 circuit metric 写进 ResultIR；
- 不把 pass/fail、score、penalty 写进 ResultIR；
- 不把 optimizer 或设计方法写进结果层；
- 不暴露 libpsf 类型；
- 不承诺所有 Spectre PSF / PSFXL 格式；
- 不在 M2 实现 native PSF parser；
- 不在 M2 处理多维 sweep、family curve 或 gm/Id table。

## 3. 当前已完成事实

### 3.1 读取状态与失败语义

`ResultStatus` 当前包括：

| 状态 | 含义 |
|---|---|
| `kOk` | 读取成功 |
| `kDirectoryNotFound` | 结果目录不存在 |
| `kFileNotFound` | 目标结果文件不存在 |
| `kSignalNotFound` | 目标 signal 不存在 |
| `kUnsupportedFormat` | 当前 backend 不支持该格式 |
| `kParseError` | 文件存在但解析失败 |
| `kInvalidInput` | 调用参数非法 |

`ReadResult<T>` 当前事实：

- 成功时 `status == kOk`；
- 失败时 `status != kOk`；
- 失败时保留 `error_message`；
- `ok()` 与 `operator bool()` 都表示是否成功；
- 合法 `0.0` 不再被误当作失败；
- 读取失败不通过伪造 `0.0`、空数组或 `None` 表达。

### 3.2 ResultIR

当前公开 ResultIR 位于 `include/su/result.hpp`。

| 类型 | 含义 | 状态 |
|---|---|---|
| `ScalarResult` | 单 signal 标量 | 已用于 DC operating point |
| `DcSweep` | 单 sweep axis、单 signal 实数响应 | 已用于 Spectre / Ngspice DC sweep |
| `AcResponse` | frequency + complex response | 已用于 AC / STB 类 swept complex response |
| `AcDerivedView` | magnitude dB + phase deg 派生视图 | 已用于 AC 后处理 |
| `TranWaveform` | time/value 波形 | 已用于普通 transient |
| `UgbwPhaseMarginResult` | UGBW / phase margin 计算结果 | 已实现数学 helper |
| `SettlingTimeResult` | settling time 计算结果 | 已实现数学 helper |
| `SensitivityEntry` | legacy sensitivity 原始条目结构 | 类型保留，读取暂缓 |

ResultIR 原则：

- 只表达仿真结果本身；
- 不表达业务指标；
- 不表达优化目标；
- 不表达电路设计策略；
- 不暴露第三方 parser 类型；
- 不把上层应用的 gm/Id table 提前写进 core。

### 3.3 `result_reader.hpp` 公开 API

当前公开读取与数学 helper：

| API | 当前状态 |
|---|---|
| `find_result_directory(work_dir)` | 已实现 `.raw` 目录定位 |
| `read_dc_value(result_dir, signal_name)` | 默认构建返回 unsupported；libpsf 构建读取 `dcOp.dc` scalar |
| `read_dc_sweep(result_dir, sweep_name, signal_name, filename)` | 默认构建返回 unsupported；libpsf 构建读取单 axis / 单 signal DC sweep |
| `read_ac_response(result_dir, signal_name, filename)` | 默认构建返回 unsupported；libpsf 构建读取 swept complex response |
| `read_tran_waveform(result_dir, signal_name, filename)` | 默认构建返回 unsupported；libpsf 构建读取普通 time-sweep transient |
| `read_sensitivity_legacy(work_dir)` | 当前保持 unsupported stub |
| `derive_ac_view(response)` | 已实现 magnitude dB / phase deg |
| `calculate_ugbw_and_phase_margin(response)` | 已实现 UGBW / phase margin |
| `calculate_settling_time(waveform, target_value, error_band)` | 已实现 settling time |

当前约定：

- 默认构建不链接 libpsf；
- 真实 PSF 文件读取只有在启用 `SPICEUNION_ENABLE_LIBPSF_READER=ON` 后生效；
- 公开 API 不暴露 libpsf 类型；
- unsupported 是明确结果，不是 crash 或静默空值。

## 4. libpsf 评估与接入结论

M2 采用 `henjo/libpsf` 作为可选内部 backend，而不是在 SPICEUnion 中直接新造原生 PSF parser。

当前结论：

| 事项 | 决定 |
|---|---|
| libpsf 是否默认启用 | 否 |
| libpsf 是否暴露给公开 API | 否 |
| libpsf 是否进入核心类型 | 否 |
| libpsf 是否覆盖所有 PSF / PSFXL | 否 |
| native PSF parser 是否启动 | 暂缓 |

这样处理的原因：

- M2 目标是建立结果层契约，不是维护完整 PSF 格式实现；
- libpsf 已能覆盖当前普通 PSF fixture；
- 可选 backend 能保护默认构建和普通开发体验；
- 如果后续出现 libpsf 无法覆盖但必须支持的格式，再评估 native parser 或转换路径。

本机路径事实：

```text
local/external/libpsf/install
local/external/libpsf/install-pic
```

说明：

- `install` 用于普通 libpsf backend 验证；
- `install-pic` 用于把静态 libpsf 链接进 Python shared module；
- 两者都是本机构建产物，不进入版本库。

## 5. fixture 与覆盖范围

PSF fixture 位于：

```text
tests/fixtures/psf/
```

当前 fixture：

| fixture | 用途 |
|---|---|
| `dc_op_minimal.raw/dcOp.dc` | 最小 DC scalar 读取 |
| `spectre_materials_dc_op.raw/dcOp.dc` | 真实 Spectre 23.1 历史 DC scalar（私有材料） |
| `amp_ac_response.raw/ac.ac` | 真实 Spectre 23.1 历史 AC response（私有材料） |
| `spectre_rc_lowpass_ac.raw/ac.ac` | Spectre RC AC 语义测试 |
| `spectre_resistor_divider_dc.raw/dc.dc` | Spectre resistor-divider DC sweep 语义测试 |
| `spectre_materials_stb_loop_gain.raw/stb.stb` | STB swept complex response |
| `tran_time_sweep.raw/tran.tran` | 普通 transient waveform |
| `spectre_rc_charging_tran.raw/tran.tran.tran` | Spectre RC TRAN 语义测试 |
| `spectre_materials_psfxl_tran.raw/tran.tran.tran` | PSFXL unsupported 边界（私有材料） |

M2 结果层当前覆盖：

- DC scalar；
- 单 axis / 单 signal DC sweep；
- swept complex AC / STB response；
- 普通 transient waveform；
- AC derived view；
- UGBW / phase margin；
- settling time；
- unsupported 格式状态。

当前不覆盖：

- legacy sensitivity 真实读取；
- Spectre 23.1 PSFXL transient 解析；
- 多维 sweep；
- family curve；
- MOS gm/Id table；
- 完整 PSF / PSFXL 格式族。

## 6. M2 与后续阶段的关系

M2 建立结果层基础；M3 和 M4 都复用了该结果层。

| 阶段 | 与 M2 的关系 |
|---|---|
| M3 | 在 M2 ResultIR 上接入 Ngspice，并补齐 Spectre / Ngspice AC、TRAN、DC sweep 对照 |
| M4 | 将 M2 / M3 的结果读取 helper 暴露给 Python binding |

后续演进事实：

- `DcSweep` 已在 M3 中用于 Spectre / Ngspice resistor-divider DC sweep 对照；
- Ngspice `wrdata` 文本读取不属于 libpsf backend，但复用 M2 ResultIR；
- Python binding 不新增结果语义，只包装现有 ResultIR 与 helper。

## 7. M2 完成定义

M2 当前满足：

- `ResultStatus` 与 `ReadResult<T>` 已建立；
- 最小 ResultIR 已建立；
- `result_reader.hpp` 公开读取 API 已建立；
- 默认构建不依赖 libpsf；
- libpsf backend 可选启用；
- 已提交小型 PSF fixture；
- 已覆盖 DC scalar、AC、TRAN 和后续补入的 DC sweep；
- 失败状态可测试；
- unsupported 格式有明确边界；
- 文档已收口。

## 8. 暂缓项说明

### 8.1 legacy sensitivity

状态：暂缓。

原因：

- 缺可信参考值；
- 缺当前必须消费方；
- 不影响 AC / TRAN / DC sweep 基础结果语义闭环。

### 8.2 Spectre PSFXL transient

状态：暂缓。

当前事实：

- 已有 fixture 固化 unsupported 边界；
- 当前 libpsf backend 明确返回 `kUnsupportedFormat`；
- 尚无转换路径或 native parser。

### 8.3 原生 PSF parser

状态：暂缓。

原因：

- libpsf 已覆盖当前普通 PSF fixture；
- 原生 parser 工作量与维护成本较高；
- 当前没有必须绕开 libpsf 的已验证需求。

### 8.4 MOS I-V / gm/Id 多曲线结果

状态：暂缓。

原因：

- 当前 `DcSweep` 只表达单 axis、单 signal；
- MOS family curve 可能需要新 ResultIR；
- gm/Id lookup table 更像上层应用或独立模块。

## 9. 验证矩阵

默认测试：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

当前结果：

```text
100% tests passed, 0 tests failed out of 81
```

libpsf backend 测试：

```bash
cmake --preset libpsf
cmake --build --preset libpsf
ctest --preset libpsf --output-on-failure
```

当前结果：

```text
100% tests passed, 0 tests failed out of 94
```

说明：

- M2 文档不要求运行外部 Spectre / Ngspice；
- M3 文档负责外部 backend 对照；
- M4 文档负责 Python binding 验证。
