# SPICEUnion 使用指引

本文面向**想直接使用 SPICEUnion 的人**（Python 或 C++ 嵌入用户），不是内部开发
文档。开发/贡献指引见根 `CONTRIBUTING.md` 与 `doc/develop_doc/`。

## 0. 一句话

SPICEUnion 帮你把“参数化仿真 + 读结果”写成几行代码：批量提交参数 → 调用真实
仿真器（Spectre / Ngspice）→ 把结果读成结构化对象。库本身不安装仿真器，只探测
机器上已有的仿真器并调用。

## 1. 安装

```bash
pip install spiceunion
```

安装后先体检本机有没有可用的仿真器：

```bash
spiceunion doctor
```

只读已有结果可以跳过仿真器检查。需要真实仿真的话，请自行安装 ngspice（例如
conda-forge 的 `ngspice`）或 Cadence Spectre；`doctor` 会提示缺失项。

## 2. 三种典型用法

### 2.1 读取已有仿真结果（不需要仿真器）

传入产物 `.raw` 目录，读取信号：

```python
import spiceunion as su

result = su.read_dc_value("sim_out.raw", "vout")        # 取 dcOp 标量
sweep  = su.read_dc_sweep("sim_out.raw", "vin_dc", "out")  # DC sweep
ac     = su.read_ac_response("sim_out.raw", "out")         # AC 频响
tran   = su.read_tran_waveform("sim_out.raw", "out", "tran.tran.tran")
```

结果对象统一用 `ok()` / `status_text()` / `message` 表达成败：

```python
if result.ok():
    print(result.value)
else:
    print(result.status_text(), result.message)
```

完整可跑示例见
[`bindings/python/examples/read_fixture_results.py`](../../bindings/python/examples/read_fixture_results.py)。

### 2.2 发起一批仿真（需要真实仿真器）

```python
import spiceunion as su

with su.Simulation(
    netlist_path="input.scs",
    simulator="spectre",     # 或 "ngspice"
    workers=4,
) as simulation:
    simulation.add_parameter("wp", default_value=14e-6)
    simulation.add_parameter("wn", default_value=10e-6)

    results = simulation.run([{"wp": 14e-6, "wn": 10e-6},
                              {"wp": 16e-6, "wn": 11e-6}])

    for r in results:
        if r.ok():
            ac = r.read_ac("out")      # read_dc / read_dc_sweep / read_tran
        else:
            print(r.status_text(), r.message)
```

Ngspice 还内置 RC AC / RC TRAN / 电阻分压 DC 任务，适合不想准备网表的快速体验，
详见 workflow 相关示例与代码。

### 2.3 C++ 嵌入

公开 API 在 `include/su/` 下，最小示例见根 `README.md`；各头文件职责见
[`include/su/README.md`](../../include/su/README.md)。

## 3. 探测与环境变量

- `spiceunion.doctor()` / `spiceunion doctor`：报告 Spectre / Ngspice 探测结果；
- `SPICEUNION_SPECTRE` / `SPICEUNION_NGSPICE`：显式指定仿真器可执行文件；
- 未设置时按 `PATH` 查找（ngspice 也认 `ngspice_con`）。

## 4. 失败语义与边界

- 读取失败不抛异常，返回结果对象并给出 `status_text()`；
- BINPSF 读取需要带 libpsf 的构建（默认 PyPI wheel 不含）：当前返回
  `unsupported_format`；
- Spectre PSFXL transient 当前明确返回 `unsupported_format`；
- 单任务失败/超时/崩溃不会污染同一批其他结果；
- SPICEUnion **不安装/管理真实仿真器**：它只探测与调用。

## 5. 更深入

- Python API 全量契约：`bindings/python/tests/`
- 能力与边界总账（事实）：`doc/develop_doc/00_项目总览/01_当前事实状态.md`
- 想参与开发：根 `CONTRIBUTING.md`
