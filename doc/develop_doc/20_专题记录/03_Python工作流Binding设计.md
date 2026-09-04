# Python 工作流 Binding 设计

状态：`active`
最后验证：`2026-09-02`
适用范围：`bindings/python workflow API`
阶段记录：`../10_阶段记录/06_M6_Python工作流Binding.md`
事实来源：

- `bindings/python/spiceunion_py.cpp`
- `bindings/python/tests/`
- `bindings/python/examples/`

## 1. 设计目标

Python workflow binding 的目标是让 Python 用户不需要理解 SPICEUnion 的 C++ 内部执行层，
也能完成一次批量仿真和结果读取。

目标路径：

```text
spiceunion.Simulation
  -> add_parameter()
  -> run(list[dict[str, float]])
  -> list[SimulationResult]
  -> SimulationResult.read_dc/read_dc_sweep/read_ac/read_tran
  -> ScalarResult/DcSweep/AcResponse/TranWaveform
```

Python binding 应绑定 C++ workflow facade，而不是直接绑定 `Evaluator`、`SimulatorSession`
或 `OrderedConcurrentPool`。

## 2. 用户 API

### 2.1 Simulation 构造

第一版主构造采用 keyword 参数：

```python
simulation = spiceunion.Simulation(
    simulator="spectre",
    netlist_path="input.scs",
    workers=4,
    work_dir_base="local/runtime/simulations",
    workspace_namespace="",
    timeout_seconds=60,
    restart_attempts=1,
    result_format="unknown",
    ngspice_task="rc_ac",
)
```

必填：

- `netlist_path`

可选：

- `simulator`：默认 `"spectre"`；
- `workers`：默认 `1`；
- `work_dir_base`：默认 `"local/runtime/simulations"`；
- `workspace_namespace`：默认 `""`，由 C++ 自动生成；
- `timeout_seconds`：默认 `60`；
- `restart_attempts`：默认 `1`；
- `result_format`：默认 `"unknown"`；
- `ngspice_task`：默认 `"rc_ac"`。

用户不应被要求先创建 `SimulationOptions`。如果实现时暴露 `SimulationOptions`，它只能作为高级
入口，不能替代 keyword constructor。

### 2.2 simulator 字符串

第一版接受：

| Python 字符串 | C++ 映射 |
|---|---|
| `"spectre"` | `SimulatorKind::kSpectre` |
| `"ngspice"` | `SimulatorKind::kNgspice` |

非法字符串抛 `ValueError`。

是否额外暴露 `SimulatorKind` enum 可由实现阶段决定；即使暴露，也不应要求普通用户必须使用 enum。

### 2.3 result_format 字符串

第一版接受：

| Python 字符串 | C++ 映射 |
|---|---|
| `"unknown"` | `ResultFormat::kUnknown` |
| `"psf_ascii"` | `ResultFormat::kPsfAscii` |
| `"nspice_wrdata"` | `ResultFormat::kNspiceWrdata` |

非法字符串抛 `ValueError`。

### 2.4 ngspice_task 字符串

第一版接受：

| Python 字符串 | C++ 映射 |
|---|---|
| `"rc_ac"` | `NgspiceBuiltinTask::kRcAc` |
| `"rc_tran"` | `NgspiceBuiltinTask::kRcTran` |
| `"resistor_divider_dc"` | `NgspiceBuiltinTask::kResistorDividerDc` |

非法字符串抛 `ValueError`。

这是当前 Ngspice backend 的临时能力映射，不代表完整 Ngspice 任意网表支持。

## 3. 参数声明

Python API：

```python
simulation.add_parameter("wp")
simulation.add_parameter("wn", default_value=10e-6)
```

规则：

- `name` 必须是非空字符串；
- `default_value` 缺省时声明必填参数；
- `default_value` 给定时必须是 finite number；
- 重复声明同名参数抛 `ValueError`；
- 这些规则应与 C++ `Simulation.add_parameter()` 保持一致。

实现建议：

- pybind11 层使用两个 overload，或使用 `py::object default_value = py::none()`；
- Python `None` 表示无默认值；
- 不接受字符串数字自动转换，例如 `"1e-6"`。

## 4. run 输入

Python API：

```python
results = simulation.run([
    {"wp": 14e-6, "wn": 10e-6},
    {"wp": 16e-6, "wn": 11e-6},
])
```

规则：

- `cases` 必须是 list-like sequence；
- 每个 case 必须是 dict-like mapping；
- key 必须是字符串；
- value 必须是 finite number；
- 空 list 返回空 list；
- 未声明参数、缺少必填参数、非 finite 值都抛 `ValueError`；
- 结构性非法 batch 不启动底层仿真。

实现建议：

- pybind11 可先接收 `std::vector<std::map<std::string, double>>`；
- 若 pybind11 默认转换产生的错误信息太底层，再改为手动解析 `py::sequence` / `py::dict`；
- 第一版优先钉住行为，不追求复杂 Python typing 兼容。

## 5. SimulationResult

Python 暴露字段 / 方法：

