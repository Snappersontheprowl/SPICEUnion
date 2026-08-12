# bindings/python

本目录存放 SPICEUnion 的 pybind11 Python 绑定层。

当前定位：

- 这是 C++ core 的可选外部语言接口；
- 默认构建不启用；
- 第一版只绑定结果读取 helper 和少量基础结果类型；
- 不绑定 `Evaluator`、`SimulatorSession`、`SpectreSession`、`NgspiceSession`；
- 不实现 C ABI；
- 不引入 numpy；
- 不做 wheel/package 发布。

## 构建

默认构建不会构建 Python extension。

开启方式：

```bash
cmake -S . -B cmake-build-python \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSPICEUNION_BUILD_TESTS=ON \
  -DSPICEUNION_BUILD_PYTHON_BINDINGS=ON
cmake --build cmake-build-python
ctest --test-dir cmake-build-python --output-on-failure
```

若本机没有安装 pybind11，CMake 只会在
`SPICEUNION_BUILD_PYTHON_BINDINGS=ON` 时通过 `FetchContent` 获取 pybind11。

若同时需要真实读取 Spectre PSF fixture，应启用 libpsf backend：

```bash
cmake -S . -B cmake-build-python-libpsf \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSPICEUNION_BUILD_TESTS=ON \
  -DSPICEUNION_BUILD_PYTHON_BINDINGS=ON \
  -DSPICEUNION_ENABLE_LIBPSF_READER=ON
cmake --build cmake-build-python-libpsf
ctest --test-dir cmake-build-python-libpsf --output-on-failure
```

注意：Python extension 是 shared module。如果链接的是静态 libpsf，libpsf 需要以
PIC 方式构建。本机验证使用：

```text
local/external/libpsf/install-pic
```

CMake 会优先查找 `install-pic`，再查找 `install`。这两个目录均是本机外部依赖构建产物，
不进入版本库。

当前本机验证结果：

- Python binding 默认无 libpsf：`83/83` passed；
- Python binding + libpsf PIC：`96/96` passed。

## 当前 Python API

模块名：

```python
import spiceunion
```

当前绑定：

- `version()`
- `libpsf_reader_enabled()`
- `ResultStatus`
- `ScalarResult`
- `DcSweep`
- `AcResponse`
- `AcDerivedView`
- `TranWaveform`
- `UgbwPhaseMarginResult`
- `SettlingTimeResult`
- `read_dc_value()`
- `read_dc_sweep()`
- `read_ac_response()`
- `read_tran_waveform()`
- `derive_ac_view()`
- `calculate_ugbw_and_phase_margin()`
- `calculate_settling_time()`

结果读取函数返回带状态对象，不抛出异常来表示普通读取失败。

示例：

```python
import spiceunion

response = spiceunion.read_dc_sweep(result_dir, "vin_dc", "out")
if response.ok():
    print(response.sweep_values)
    print(response.values)
else:
    print(response.status, response.message)
```

## 命名规则

- binding 源码放在 `bindings/python/`。
- extension 模块名固定为 `spiceunion`。
- 测试脚本放在 `bindings/python/tests/`。
- 不使用 `py/`、`python2/`、`new_binding/` 等阶段性目录名。
