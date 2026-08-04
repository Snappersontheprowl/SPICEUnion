# M2 libpsf 接入策略与当前口径

时间：2026-08-04

本文记录 M2 阶段 `henjo/libpsf` 在 SPICEUnion 中的当前接入口径。早期“是否可用”
类探索表述已经删除；当前事实是：libpsf 已作为可选内部 backend 接入，并已覆盖
DC、标准 AC、STB 与普通 transient fixture。

## 当前结论

`henjo/libpsf` 可以在 C++ 版本中使用，但只能作为内部 backend。

SPICEUnion 的公开接口保持为自己的 ResultIR：

```cpp
ReadResult<ScalarResult> read_dc_value(...);
ReadResult<AcResponse> read_ac_response(...);
ReadResult<TranWaveform> read_tran_waveform(...);
ReadResult<std::vector<SensitivityEntry>> read_sensitivity_legacy(...);
```

libpsf 类型不得出现在公开头文件中。

当前策略：

```text
公开 API       -> SPICEUnion ResultIR / ReadResult<T>
内部 backend   -> henjo/libpsf PSFDataSet
默认构建       -> 不依赖 libpsf
显式开关       -> SPICEUNION_ENABLE_LIBPSF_READER=ON
native parser  -> 后续可选演进方向，不属于当前已完成事实
```

## 已确认事实

### libpsf 本体是 C++ 库

`henjo/libpsf` 是读取 Cadence PSF waveform files 的 C++ library，Python 版
`libpsf.PSFDataSet(...)` 是对 C++ `PSFDataSet` 的 binding。

工程含义：

- C++ 版本可以复用其核心读取能力。
- SPICEUnion 不需要通过 Python binding 间接读取 PSF。
- 第三方 API 必须封装在 `src/parse/` 内部。

### license 是 LGPL-3.0

`henjo/libpsf` 使用 LGPL-3.0。

工程口径：

- 不复制 libpsf 源码到 SPICEUnion core。
- 不让默认构建强依赖 libpsf。
- 产品化分发或静态链接场景需要单独处理 license notice 和合规策略。
- 本项目文档中的 license 判断只作为工程风险提醒，不替代正式法律意见。

### 上游构建形态偏老

上游主线使用 autotools，Python binding 使用 Boost.Python，构建方式不适合作为
SPICEUnion 默认依赖。

当前处理方式：

- SPICEUnion 不自动下载或构建 libpsf。
- 本机 spike 使用 `local/external/libpsf/` 保存外部源码、构建和安装产物。
- CMake 通过显式 include/library 路径或本机 spike install 路径接入。

## 当前实现结构

```text
include/su/result.hpp
include/su/result_reader.hpp
  -> SPICEUnion 公开 ResultIR / result_reader API

src/parse/result_reader.cpp
  -> 公开入口调度与默认 unsupported 行为

src/parse/libpsf_backend.hpp
src/parse/libpsf_backend.cpp
  -> 可选 libpsf backend
```

公开 API 只表达 SPICEUnion 类型：

```text
PSFDataSet / PSFVector / PSFScalar
  -> ScalarResult / AcResponse / TranWaveform / SensitivityEntry
```

## CMake 接入策略

当前开关：

```cmake
SPICEUNION_ENABLE_LIBPSF_READER=OFF
```

默认构建：

- 不寻找 libpsf。
- 不编译 `src/parse/libpsf_backend.cpp`。
- 调用真实文件读取入口时返回 `kUnsupportedFormat`。

启用构建：

```bash
cmake -S . -B cmake-build-libpsf \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSPICEUNION_BUILD_TESTS=ON \
  -DSPICEUNION_ENABLE_EXTERNAL_TESTS=OFF \
  -DSPICEUNION_ENABLE_LIBPSF_READER=ON
```

可显式指定：

```cmake
SPICEUNION_LIBPSF_INCLUDE_DIR=/path/to/libpsf/include
SPICEUNION_LIBPSF_LIBRARY=/path/to/libpsf.a
```

## 当前支持范围

| 入口 | 默认构建 | 启用 libpsf backend 后 |
|---|---|---|
| `read_dc_value()` | `kUnsupportedFormat` | 读取 `dcOp.dc` 单信号 scalar |
| `read_ac_response()` | `kUnsupportedFormat` | 读取标准 `ac.ac` / `stb.stb` swept complex response |
| `read_tran_waveform()` | `kUnsupportedFormat` | 读取普通 time-sweep `tran.tran` |
| `read_sensitivity_legacy()` | `kUnsupportedFormat` | 当前仍为 stub |

当前明确不支持：

- Spectre 23.1 PSFXL transient。
- 尚未固化的 legacy sensitivity 文件。
- 完整 PSF 方言世界。

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

## 当前测试策略

默认单元测试不依赖 libpsf：

- ResultIR 类型测试。
- 错误状态测试。
- `.raw` 目录定位测试。
- AC 数学 helper 测试。
- settling time helper 测试。

启用 `SPICEUNION_ENABLE_LIBPSF_READER=ON` 后额外覆盖：

- `dcOp.dc` 正常读取。
- 标准 `ac.ac` 正常读取。
- `stb.stb` 正常读取。
- 普通 `tran.tran` 正常读取。
- 文件缺失。
- signal 缺失。
- unsupported format。
- PSFXL transient 边界样本。

Python / libpsf 可以作为参考数值来源，但对照重点是数值语义：

- C++ 结果与参考值在容差内一致。
- C++ 失败语义允许与 Python 不同。
- 真实数值 `0.0` 与读取失败必须可区分。

## native parser 当前口径

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

## 当前剩余事项

通用仿真结果解析工作当前告一段落。

仍需保留的未完成事项：

- legacy sensitivity：缺 fixture、缺可信参考值，读取入口仍为 stub。
- Spectre 23.1 PSFXL transient：已有边界 fixture，当前明确不支持，后续需单独决策。

除此之外，DC、标准 AC、STB 与普通 transient 的通用读取链路已经完成项目内 fixture
验证。
