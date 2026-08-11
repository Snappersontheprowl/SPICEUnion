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
- 外部 `OrderedConcurrentPool` 项目装配；
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

- `doc/develop_doc/README.md`：开发文档索引与维护规则；
- `doc/develop_doc/当前事实状态.md`：当前事实唯一总账；
- `doc/develop_doc/CPP版本开发计划书.md`：项目章程；
- `doc/develop_doc/开发路线图.md`：后续施工路线；
- `doc/develop_doc/OrderedConcurrentPool开发路线图.md`：OCP 专项；
- `doc/develop_doc/简历亮点解析.md`：对外表达材料。

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
- Xyce / Hspice backend。

## 仓库结构

```text
include/su/      公开 C++ API
src/core/        evaluator 与通用执行逻辑
src/pool/        SimulatorPool adapter
src/session/     Spectre / Ngspice backend
src/parse/       ResultIR helper 与可选 libpsf backend
tests/           GoogleTest 测试与 fixture
doc/develop_doc/ 开发文档：事实、章程、路线、专题、表达
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

默认构建需要 sibling source tree：

```text
~/my_lab/projects/OrderedConcurrentPool
```

当前装配的 `OrderedConcurrentPool` 是 sibling 独立项目，已具备 MIT license、
`CHANGELOG.md`、最小 benchmark、CMake install/export package 和 GitHub Actions
CI，并已将 `main` 与 `v0.1.0` tag 发布到 `origin`。SPICEUnion 默认仍通过
source tree 装配，不复制维护池源码。

可通过 CMake cache 变量覆盖：

```text
SPICEUNION_ORDERED_POOL_SOURCE_DIR
```

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
