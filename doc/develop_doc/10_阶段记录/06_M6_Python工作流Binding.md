# M6 Python 工作流 Binding 开发文档

更新时间：2026-09-02

本文负责 SPICEUnion M6 阶段的开发目标、范围边界、实施切分和完成定义。

当前状态：

```text
M6 Python workflow binding 第一版已完成；
本阶段应绑定 M5 workflow facade，而不是绑定底层 Evaluator / Session / Pool。
```

## 1. M6 目标

M6 的目标是让 Python 用户也能使用和 C++ 用户一致的低负担 workflow 主路径：

```text
import spiceunion as su

simulation = su.Simulation(
    simulator="spectre",
    netlist_path="input.scs",
    workers=4,
)

simulation.add_parameter("wp")
simulation.add_parameter("wn")

results = simulation.run([
    {"wp": 14e-6, "wn": 10e-6},
    {"wp": 16e-6, "wn": 11e-6},
])

ac = results[0].read_ac("out")
```

M6 不是把 M1 执行层原样暴露给 Python，而是把 M5 的 `Simulation` /
`SimulationResult` 用户模型绑定到 Python。

## 2. 和 M4 / M5 的关系

| 阶段 | 已有职责 | M6 如何承接 |
|---|---|---|
| M4 | pybind11 框架、Python result 对象、结果读取 helper | 继续复用现有 Python result 包装和 CTest 入口 |
| M5 | C++ workflow facade、参数声明、batch run、`SimulationResult.read_*()` | Python 绑定这一层 facade，不绕回底层执行对象 |
| M6 | Python 侧一次仿真主路径 | 建立 `spiceunion.Simulation` 和 `spiceunion.SimulationResult` |

M6 完成后，M4 文档仍负责语言绑定基础设施；M5 文档仍负责 C++ workflow facade；
M6 文档负责 Python workflow binding 阶段收口。

## 3. 已确认开发基线

| 事项 | M6 基线 |
|---|---|
| 绑定技术 | 继续使用 pybind11 |
| Python 模块名 | `spiceunion` |
| 源码目录 | `bindings/python/` |
| 绑定对象 | `Simulation`、`SimulationResult`，必要时补 `TaskStatus` / `SimulatorKind` |
| 用户构造方式 | 优先支持 keyword constructor，不强制用户先创建 options 对象 |
| 参数 batch 输入 | `list[dict[str, float]]` |
| 读取返回值 | 复用当前 `ScalarResult`、`DcSweep`、`AcResponse`、`TranWaveform` 包装 |
| 输入错误 | Python `ValueError` |
| 单点仿真失败 | `SimulationResult.ok() == False` |
| 结果读取失败 | result 对象 `ok() == False` |
| 默认测试 | 不依赖外部 Spectre / Ngspice |
| 外部测试 | 已有显式开启的 Ngspice workflow smoke；Spectre workflow smoke 暂缓 |

## 4. 范围

### 4.1 范围内

- 在 `bindings/python/spiceunion_py.cpp` 中绑定 workflow 层。
- Python 暴露 `Simulation`，支持 keyword 风格构造。
- Python 暴露 `Simulation.add_parameter(name, default_value=None)`。
- Python 暴露 `Simulation.run(cases)`，接受 `list[dict[str, float]]`。
- Python 暴露 `Simulation.cleanup()`。
- Python `Simulation` 支持 context manager：`with su.Simulation(...) as sim:`。
- Python 暴露 `SimulationResult.ok()`、`status_text()`、`message`、`detail`。
- Python `SimulationResult.read_dc()`、`read_dc_sweep()`、`read_ac()`、`read_tran()`
  返回现有 Python result 对象。
- 补充 Python API contract tests。
- 补充至少一个不依赖外部 EDA 工具的 workflow 输入契约测试。
- 若本机或预设允许，补充可跳过的外部 workflow smoke test。
- 更新 `bindings/python/README.md` 和示例脚本。

### 4.2 范围外

- 不暴露 `Evaluator`。
- 不暴露 `SimulatorSession`、`SessionFactory`、`SpectreSession`、`NgspiceSession`。
- 不暴露 `OrderedConcurrentPool`。
- 不实现 C ABI。
- 不引入 numpy。
- 不引入 pytest。
- 不做 wheel/package 发布。
- 不在 Python 侧实现 optimizer、objective、metric 或 netlist DSL。
- 不改变 M5 C++ workflow facade 的用户契约，除非先回到 M5 文档修订。

## 5. Python API 第一版形态

推荐主入口：

```python
import spiceunion as su

simulation = su.Simulation(
    simulator="spectre",
    netlist_path="input.scs",
    workers=4,
    work_dir_base="local/runtime/simulations",
    workspace_namespace="python-demo",
    timeout_seconds=60,
    restart_attempts=1,
    result_format="psf_ascii",
)

simulation.add_parameter("wp")
simulation.add_parameter("wn", default_value=10e-6)

results = simulation.run([
    {"wp": 14e-6},
    {"wp": 16e-6, "wn": 11e-6},
])
```

