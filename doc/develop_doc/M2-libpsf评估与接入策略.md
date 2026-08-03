# M2 libpsf 评估与接入策略

时间：2026-08-03

## 1. 背景

当前 Python 版 `spectre_materials` 使用 `libpsf` 读取 Spectre PSF 结果。M2 开始进入
SPICEUnion 的结果读取层，因此需要明确：

- `henjo/libpsf` 能否在 C++ 版中复用；
- 是否应该直接依赖它；
- 是否需要立刻自研 PSF parser；
- 如何避免第三方库影响 SPICEUnion 的公开 API 边界。

结论先行：

```text
henjo/libpsf 能用于 C++，因为它本体就是 C++ 库，Python 只是 binding。
但 SPICEUnion 不应把 libpsf 类型暴露到 include/su/。
M2 应先设计自己的 ResultIR / result_reader API，再把 libpsf 作为可选内部 backend、
fixture oracle 和 spike 对象。
是否自研 native PSF parser，应在 ResultIR、fixture 和可选 backend 跑通后再决定。
```

## 2. 已确认事实

### 2.1 libpsf 本体是 C++ 库

`henjo/libpsf` 仓库说明其用途是读取 Cadence PSF waveform files 的 C++ library。
仓库结构也包含：

```text
include/
src/
bindings/python/
test/
examples/
```

Python `libpsf.PSFDataSet(...)` 不是独立 Python parser，而是对 C++ `PSFDataSet` 的
binding。

参考资料：

- <https://github.com/henjo/libpsf>
- <https://raw.githubusercontent.com/henjo/libpsf/master/README.rst>

### 2.2 libpsf 的 C++ API 可覆盖 M2 的部分需求

上游公开头文件中存在 `PSFDataSet`，可读取 signal names、sweep values、signal vector
和 signal scalar。概念上可以支撑：

- `dcOp.dc` 标量读取；
- `ac.ac` sweep axis 与 complex response 读取；
- `tran.tran` time axis 与 waveform 读取；
- sensitivity / info 文件的部分读取尝试。

典型接口形态：

```cpp
PSFDataSet ds(filename);
auto names = ds.get_signal_names();
auto sweep = ds.get_sweep_values();
auto scalar = ds.get_signal_scalar(signal_name);
auto vector = ds.get_signal_vector(signal_name);
```

这说明它“能用得上”，但不意味着它适合直接成为 SPICEUnion 的公开接口。

### 2.3 license 是 LGPL-3.0

`henjo/libpsf` 使用 LGPL-3.0。

参考资料：

- <https://raw.githubusercontent.com/henjo/libpsf/master/COPYING>

工程含义：

- 可以考虑动态链接的可选依赖。
- 不建议把源码直接复制进 SPICEUnion core。
- 不建议在没有 license / 分发方案评估前，将其静态链接进单二进制交付路径。
- 若未来产品化或对外分发，需要重新审视合规要求。

本项目文档中的 license 判断只作为工程风险提醒，不替代正式法律意见。

### 2.4 上游维护和构建形态偏老

上游主线仓库使用 autotools，README 中的依赖包含 autoconf、automake、libtool、Boost、
Python / NumPy header 等。Python binding 部分使用 Boost.Python，构建说明偏老。

已观察到的风险：

- 构建系统不是现代 CMake；
- Python binding 时代较老，但 C++ core 仍可独立评估；
- 上游维护节奏慢，部分现代化和 bug fix 可能存在于 fork 或 PR 中；
- 大结果文件、特殊 PSF 方言、现代 Spectre 输出的稳定性需要实测；
- 裸指针和异常风格不适合直接暴露给 SPICEUnion 用户。

这些风险意味着：

```text
libpsf 可以作为 backend，但不应决定 SPICEUnion 的 API 形状。
```

## 3. 不推荐方案

### 3.1 不推荐直接暴露 libpsf API

不推荐在公开头文件中出现：

```cpp
#include <psf.h>

PSFDataSet
PSFVector
PSFScalar
```

也不推荐让用户直接写：

```cpp
su::Evaluator evaluator(...);
PSFDataSet ds(result.work_dir + "/xxx.raw/ac.ac");
```

原因：

