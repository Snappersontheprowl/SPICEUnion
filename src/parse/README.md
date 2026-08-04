# src/parse

本目录存放结果读取器与底层 result helper 实现。

M2 会先实现最小 ResultIR、通用结果读取 helper 和清晰失败语义。
Python `task_library.py` 只作为历史参考与 fixture 来源；本目录不为了强行兼容 Python
返回习惯而牺牲 C++ API 的类型安全。

若接入 `henjo/libpsf`，它只能作为可选内部 backend。公开头文件不得暴露 `PSFDataSet`、
`PSFVector`、`PSFScalar` 或 `psf.h`，默认构建也不应依赖 libpsf。

当前实现：

- `result.cpp`：`ResultStatus` 稳定文本转换。
- `result_reader.cpp`：`.raw` 目录定位、AC magnitude / phase、UGBW / phase margin、
  waveform settling time，以及公开 result reader 入口。
- `libpsf_backend.cpp`：可选内部 backend。仅在
  `SPICEUNION_ENABLE_LIBPSF_READER=ON` 时编译，用 `henjo/libpsf` 读取 `dcOp.dc`
  单信号 scalar、swept complex response 和普通 time-sweep transient；默认构建不编译
  该文件。

当前文件读取状态：

- `read_dc_value()`：默认构建返回 `kUnsupportedFormat`；启用 libpsf backend 后读取
  `dcOp.dc`。
- `read_tran_waveform()`：默认构建返回 `kUnsupportedFormat`；启用 libpsf backend 后
  读取普通 time-sweep `tran.tran`。Spectre 23.1 PSFXL transient 当前明确返回
  `kUnsupportedFormat`。
- `read_ac_response()`：默认构建返回 `kUnsupportedFormat`；启用 libpsf backend 后
  读取 swept complex PSF 数据，并将 sweep values 映射为 `frequency_hz`，将
  complex vector 拆分为 `real` / `imag`。当前已用 `stb.stb` fixture 验证，
  标准 `ac.ac` fixture 仍需补齐。
- `read_sensitivity_legacy()`：仍是 M2.3 后续待实现的 `kUnsupportedFormat` stub。

完整 netlist IR、业务 parser、objective、penalty、pass/fail 规则不属于本目录职责。
