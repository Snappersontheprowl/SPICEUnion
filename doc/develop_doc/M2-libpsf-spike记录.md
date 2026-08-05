# M2 libpsf 接入与验证状态

时间：2026-08-04

本文只记录当前已经验证的事实状态。早期探索过程和“下一步尝试”类表述已收口删除。

## 当前结论

`henjo/libpsf` 已在 SPICEUnion 中作为可选内部 backend 接入，用于 M2 结果读取层的
真实 PSF fixture 验证。

当前边界保持不变：

- 默认构建不编译、不链接 libpsf。
- 只有显式启用 `SPICEUNION_ENABLE_LIBPSF_READER=ON` 时才编译 backend。
- `include/su/` 公开头文件不暴露 `psf.h`、`PSFDataSet`、`PSFVector`、
  `PSFScalar` 或其他 libpsf 类型。
- libpsf exception、裸指针 ownership 和格式细节都封装在 `src/parse/` 内部。
- libpsf 是 backend，不是 SPICEUnion 的公开 API 车架。

## 本机 libpsf 状态

本项目在本机使用独立路径保存外部 libpsf spike 产物：

```text
local/external/libpsf/
├── src/       # henjo/libpsf clone 与本机临时 CMake 构建脚本
├── build/     # 本机构建目录
├── install/   # 本机安装目录
└── libpsf_probe
```

当前可用安装产物：

```text
local/external/libpsf/install/include/psf.h
local/external/libpsf/install/include/psfdata.h
local/external/libpsf/install/lib64/libpsf.a
```

说明：

- `local/external/libpsf/` 不进入 SPICEUnion 版本库。
- SPICEUnion 不自动下载、构建或安装 libpsf。
- 本机临时 CMake 构建脚本只服务验证，不代表上游源码被纳入项目。
- Cadence 自带 `libpsf.so` 不是 `henjo/libpsf` API，不能按 `PSFDataSet` 方式使用。

## License 与分发口径

`henjo/libpsf` 使用 LGPL-3.0。

当前工程口径：

- 不复制 libpsf 源码到 SPICEUnion core。
- 不让默认构建强依赖 libpsf。
- 产品化分发或静态链接场景需要单独处理 license notice 和合规策略。
- 根 `TODO` 保留 `libpsf backend 产品化分发与 license notice` 监控项。
- 本项目文档中的 license 判断只作为工程风险提醒，不替代正式法律意见。

## CMake 接入状态

当前 CMake 开关：

```cmake
SPICEUNION_ENABLE_LIBPSF_READER=OFF
```

默认值为 `OFF`。

启用时，CMake 会优先使用显式路径；未显式指定时，会尝试本机默认 spike 安装位置：

```text
local/external/libpsf/install/include
local/external/libpsf/install/lib
local/external/libpsf/install/lib64
```

可显式指定：

```cmake
SPICEUNION_LIBPSF_INCLUDE_DIR=/path/to/libpsf/include
SPICEUNION_LIBPSF_LIBRARY=/path/to/libpsf.a
```

libpsf include 目录按 `SYSTEM` include 处理，避免第三方 header warning 影响项目自身
warning 策略。

## 已接入代码路径

当前文件读取入口：

```text
include/su/result_reader.hpp
  -> src/parse/result_reader.cpp
  -> src/parse/libpsf_backend.cpp
  -> henjo/libpsf PSFDataSet
```

当前 backend 文件：

```text
src/parse/libpsf_backend.hpp
src/parse/libpsf_backend.cpp
```

当前 manual probe：

```text
tests/manual/libpsf_probe.cpp
```

manual probe 只用于人工检查 PSF 文件结构，不进入默认 `ctest`。

## backend 边界规则

所有 parser backend 必须遵守：

- 不暴露第三方库类型到 `include/su/`。
- 不把 backend exception 直接抛给公开 API 用户。
- 不用 `0.0`、空数组或 `None` 表示失败。
- 不解释业务 signal 含义。
- 不定义 objective、penalty 或 pass/fail。
- 不在 `TaskResult` 中塞 waveform 或 metric。
- 不让默认构建依赖外部 PSF parser。
- backend 替换不应影响 `result.hpp` / `result_reader.hpp` 用户代码。

## 当前支持的读取能力

### DC scalar

入口：

```cpp
su::read_dc_value(result_dir, signal_name)
```

当前行为：

- 默认构建返回 `kUnsupportedFormat`。
- 启用 libpsf backend 后读取 `dcOp.dc` 中的单信号 scalar。
- 缺失文件返回 `kFileNotFound`。
- 缺失 signal 返回 `kSignalNotFound`。

### AC / STB swept complex response

入口：

```cpp
su::read_ac_response(result_dir, signal_name, filename = "ac.ac")
```

当前行为：

- 默认构建返回 `kUnsupportedFormat`。
- 启用 libpsf backend 后读取 swept complex PSF 数据。
- PSF sweep values 映射为 `AcResponse.frequency_hz`。
- `PSFComplexDoubleVector(signal)` 拆分为 `AcResponse.real` / `AcResponse.imag`。
- 若 signal 存在但不是 complex vector，返回 `kUnsupportedFormat`。
- 标准 `ac.ac` 与 `stb.stb` 都已有 fixture 覆盖。

### 普通 transient waveform

入口：

```cpp
su::read_tran_waveform(result_dir, signal_name, filename = "tran.tran")
```