- 会让 SPICEUnion 的公开 API 绑定到第三方库；
- 会把 PSF 格式细节泄漏到用户层；
- 会把裸指针 ownership、第三方异常和老式类型系统暴露出去；
- 未来替换 parser 时会破坏用户代码；
- C ABI / Python binding 会被迫处理 libpsf 的对象生命周期。

### 3.2 不推荐马上自研完整 PSF parser

也不推荐 M2 一开始就直接实现完整 native PSF parser。

原因：

- ResultIR 和失败语义还没落地；
- fixture 和参考数值还没固定；
- 当前只需要 dc/ac/tran/sens 子集；
- 完整 PSF 方言空间大，容易拖垮 M2；
- 过早自研会把开发重心从“稳定 API”带偏到“格式考古”。

M2 的核心不是炫技式 parser，而是：

```text
清晰 ResultIR + 可测试读取原语 + 稳定失败语义。
```

## 4. 推荐方案

推荐采用三层结构：

```text
include/su/result.hpp
include/su/result_reader.hpp
  -> SPICEUnion 公开 ResultIR / result_reader API

src/parse/libpsf_reader.hpp
src/parse/libpsf_reader.cpp
  -> 可选 libpsf backend，内部把 PSFDataSet 转换为 ResultIR

src/parse/native_psf_reader.hpp
src/parse/native_psf_reader.cpp
  -> 后续可选 native parser，替换或补充 libpsf backend
```

公开 API 只表达 SPICEUnion 自己的类型：

```cpp
ReadResult<ScalarResult> read_dc_value(...);
ReadResult<AcResponse> read_ac_response(...);
ReadResult<TranWaveform> read_tran_waveform(...);
ReadResult<std::vector<SensitivityEntry>> read_sensitivity_legacy(...);
```

内部 backend 负责转换：

```text
PSFDataSet / PSFVector / PSFScalar
  -> ScalarResult / AcResponse / TranWaveform / SensitivityEntry
```

这样 libpsf 只是“轮子”，不是“车架”。

## 5. CMake 接入策略

M2 初期不应让默认构建依赖 libpsf。

建议新增可选开关：

```cmake
SPICEUNION_ENABLE_LIBPSF_READER=OFF
```

默认：

```text
OFF
```

原因：

- 保持默认开发环境轻量；
- 默认测试不被第三方库和 license 环境卡住；
- 让 M2.1 / M2.2 可以先完成 ResultIR 与纯数学 helper；
- 等 libpsf spike 成功后再打开真实文件读取路径。

启用时可优先尝试：

```cmake
find_package(PkgConfig)
pkg_check_modules(LIBPSF libpsf)
```

若系统没有 `libpsf.pc`，再考虑手动指定：

```cmake
SPICEUNION_LIBPSF_INCLUDE_DIR
SPICEUNION_LIBPSF_LIBRARY
```

不建议在第一阶段自动下载、编译、安装 libpsf。

## 6. 建议实施顺序

### 6.1 M2.1 不依赖 libpsf

先完成：

```text
include/su/result.hpp
include/su/result_reader.hpp
tests/result_test.cpp
```

定义：

- `ResultStatus`
- `ReadResult<T>`
- `ScalarResult`
- `AcResponse`
- `AcDerivedView`
- `TranWaveform`
- `SensitivityEntry`

验收：

- 公开头文件不 include `psf.h`。
- 类型不包含业务指标语义。
- 错误状态可区分真实 `0.0` 和读取失败。

### 6.2 M2.2 仍不依赖 libpsf

先实现纯路径与纯数学 helper：

- `.raw` 目录定位；
- AC magnitude / phase 计算；
- UGBW / phase margin 计算；
- waveform settling time 计算。

这些能力不需要 PSF parser，适合先用人工数组和临时目录测试。

### 6.3 M2.3 做 libpsf spike

单独做一个 libpsf 可用性 spike，目标不是立即合入核心，而是回答：

- 当前机器是否能编译 / 链接 libpsf；
- 是否能用 C++ 读取当前 Spectre 生成的 `dcOp.dc`；
- 是否能读取 `ac.ac` 的 sweep 与 complex response；
- 是否能读取 `tran.tran` 的 time axis 与 waveform；
- 是否能读取 sensitivity / info 文件；
- 对大文件或真实 AMP fixture 是否稳定；
- 是否能通过 pkg-config 或显式 include/library 路径接入 CMake；
- license / 分发方式是否可接受。

建议产出：

