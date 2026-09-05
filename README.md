# SPICEUnion

[![CI](https://github.com/Snappersontheprowl/SPICEUnion/actions/workflows/ci-eda-free.yml/badge.svg)](https://github.com/Snappersontheprowl/SPICEUnion/actions/workflows/ci-eda-free.yml)
[![License: Apache-2.0](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](./LICENSE)

A C++17 **simulator execution and result-reading infrastructure library** for
EDA-style parameter sweeps and design-space exploration. It submits batches of
parameterized simulation tasks to real simulators (Spectre / Ngspice), runs
them concurrently in order-preserving worker pools, and returns structured
results through one unified model.

> 中文版见 [下文](#中文版chinese)。Simplified-Chinese readers can jump to the
> Chinese section below.

## What problem does it solve?

Circuit design, optimization, and parameter-scan projects usually need
hundreds or thousands of simulations: change parameters -> run simulator ->
read results -> repeat. Doing that by hand (shell scripts, ad-hoc parsing) is
slow and brittle. SPICEUnion packages this loop as an embeddable library:

- submit a batch of parameter states (`ParameterState`) or user cases
  (`SimulationCase`);
- the library starts / reuses real simulators, executes concurrently, and
  returns results **in input order**;
- each task's `.raw` outputs stay in its own worker directory, and a unified
  reader turns them into typed results;
- a single task failure, timeout, or simulator crash does not poison the rest
  of the batch.

User-facing main path:

```text
Simulation
  -> declare parameters
  -> run SimulationCase batch
  -> SimulationResult
  -> read signals
```

Internal execution path:

```text
ParameterState batch
  -> Evaluator
  -> ordered worker pool
  -> SimulatorSession
  -> worker work directory
  -> caller-owned result reading
  -> ordered TaskResult list
```

## Feature highlights

**Execution**

- user workflow facade (`Simulation` / `SimulationResult`) that hides worker
  and result-directory plumbing;
- `Evaluator` batch facade with per-task failure isolation and ordered return;
- Spectre interactive backend (SKILL handshake, parameter injection,
  `(sclRun "all")`, completion detection);
- Ngspice batch backend with built-in RC AC / RC TRAN / resistor-divider DC
  tasks.
- simulator discovery and diagnostics: `find_simulator()` honors
  `SPICEUNION_SPECTRE` / `SPICEUNION_NGSPICE`; a manual `spiceunion doctor`
  reports which simulators are available.

**Results**

- one unified ResultIR: `ScalarResult`, `DcSweep`, `AcResponse`,
  `AcDerivedView`, `TranWaveform`;
- PSF result directory discovery and reading driven by an execution-declared
  `ResultFormat`; PSFASCII is parsed by the built-in parser, BINPSF by the
  optional libpsf backend, and content sniffing is only a fallback for unknown
  formats;
- Ngspice `wrdata` AC / TRAN / DC sweep parsing;
- AC math helpers (magnitude/phase, UGBW, phase margin) and transient
  settling-time helper.

**Language bindings**

- C++17 public API under `include/su/`;
- optional pybind11 Python bindings (workflow + result helpers + result
  types), module name `spiceunion`;
- C ABI: draft / deferred.

## Repository layout

```text
include/su/      public C++ API
src/workflow/    user workflow facade implementation
src/core/        evaluator and shared execution types
src/pool/        ordered worker-pool adapter
src/session/     Spectre / Ngspice session backends
src/parse/       ResultIR helpers and optional libpsf backend
src/toolchain/   simulator discovery / version probing
bindings/python/ optional pybind11 bindings
tests/           GoogleTest suites and fixtures
doc/             end-user guide + internal development docs (Simplified Chinese mostly)
local/           local run outputs / external build products (not versioned)
build/           CMake build products (not versioned)
third_party/     vendored build recipes for third-party dependencies
packaging/       wheel / conda-forge release materials
.github/         GitHub Actions pipelines (cloud CI + self-hosted CI)
```

## Quick start

### Prerequisites

- a C++17 compiler;
- CMake >= 3.20;
- git;
- [OrderedConcurrentPool](https://github.com/Snappersontheprowl/OrderedConcurrentPool)
  cloned as a sibling of this repository (MIT).

The default build needs **no EDA tools, no license, and no PDK**.

### Build and run the default test suite

```bash
git clone https://github.com/Snappersontheprowl/SPICEUnion.git
git clone https://github.com/Snappersontheprowl/OrderedConcurrentPool.git
cd SPICEUnion
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

If the pool repository lives elsewhere, pass
`-DSPICEUNION_ORDERED_POOL_SOURCE_DIR=/path/to/OrderedConcurrentPool`.

### Optional presets

| Preset | Purpose |
|---|---|
| `default` | default tests, no external EDA tools |
| `external` | real Spectre / Ngspice external tests |
| `libpsf` | enable the libpsf PSF result backend |
| `external-libpsf` | real-netlist end-to-end: external sim + libpsf parsing |
| `python` | pybind11 Python bindings |
| `python-libpsf-pic` | Python bindings + libpsf (PIC static link) |

External-simulation presets are **opt-in**: they require your own licensed
simulator and private netlist / PDK materials. No proprietary materials are
bundled or referenced by this repository.

### Python quick check (after building the `python` preset)

```bash
PYTHONPATH=build/python/bindings/python python3 -c "import spiceunion; print(spiceunion.version())"
```

More examples live in `bindings/python/examples/`.

### Python users: 3-minute start

No C++/CMake needed. From a fresh environment:

```bash
# install the library only (a simulator is never installed by SPICEUnion)
pip install spiceunion
# or install the latest development build straight from Git:
# pip install git+https://github.com/Snappersontheprowl/SPICEUnion.git

# check what this machine can run
spiceunion doctor
```

```python
import spiceunion as su

with su.Simulation(netlist_path="input.scs", simulator="spectre", workers=4) as sim:
    sim.add_parameter("wp")
    results = sim.run([{"wp": 14e-6}])
    if results[0].ok():
        ac = results[0].read_ac("out")
```

Reading existing result files needs no simulator at all; see
`bindings/python/examples/`. Real simulation additionally requires your own
ngspice / Spectre installation — `spiceunion doctor` tells you if it is
missing and how to provide it.

## Documentation

Most project documentation is Simplified Chinese; the doc map and conventions
start at `doc/develop_doc/README.md`:

- `doc/usage/README.md` — end-user guide: install, read results, run simulations,
  and failure semantics (Simplified Chinese);
- `00_项目总览/01_当前事实状态.md` — current capability, verification results,
  and boundary ledger (the single source of truth);
- `00_项目总览/02_架构总览.md` — layered architecture and execution/reading
  pipelines;
- `00_项目总览/03_开发路线图.md` — what is next;
- `10_阶段记录/` — per-milestone design background (M1–M6);
- `20_专题记录/` — cross-milestone design topics.
- `doc/develop_doc/DEVELOP_GUIDE.md` — how documentation should participate in a
  feature development (Simplified Chinese).

## Boundaries and known limitations

- not a circuit metric/objective/penalty engine, optimizer, PDK manager, GUI,
  or full netlist IR;
- SPICEUnion does **not** install, download, or manage real simulators
  (Spectre / Ngspice). It only detects executables already present on the
  machine (`SPICEUNION_SPECTRE` / `SPICEUNION_NGSPICE` or PATH) and calls them;
  when missing, `spiceunion doctor` tells the user how to provide one
  themselves.
- parsing boundaries verified against real netlists: PSFXL transient returns
  `unsupported_format`; PSFASCII is supported by the built-in parser; BINPSF is
  supported through optional libpsf; legacy sensitivity and a fully native
  BINPSF parser are not implemented;
- Python bindings currently cover workflow + result reading; starting
  simulations from Python is supported through `Simulation`;
- the first PyPI release (`spiceunion` 0.1.0, default wheel without libpsf) is
  available; performance numbers are not yet systematically measured.

## Contributing

Contributions are welcome. Please start with
[`CONTRIBUTING.md`](./CONTRIBUTING.md), which points to:

- code and collaboration conventions in `AGENTS.md`;
- the documentation map and the single source of truth for facts;
- the local verification gate: `scripts/verify_all_presets.sh`;
- cloud CI and self-hosted CI pipelines under `.github/workflows/`.

By contributing, you agree that your contributions are licensed under
Apache-2.0.

## License

SPICEUnion itself is licensed under
[Apache-2.0](./LICENSE). Third-party components are used under their own
licenses:

| Component | Use | License |
|---|---|---|
| [henjo/libpsf](https://github.com/henjo/libpsf) | optional BINPSF result backend | LGPL-3.0 (built from source; not distributed with this repo) |
| pybind11 | optional Python bindings | MIT (fetched at build time) |
| GoogleTest | test framework | BSD-3-Clause (fetched at build time) |
| OrderedConcurrentPool | sibling dependency | MIT (see its `LICENSE`) |

`third_party/libpsf/CMakeLists.txt` is this project's own CMake recipe for the
upstream libpsf and is released under Apache-2.0.

---

# 中文版（Chinese）

## 项目是什么

SPICEUnion 是一个 C++17 的**仿真器执行与结果读取基础设施库**：把一批参数化仿真
任务交给真实仿真器（Spectre / Ngspice）批量执行，并把结果统一读取成结构化数据，
供上层算法、优化器或工具链直接使用。它把“改参数 → 跑仿真 → 读结果”的重复劳动
抽象为可嵌入的库，支持并发执行、按输入顺序返回结果、单任务失败隔离。

普通用户主链路：`Simulation` → 声明参数 → 提交 `SimulationCase` batch →
`SimulationResult` → 读取信号。

## 核心能力

- **执行层**：用户工作流 facade（`Simulation`/`SimulationResult`）；
  `Evaluator` batch facade；Spectre interactive backend（SKILL handshake、
  参数写入、`(sclRun "all")`、完成判定）；Ngspice batch backend；
  工具链探测（`SPICEUNION_SPECTRE` / `SPICEUNION_NGSPICE` 自动发现 + 版本解析）。
- **结果层**：统一 ResultIR；产物格式由执行层声明交付（`ResultFormat`），
  PSFASCII 内置解析、BINPSF 走可选 libpsf；AC 数学 helper 与 settling time。
- **多语言**：C++17 公开 API（`include/su/`）；可选 pybind11 Python 绑定
  （`Simulation` 工作流 + 结果读取）；C ABI 暂缓。

## 快速开始

默认构建**不需要任何 EDA 工具 / license / PDK**。前置：C++17 编译器、CMake
3.20+，以及 sibling 仓库
[OrderedConcurrentPool](https://github.com/Snappersontheprowl/OrderedConcurrentPool)
（MIT）。

```bash
git clone https://github.com/Snappersontheprowl/SPICEUnion.git
git clone https://github.com/Snappersontheprowl/OrderedConcurrentPool.git
cd SPICEUnion
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

预设一览：`default`（无外部 EDA）、`external`（真实外部仿真）、`libpsf`、
`external-libpsf`（真实网表端到端）、`python`、`python-libpsf-pic`。真实仿真
预设需要你自己的 simulator 许可与私有网表 / PDK 材料，**仓库不捆绑也不引用任何
私有材料**。

## 文档入口

- `doc/usage/README.md`：使用者指引（安装、读结果、跑仿真、doctor 与边界）；
- `doc/develop_doc/README.md`：开发文档地图与维护规范；
- `doc/develop_doc/DEVELOP_GUIDE.md`：一次功能开发的文档参与流程使用指导；
- `00_项目总览/01_当前事实状态.md`：当前能力、验证数字与边界总账（事实唯一来源）；
- `00_项目总览/02_架构总览.md`：分层架构与执行/读取链路；
- `00_项目总览/03_开发路线图.md`：下一步施工；
- `10_阶段记录/`：M1–M6 阶段背景；`20_专题记录/`：跨阶段专题。

## 边界与不足

- 不做 metric/objective/penalty、optimizer、PDK 内容管理、GUI、完整 netlist IR；
- SPICEUnion **不负责安装/下载/管理真实仿真器**（Spectre / Ngspice）：只探测机器上
  已存在的可执行文件（`SPICEUNION_SPECTRE` / `SPICEUNION_NGSPICE` 或 PATH）并调用；
  缺失时 `spiceunion doctor` 提示用户自行安装；
- 解析边界（真实网表实测）：PSFXL transient 明确返回 `unsupported_format`；
  PSFASCII 内置支持、BINPSF 走可选 libpsf；legacy sensitivity 与完整原生
  BINPSF parser 未实现；
- Python 已支持第一版 workflow binding；真实 simulator smoke 需显式开启；
- 首版 PyPI 已发布（`spiceunion` 0.1.0，默认 wheel 不含 libpsf）；性能数字未
  系统实测。

## 参与贡献

欢迎任何形式的参与，请先读 [`CONTRIBUTING.md`](./CONTRIBUTING.md)：里面有协作
约定（`AGENTS.md`）、文档地图、本地门禁 `scripts/verify_all_presets.sh` 与 CI
说明。提交即表示你的贡献按 Apache-2.0 授权。

## 许可证

本体采用 Apache-2.0（见 [LICENSE](./LICENSE)）。第三方组件按各自许可使用：
libpsf（LGPL-3.0，构建期以源形式获取）、pybind11（MIT）、GoogleTest
（BSD-3-Clause）、OrderedConcurrentPool（MIT）。`third_party/libpsf/` 下的
CMake 配方是本项目为上游 libpsf 编写的构建配方，随本项目以 Apache-2.0 发布。