当前行为：

- 默认构建返回 `kUnsupportedFormat`。
- 启用 libpsf backend 后读取普通 time-sweep PSF。
- PSF sweep values 映射为 `TranWaveform.time_s`。
- `PSFDoubleVector(signal)` 映射为 `TranWaveform.value`。
- 若发现同名 `.psfxl` 或 `.sig` sidecar，明确返回 `kUnsupportedFormat`。

### legacy sensitivity

入口：

```cpp
su::read_sensitivity_legacy(work_dir)
```

当前状态：

- 仍为 `kUnsupportedFormat` stub。
- 尚未固化 legacy sensitivity fixture。
- 尚未建立可信参考数值。

## 已固化 fixture

项目内 PSF fixture 位于：

```text
tests/fixtures/psf/
```

当前 fixture：

| fixture | 来源 | 用途 | 已知参考值 |
|---|---|---|---|
| `dc_op_minimal.raw/dcOp.dc` | `henjo/libpsf` 上游测试数据 | 最小 DC scalar | `vout = 2.5` |
| `spectre_materials_dc_op.raw/dcOp.dc` | `spectre_materials` 历史 Spectre 输出 | Cadence Spectre 23.1 DC scalar | `net6 = 0.8` |
| `amp_ac_response.raw/ac.ac` | `spectre_materials/local/runtime/amp_ac_input_20260804_160332/input.raw/ac.ac` | 标准 AC swept complex response | `freq[0] = 1`，`freq[1]≈1.51356`，`net1[0]≈(1.00000109476, -3.33776e-7)`，共 51 点 |
| `spectre_materials_stb_loop_gain.raw/stb.stb` | `spectre_materials` 历史 Spectre 输出 | STB swept complex response | `freq[0] = 1`，`freq[1]≈1.12202`，`loopGain[0]≈(-287890, 26932.1)`，共 201 点 |
| `tran_time_sweep.raw/tran.tran` | `henjo/libpsf` 上游 examples | 普通 time-sweep transient waveform | `INN` 共 323 点，`time[0] = 0`，`INN[0] = 0.6` |
| `spectre_materials_psfxl_tran.raw/tran.tran.tran` | `spectre_materials/local/runtime/amp_tran_input_20260804_1435/input.raw` | PSFXL transient 边界样本 | 当前 libpsf backend 返回 `kUnsupportedFormat` |

## 已确认边界

### Spectre 23.1 PSFXL transient

当前 fixture：

```text
tests/fixtures/psf/spectre_materials_psfxl_tran.raw/
├── tran.tran.tran
├── tran.tran.tran.psfxl
└── tran.tran.tran.sig
```

当前行为：

```text
read_tran_waveform(..., filename="tran.tran.tran")
  -> 发现同名 .psfxl 或 .sig
  -> 返回 kUnsupportedFormat
```

当前结论：

- `henjo/libpsf` backend 不直接支持该 Spectre 23.1 PSFXL transient 形态。
- 该 fixture 保留为边界样本，防止未来误把不可读格式解释成空波形或合法零值。
- PSFXL 支持需要单独设计，不混入普通 `tran.tran` 读取路径。

可能路线：

1. 研究 Cadence PSFXL 二进制格式并实现 native parser。
2. 调用 Cadence 工具做外部转换，但这会引入 Cadence runtime 依赖，不适合作为默认
   reader backend。
3. 调整仿真输出格式，生成 `henjo/libpsf` 能读取的普通 PSF / psfascii fixture。

### native parser 当前口径

native PSF parser 不是当前已完成能力。

只有满足以下条件之一时，才重新评估 native parser：

- libpsf 无法覆盖目标格式。
- LGPL / 分发约束影响目标交付。
- 性能无法满足后续 benchmark 目标。
- 需要更细粒度错误恢复。
- 需要完全控制内存布局和 C ABI。
- 当前 fixture 已经明确 native parser 的最小支持子集。

当前 native parser 的可能最小支持子集是：

- `dcOp.dc`
- `ac.ac`
- `tran.tran`
- `dcOpInfo.info`
- legacy sensitivity

## 测试状态

默认构建：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

当前结果：

```text
100% tests passed, 0 tests failed out of 40
```

启用 libpsf backend：

```bash
cmake -S . -B cmake-build-libpsf \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSPICEUNION_BUILD_TESTS=ON \
  -DSPICEUNION_ENABLE_EXTERNAL_TESTS=OFF \
  -DSPICEUNION_ENABLE_LIBPSF_READER=ON

cmake --build cmake-build-libpsf
ctest --test-dir cmake-build-libpsf --output-on-failure
```

当前结果：

```text
100% tests passed, 0 tests failed out of 50
```

libpsf backend 测试覆盖：

- `dcOp.dc` 正常读取。
- 标准 `ac.ac` 正常读取。
- `stb.stb` 正常读取。
- 普通 `tran.tran` 正常读取。
- 缺失文件、缺失 signal、非 complex response、PSFXL transient unsupported format。

## 当前剩余事项

通用仿真结果解析工作当前告一段落。M2.3 当前只保留两类明确边界事项：

- legacy sensitivity：缺 fixture、缺可信参考值、读取入口仍为 stub。
- Spectre 23.1 PSFXL transient：已有边界 fixture，当前明确不支持，后续需单独决策。
