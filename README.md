# SPICEUnion

SPICEUnion 是一个 C++17 仿真器执行与结果读取基础设施库，来源于
`~/my_lab/projects/spectre_materials/src/spectre_interactive/` 中已有 Python 执行路径的
C++ 化沉淀。

核心链路：

```text
ParameterState batch
  -> Evaluator
  -> ordered worker pool
  -> SimulatorSession
  -> worker work directory
  -> caller-owned result reading
  -> ordered TaskResult list
```

## 当前事实

已实现：

- batch execution facade；
- worker 目录隔离；
- 输入顺序保序返回；
- per-task failure isolation；
- Spectre interactive backend；
- Ngspice batch backend；
- 内部 `OrderedConcurrentPool` 通用池核心；
- `SimulatorPool` adapter；
- 最小 ResultIR；
- 可选 libpsf backend；
- Spectre PSF fixture 读取；
- Ngspice `wrdata` AC / TRAN / DC sweep 读取。

当前 ResultIR：

| 类型 | 含义 |
|---|---|
| `ScalarResult` | 单 signal 标量 |
| `DcSweep` | 单 sweep axis、单 signal 实数响应 |
| `AcResponse` | frequency + complex response |
| `AcDerivedView` | AC magnitude / phase 派生视图 |
| `TranWaveform` | time/value 波形 |
| `SensitivityEntry` | legacy sensitivity 原始条目 |

当前真实 backend：

| Backend | 当前能力 |
|---|---|
| Spectre | interactive handshake、参数写入、`(sclRun "all")`、completion 判断、PSF DC/AC/TRAN fixture 读取 |
| Ngspice | batch-mode RC AC、RC TRAN、电阻分压 DC sweep |

开发文档入口：

- `doc/develop_doc/当前事实状态.md`
- `doc/develop_doc/CPP版本开发计划书.md`
- `doc/develop_doc/开发路线图.md`
- `doc/develop_doc/OrderedConcurrentPool开发路线图.md`
- `doc/develop_doc/简历亮点解析.md`

## 当前边界

尚未实现：

- legacy sensitivity 读取；
- Spectre 23.1 PSFXL transient 解析或转换；
- 原生 PSF parser；
- C ABI 稳定化；
- Python / pybind11 binding；
- Ngspice `.op` operating point；
- MOS I-V / gm/Id 多曲线 DC 结果；
- Spectre 与 Ngspice 同类 DC sweep 语义对照；
- `OrderedConcurrentPool` 独立仓库、独立 CMake package 与外部装配；
- Xyce / Hspice backend。

## 仓库结构

```text
include/su/      公开 C++ API
src/core/        evaluator 与通用执行逻辑
src/pool/        OrderedConcurrentPool 通用池核心与 SimulatorPool adapter
src/session/     Spectre / Ngspice backend
src/parse/       ResultIR helper 与可选 libpsf backend
tests/           GoogleTest 测试与 fixture
doc/develop_doc/ 当前开发事实状态
doc/study_notes/ 可复用学习笔记
```

## 构建与测试

默认测试不依赖外部 EDA 工具：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

外部测试会真实调用 Spectre / Ngspice：

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external --output-on-failure
```

libpsf backend 需要显式启用：

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

## 外部依赖

外部测试可能需要：

- `spectre` 位于 `PATH` 中；
- `/dev/shm/pdk_cache/toplevel.scs`；
- `~/my_lab/projects/spectre_materials/netlist/AMP/dc/input.scs`；
- `ngspice_con` 或 `ngspice` 位于 `PATH` 中；
- 或通过 `SPICEUNION_NGSPICE` 指向 Ngspice 可执行文件。

libpsf backend 默认查找：

```text
local/external/libpsf/install
```

该目录不进入版本库。