| 名称 | 类型 | 含义 |
|---|---|---|
| `ok()` | method | 单点仿真是否成功 |
| `status_text()` | method | `TaskStatus` 稳定文本 |
| `status` | enum 或 string | 单点仿真状态；第一版可选 |
| `message` | string property | 单点失败摘要 |
| `detail` | string property | 单点诊断细节 |
| `work_dir` | string property | 高级诊断字段 |
| `result_format` | enum 或 string | 高级诊断字段 |
| `read_dc(signal_name)` | method | 返回 `ScalarResult` |
| `read_dc_sweep(sweep_name, signal_name, filename="dc.dc")` | method | 返回 `DcSweep` |
| `read_ac(signal_name, filename="ac.ac")` | method | 返回 `AcResponse` |
| `read_tran(signal_name, filename="tran.tran")` | method | 返回 `TranWaveform` |

普通用户只需要 `ok()`、`message` 和 `read_*()`。`work_dir`、`result_format` 只作为诊断字段。

## 6. Python result 对象复用

M4 已有 Python result 包装：

- `ScalarResult`
- `DcSweep`
- `AcResponse`
- `AcDerivedView`
- `TranWaveform`
- `UgbwPhaseMarginResult`
- `SettlingTimeResult`

M6 不新建另一套同名 result 类型。`SimulationResult.read_*()` 必须返回现有包装对象，保持：

- `ok()`；
- `status_text()`；
- `status`；
- `message`；
- `shape_consistent()`；
- `len()`。

这避免 Python 用户在“直接读 fixture”和“跑仿真后读结果”两条路径上面对两套不一致对象。

## 7. 生命周期

Python `Simulation` 应支持：

```python
simulation.cleanup()
```

以及：

```python
with spiceunion.Simulation(netlist_path="input.scs") as simulation:
    ...
```

规则：

- `cleanup()` 可重复调用；
- `__exit__()` 调用 `cleanup()`；
- `Simulation` 对象析构时仍依赖 C++ RAII，但示例优先推荐 context manager；
- 不暴露 worker 或 session 生命周期。

## 8. 异常与失败语义

| 场景 | Python 行为 |
|---|---|
| `netlist_path=""` | `ValueError` |
| `workers <= 0` | `ValueError` |
| 参数名为空 | `ValueError` |
| 默认值不是 finite number | `ValueError` |
| case 出现未声明参数 | `ValueError` |
| case 缺少必填参数 | `ValueError` |
| case value 非 finite | `ValueError` |
| 单个仿真失败 | 返回 `SimulationResult`，`ok() == False` |
| 结果文件缺失 | 返回 result 对象，`ok() == False` |
| signal 缺失 | 返回 result 对象，`ok() == False` |

原则：

- Python 类型错误可以使用 pybind11 默认异常；
- workflow 结构错误应尽量给出用户可读 `ValueError`；
- 普通仿真失败和普通结果读取失败不抛异常。

## 9. 测试切分

### 9.1 默认 Python preset

默认 Python preset 不依赖外部 EDA 工具。

已新增：

- `bindings/python/tests/test_workflow_api_contract.py`

覆盖：

- `Simulation` 存在；
- 最小 keyword constructor 可创建对象；
- `add_parameter()` 正常；
- `run([])` 返回空 list；
- 未声明参数抛 `ValueError`；
- 缺少必填参数抛 `ValueError`；
- 非 finite 参数抛 `ValueError`；
- `cleanup()` 可重复调用；
- context manager 可进入和退出。

### 9.2 外部 simulator preset

已新增可跳过测试：

- `bindings/python/tests/test_workflow_external.py`

覆盖：

- Ngspice workflow smoke；
- 成功 `SimulationResult` 可调用 `read_*()`。

外部测试默认跳过真实执行。设置 `SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS=1` 后，
若本机存在 `ngspice` 或 `SPICEUNION_NGSPICE` 指向可执行文件，则运行真实 Ngspice workflow。
Spectre Python workflow smoke 暂缓。

## 10. 示例脚本

已新增：

- `bindings/python/examples/run_workflow.py`

示例应展示：

- 创建 `Simulation`；
- 声明参数；
- 提交 batch；
- 遍历 `SimulationResult`；
- 成功时读取信号；
- 失败时打印 `status_text()` 和 `message`。

示例不应引入 optimizer 或业务 metric，以免混淆 SPICEUnion core 边界。

## 11. 实现注意事项

- `spiceunion_py.cpp` 当前已经较长；若 M6 实现导致文件继续膨胀，可先在
  `bindings/python/README.md` 记录拆分方向，再进行源码拆分。
- `PySimulationResult` 可以持有 `su::SimulationResult`，其 `read_*()` 方法内部调用现有
  `to_python()` 转换函数。
- `PySimulation` 可以持有 `std::unique_ptr<su::Simulation>`，负责 keyword constructor、
  `add_parameter()`、`run()`、`cleanup()` 和 context manager。
- 不把 `make_simulation_for_session_factory()` 暴露给 Python 普通用户。
- 不为了 Python 测试暴露 fake session factory。
- 如果确实需要 fake workflow 测试，应优先保留在 C++ `tests/unit/workflow/workflow_test.cpp`。

## 12. 完成定义

当前完成事实：

- Python workflow API 已实现；
- 默认 Python preset 覆盖 API contract；
- Ngspice external workflow smoke 有明确门控；
- Python README 和 examples 已更新；
- `00_项目总览/01_当前事实状态.md`、`02_架构总览.md`、`03_开发路线图.md` 已更新；
- M6 阶段记录完成收口。
