# tests/external 说明

## 功能

- 存放依赖外部仿真材料（Spectre 基线网表与 PDK）的真实工具测试。
- 本目录测试默认不执行：未启用 `SPICEUNION_ENABLE_EXTERNAL_TESTS` 时，用例在测试体
  开头 `GTEST_SKIP()`，不调用真实 spectre。

## 本级模块职责

- `spectre_session_lifecycle_test.cpp`：外部 Spectre 生命周期契约（interactive
  handshake、单任务 `(sclRun "all")`、multi-worker batch）。
- `spectre_real_result_test.cpp`：真实网表端到端——用真实 spectre 跑
  `AMP/dc/input.scs`，再用 libpsf 解析真实 dcOp.dc，断言 Vos / Idd 与
  `spectre_materials` `amp_dc` profile 校准值一致。需要 `SPICEUNION_ENABLE_LIBPSF_READER`。
- `spectre_external_env.hpp`：外部环境检查与材料路径共享 helper。

## 当前约定

- 基线网表与 PDK 统一来自 `SPICEUNION_SPECTRE_MATERIALS_DIR`（默认
  `~/my_lab/projects/spectre_materials`）的 `external/netlist` 与 `external/pdk`，
  路径由 CMake 编译宏注入，不在本目录硬编码机器路径。
- `spectre_materials/external/netlist` 下的网表均为项目所有者实测的合法仿真网表；
  当前外部测试仅消费 `AMP/dc/input.scs`，其余电路网表可作为后续契约测试材料扩展。
- 基线网表内部以绝对路径 include PDK，跨机器需要等价材料布局。
- 本机 `/dev/shm/pdk_cache` 已废弃，SPICEUnion 不再检查该路径。
- 真实仿真产物写入 `<项目根>/local/runtime/spectre_<场景>/`，不使用内存盘
  （实测 tmpfs 对当前产物规模加速不可测量）。
- 真实网表端到端测试需要 external 与 libpsf 同时开启，使用 `external-libpsf` 预设；
  仅开启 libpsf 时该用例会在测试体开头 `GTEST_SKIP()`。

## 常用入口

- 外部测试：

```bash
cmake --preset external && cmake --build --preset external && ctest --preset external --output-on-failure
```

- 真实网表端到端（仿真 + 解析）：

```bash
cmake --preset external-libpsf && cmake --build --preset external-libpsf && ctest --preset external-libpsf --output-on-failure
```
