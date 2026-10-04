# tests/manual

本目录存放人工验证工具。

## 使用边界

- 本目录下的程序默认不进入 `ctest`。
- 允许依赖外部库、真实 Spectre 输出、PDK 或本机环境。
- 验证结果用于开发决策，不作为默认 CI 契约。
- 若某个人工验证工具成熟到可以稳定自动化，再移动到正式 `tests/` 并接入 CMake。

## 当前工具

- `libpsf_probe.cpp`：用于手动验证 `henjo/libpsf` C++ API 是否能读取指定 PSF 文件。
  它不会进入默认构建，也不会把 `psf.h` 暴露到 SPICEUnion 公开 API。
- `spiceunion_doctor.cpp`：一次性报告本机可探测到的仿真器（Spectre / Ngspice）、
  路径、来源与版本。随启用 `SPICEUNION_BUILD_TESTS` 的构建产出，但不注册为
  `ctest` 用例；实现细节见
  `../../include/su/toolchain.hpp`。
- `spectre_output_dir_probe.cpp`：验证“Spectre 常驻交互会话下，连续 run 的产物
  目录是否可做到每 job 独立”的手动探针（专题 06 方案三的验证实验 A/B）。

## Spectre 产物目录探针

```bash
cmake --build --preset default --target spiceunion_spectre_output_dir_probe
./build/default/tests/spiceunion_spectre_output_dir_probe
# 也可指定自有网表（需含 ac 分析与 save）
./build/default/tests/spiceunion_spectre_output_dir_probe --netlist path/to/input.scs
```

该探针会：启动一个常驻会话 → 连续运行两次 → 打印两次运行后的产物快照与差异。
若第二次运行只是“同名路径被重写”，说明会话下无法直接换目录，需要走实验 B
（SKILL 探查）或快照方案。结论回填到
`doc/develop_doc/20_专题记录/06_产物生命周期与指标提取设计.md` 的「验证结果」。

## spiceunion doctor 本机示例

构建（任意启用测试的 preset 均可）：

```bash
cmake --preset default
cmake --build --preset default --target spiceunion_doctor
./build/default/tests/spiceunion_doctor
```

默认 PATH 场景（本机未显式指定 ngspice 时找到 PATH 中的版本）：

```text
[Spectre]
  found: yes
  executable: /opt/cadence/SPECTRE231/bin/spectre
  source: path
  version number: (unavailable)

[Ngspice]
  found: yes
  executable: /usr/local/bin/ngspice
  source: path
  version text: ngspice compiled from ngspice revision 27
  version number: 27
```

显式指定 conda ngspice-41 场景：

```bash
SPICEUNION_NGSPICE=<conda-env>/bin/ngspice ./build/default/tests/spiceunion_doctor
```

```text
[Ngspice]
  found: yes
  executable: <conda-env>/bin/ngspice
  source: env
  version text: ngspice-41 : Circuit level simulation program
  version number: 41
```

说明：MVP 阶段 Spectre 不做 `--version` 探测（实测 `spectre --version` 会崩溃），
因此只报告“找到/路径/来源”；版本探测策略见
`src/toolchain/simulator_probe.cpp` 注释。

安装 wheel 后也有等价命令：`spiceunion doctor`（或 `python -m spiceunion_cli doctor`）。

## libpsf probe 本机示例

若本机已在 `local/external/libpsf/` 中构建并安装 `henjo/libpsf`，可用：

```bash
g++ -std=c++17 tests/manual/libpsf_probe.cpp \
  -Ilocal/external/libpsf/install/include \
  -Llocal/external/libpsf/install/lib64 \
  -lpsf \
  -o local/external/libpsf/libpsf_probe
```

读取 libpsf 自带样本：

```bash
local/external/libpsf/libpsf_probe \
  local/external/libpsf/src/test/data/dcOp.dc vout
```

读取仓库内真实 Spectre 历史输出 fixture：

```bash
local/external/libpsf/libpsf_probe \
  tests/fixtures/psf/spectre_materials_stb_loop_gain.raw/stb.stb \
  loopGain
```
