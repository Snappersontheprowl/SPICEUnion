# bindings/python

本目录存放 SPICEUnion 的 pybind11 Python 绑定层。

当前定位：

- 这是 C++ core 的可选外部语言接口；
- 默认构建不启用；
- 第一版只绑定结果读取 helper 和少量基础结果类型；
- M6 Python workflow binding 文档侧已完成，后续应绑定 `Simulation` /
  `SimulationResult`；
- 不绑定 `Evaluator`、`SimulatorSession`、`SpectreSession`、`NgspiceSession`；
- 不实现 C ABI；
- 不引入 numpy；
- 不做 wheel/package 发布。

## 构建

默认构建不会构建 Python extension。

开启方式：

```bash
cmake --preset python
cmake --build --preset python
ctest --preset python --output-on-failure
```

若本机没有安装 pybind11，CMake 只会在
`SPICEUNION_BUILD_PYTHON_BINDINGS=ON` 时通过 `FetchContent` 获取 pybind11。

若同时需要真实读取 Spectre PSF fixture，应启用 libpsf backend：

```bash
cmake --preset python-libpsf-pic
cmake --build --preset python-libpsf-pic
ctest --preset python-libpsf-pic --output-on-failure
```

注意：Python extension 是 shared module。如果链接的是静态 libpsf，libpsf 需要以
PIC 方式构建。本机验证使用：

```text
local/external/libpsf/install-pic
```

CMake 会优先查找 `install-pic`，再查找 `install`。这两个目录均是本机外部依赖构建产物，
不进入版本库。

当前本机验证结果：

- Python binding 默认无 libpsf：`91/91` passed；
- Python binding + libpsf PIC：`110/110` passed。

## 当前 Python API

模块名：

```python
import spiceunion
```

当前绑定：

- `version()`
- `libpsf_reader_enabled()`
- `status_text()`
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

## Python workflow binding 计划

M6 计划把 M5 的 C++ workflow facade 绑定到 Python，而不是直接暴露底层执行层对象。

目标用户路径：

```python
import spiceunion as su

simulation = su.Simulation(simulator="spectre", netlist_path="input.scs", workers=4)
simulation.add_parameter("wp")
simulation.add_parameter("wn")

results = simulation.run([
    {"wp": 14e-6, "wn": 10e-6},
])

if results[0].ok():
    ac = results[0].read_ac("out")
```

文档入口：

- `../../doc/develop_doc/10_阶段记录/06_M6_Python工作流Binding.md`
- `../../doc/develop_doc/20_专题记录/03_Python工作流Binding设计.md`

## API 契约

### 状态与失败语义

- 普通读取失败返回 result 对象，不抛异常。
- 调用方使用 `ok()` 判断成功。
- 失败原因使用 `status`、`status_text()` 和 `message`。
- Python 参数类型错误仍由 pybind11 按 Python 异常处理。
- `file_not_found`、`signal_not_found`、`unsupported_format` 这类可预期失败不使用异常表达。

示例：

```python
result = spiceunion.read_dc_value(result_dir, "vout")
if not result.ok():
    print(result.status_text(), result.message)
```

### 当前 result 对象字段

| 类型 | 字段 / 方法 |
|---|---|
| `ScalarResult` | `status`、`message`、`signal`、`value`、`ok()`、`status_text()` |
| `DcSweep` | `status`、`message`、`sweep_name`、`signal`、`sweep_values`、`values`、`ok()`、`status_text()`、`shape_consistent()`、`len()` |
| `AcResponse` | `status`、`message`、`signal`、`frequency_hz`、`real`、`imag`、`ok()`、`status_text()`、`shape_consistent()`、`len()` |
| `AcDerivedView` | `status`、`message`、`frequency_hz`、`magnitude_db`、`phase_deg`、`ok()`、`status_text()`、`shape_consistent()`、`len()` |
| `TranWaveform` | `status`、`message`、`signal`、`time_s`、`value`、`ok()`、`status_text()`、`shape_consistent()`、`len()` |
| `UgbwPhaseMarginResult` | `status`、`message`、`unity_gain_bandwidth_hz`、`phase_margin_deg`、`ok()`、`status_text()` |
| `SettlingTimeResult` | `status`、`message`、`settling_time_s`、`ok()`、`status_text()` |

### pytest 取舍

当前不引入 pytest。原因：

- SPICEUnion 仍是 C++ / CMake 主项目；
- M4.2 的 Python 测试仍属于 binding smoke 与 API contract；
- CTest 可以统一管理 C++ 测试、Python import 测试、fixture 读取测试和示例脚本测试。

若后续 Python API 变成独立用户入口，再重新评估 pytest、package layout 和 wheel 发布。

## 示例脚本

示例脚本位于 `bindings/python/examples/`：

| 脚本 | 用途 |
|---|---|
| `read_fixture_results.py` | 演示读取 DC / AC / TRAN fixture 和失败状态 |
| `analyze_fixture_results.py` | 演示基于 Python list 做 DC / AC / TRAN 最小后处理 |

说明文档见 `bindings/python/examples/README.md`。

最小用法：

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
- 示例脚本放在 `bindings/python/examples/`。
- 不使用 `py/`、`python2/`、`new_binding/` 等阶段性目录名。
