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

读取 `spectre_materials` 历史输出样本：

```bash
local/external/libpsf/libpsf_probe \
  ~/my_lab/projects/spectre_materials/local/runtime/sim_result/input_C11/input_C11.raw/stb.stb \
  loopGain
```