```text
doc/develop_doc/M2-libpsf-spike记录.md
tests/manual/libpsf_probe.cpp
```

`tests/manual/` 只放人工验证或外部依赖验证，不进入默认测试。

### 6.4 M2.4 做可选 libpsf backend

如果 spike 成功，再实现：

```text
src/parse/libpsf_reader.hpp
src/parse/libpsf_reader.cpp
```

要求：

- 只在 `SPICEUNION_ENABLE_LIBPSF_READER=ON` 时编译；
- 不在 `include/su/` 中暴露 libpsf 类型；
- 把 libpsf exception 映射为 `ResultStatus`；
- 把裸指针 ownership 包在内部 RAII helper 中；
- 正常路径和失败路径都有测试；
- 测试可根据开关跳过。

### 6.5 M2.5 决定是否自研 native parser

在下列信息明确之后，再决定是否自研：

- ResultIR 是否稳定；
- fixture 是否覆盖真实需求；
- libpsf backend 是否能稳定读取目标文件；
- 性能是否足够；
- LGPL / 分发是否可接受；
- native parser 的最小支持子集是否明确。

若自研，目标也应是“最小可用子集”，不是完整 PSF 世界：

- `dcOp.dc`
- `ac.ac`
- `tran.tran`
- `dcOpInfo.info`
- legacy sensitivity

## 7. backend 边界规则

所有 parser backend 必须遵守：

- 不暴露第三方库类型到 `include/su/`。
- 不把 backend exception 直接抛给公开 API 用户。
- 不用 `0.0` / 空数组 / `None` 表示失败。
- 不解释业务 signal 含义。
- 不定义 objective / penalty / pass-fail。
- 不在 `TaskResult` 中塞 waveform 或 metric。
- 不让默认构建依赖外部 PSF parser。
- backend 替换不应影响 `result.hpp` / `result_reader.hpp` 的用户代码。

## 8. 测试策略

测试分四类：

### 8.1 默认单元测试

默认启用，不依赖 libpsf：

- ResultIR 类型测试；
- 错误状态测试；
- `.raw` 目录定位测试；
- AC 数学 helper 测试；
- settling time helper 测试。

### 8.2 manual libpsf probe

人工启用，不进入默认测试：

- 编译 / 链接 libpsf；
- 读取真实 Spectre 文件；
- 输出 signal names、sweep size、数据类型和前几个值；
- 记录失败文件和异常信息。

### 8.3 可选 backend 测试

仅在 `SPICEUNION_ENABLE_LIBPSF_READER=ON` 时启用：

- dc/ac/tran/sens 正常读取；
- 文件缺失；
- signal 缺失；
- unsupported format；
- parse error；
- 大文件 smoke test。

### 8.4 reference 对照测试

Python / libpsf 可以作为参考数值来源，但对照重点是数值语义：

- C++ 结果与参考值在容差内一致；
- 失败语义允许与 Python 不同；
- 文档记录有意差异。

## 9. 何时接受 libpsf backend

满足以下条件时，可以接受 libpsf backend 进入 M2：

- 默认构建不依赖 libpsf；
- 开关关闭时所有默认测试通过；
- 开关开启时能读取目标 fixture；
- libpsf 类型不出现在 `include/su/`；
- license / third-party notice 有记录；
- CMake 接入方式可复现；
- 错误映射到 `ResultStatus`；
- backend 有清晰 README 或文档小节说明。

## 10. 何时启动自研 native parser

满足任一条件时，可以考虑启动 native parser：

- libpsf 无法稳定读取当前 Spectre 输出；
- libpsf 构建成本过高；
- LGPL / 分发约束影响目标交付；
- 性能无法满足 benchmark 目标；
- 需要更细粒度错误恢复；
- 需要完全控制内存布局和 C ABI；
- 当前 fixture 已经明确 native parser 的最小支持子集。

不满足这些条件时，不急着自研。

## 11. 最终决策口径

M2 的 parser 策略是：

```text
先建自己的 ResultIR 和 result_reader API；
再用 libpsf 验证真实 PSF 读取路径；
把 libpsf 封装为可选内部 backend；
等真实需求、fixture、性能和分发约束明确后，再决定是否自研 native parser。
```

这让 SPICEUnion 同时避免两种风险：

- 盲目造轮子，过早陷入 PSF 格式细节；
- 盲目依赖旧轮子，把公开 API 和分发策略锁死。