上下文管理：

```python
with su.Simulation(simulator="spectre", netlist_path="input.scs") as simulation:
    simulation.add_parameter("wp")
    results = simulation.run([{"wp": 14e-6}])
```

读取：

```python
result = results[0]
if result.ok():
    ac = result.read_ac("out")
    if ac.ok():
        print(ac.frequency_hz, ac.real, ac.imag)
else:
    print(result.status_text(), result.message)
```

详细设计见 `../20_专题记录/03_Python工作流Binding设计.md`。

## 6. 失败语义

M6 继承 M5 三层失败语义，并映射到 Python：

| 层级 | C++ 语义 | Python 语义 |
|---|---|---|
| workflow 输入非法 | `std::invalid_argument` | `ValueError` |
| 单个仿真任务失败 | `SimulationResult.ok() == false` | `SimulationResult.ok() == False` |
| 单个结果读取失败 | `ReadResult<T>.ok() == false` | `ScalarResult/DcSweep/AcResponse/TranWaveform.ok() == False` |

Python API 不用异常表达普通仿真点失败，也不用异常表达可预期的结果读取失败。

## 7. 测试策略

默认 Python preset 必须覆盖：

- `import spiceunion`；
- `Simulation` 类存在；
- keyword constructor 接受合法最小参数；
- `add_parameter()` 支持必填参数和默认参数；
- `run([])` 返回空 list 且不需要外部 EDA 工具；
- 未声明参数、缺少必填参数、非 finite 参数抛 `ValueError`；
- `Simulation` 支持 `cleanup()`；
- `Simulation` 支持 context manager；
- Python result 对象字段和方法契约不回退。

外部 EDA 工具可用时再覆盖：

- Python `Simulation(..., simulator="spectre")` 可跑真实最小 workflow；
- Python `Simulation(..., simulator="ngspice")` 可跑当前内置 Ngspice workflow；
- 成功结果可通过 `result.read_*()` 得到 Python result 对象。

## 8. 实施切分

### M6.0 文档侧启动

状态：已完成。

产出：

- 本阶段文档；
- Python workflow binding 专题草案；
- 路线图状态更新；
- M4 / M5 承接关系更新；
- 根 `TODO` 实施计划。

### M6.1 Python API 最小绑定

状态：已完成。

产出：

- 绑定 `Simulation`；
- 绑定 `SimulationResult`；
- 绑定必要枚举或字符串转换；
- 支持空 batch、输入契约和生命周期测试。

### M6.2 Python workflow 读取闭环

状态：已完成。

产出：

- `SimulationResult.read_*()` 返回现有 Python result 对象；
- 补充读取相关 contract test；
- 补充示例脚本和 README。

### M6.3 外部 simulator smoke

状态：部分完成。

产出：

- 新增 `python_workflow_external_smoke`；
- 默认不调用真实 EDA 工具；
- 设置 `SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS=1` 时跑 Ngspice workflow smoke；
- Spectre Python workflow smoke 暂缓。

## 9. 验证事实

Python binding 默认无 libpsf：

```bash
cmake --preset python
cmake --build --preset python
ctest --preset python --output-on-failure
```

```text
100% tests passed, 0 tests failed out of 104
```

Python workflow external smoke（显式开启，当前覆盖 Ngspice）：

```bash
SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS=1 \
  ctest --test-dir build/python -R python_workflow_external_smoke --output-on-failure
```

```text
100% tests passed, 0 tests failed out of 1
```

Python binding + libpsf PIC：

```bash
cmake --preset python-libpsf-pic
cmake --build --preset python-libpsf-pic
ctest --preset python-libpsf-pic --output-on-failure
```

```text
100% tests passed, 0 tests failed out of 123
```

## 10. 完成定义

M6 第一版完成事实：

- Python 用户可以创建 `spiceunion.Simulation`；
- Python 用户可以声明参数；
- Python 用户可以提交 `list[dict[str, float]]`；
- Python 用户可以获得 `list[SimulationResult]`；
- 单点失败通过 `SimulationResult` 表达；
- 成功点可通过 `SimulationResult.read_*()` 读取信号；
- 结果读取返回现有 Python result 对象；
- 普通用户不需要接触 `Evaluator`、`SessionFactory`、`SimulatorSession`、worker directory
  或 `.raw` 查找 helper；
- 默认 Python preset 测试通过；
- Ngspice external workflow smoke 可按门控执行；
- README、当前事实、架构总览、路线图、阶段记录和专题记录完成收口。

## 11. 暂缓项

- numpy 数组返回；
- pytest；
- wheel / package 发布；
- Spectre Python workflow smoke；
- MATLAB binding；
- C ABI；
- optimizer / objective / metric；
- netlist DSL。
