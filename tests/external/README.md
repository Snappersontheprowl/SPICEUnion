# tests/external 说明

## 功能

- 存放依赖真实仿真器、私有材料目录或本机工具链环境的外部测试。
- 本目录测试默认不执行：未启用 `SPICEUNION_ENABLE_EXTERNAL_TESTS` 时，用例在测试体
  开头 `GTEST_SKIP()`，不调用真实外部工具。

## 本级模块职责

- `spectre/`：依赖 Cadence Spectre 与 `SPICEUNION_SPECTRE_MATERIALS_DIR` 的生命周期、
  能力矩阵和真实结果解析测试，详见 `spectre/README.md`。
- `ngspice/`：依赖本机 `ngspice` / `ngspice_con` 可执行文件的 session 与 evaluator
  真实执行测试，详见 `ngspice/README.md`。

## 当前约定

- 外部测试 target 会参与默认构建，但在 `SPICEUNION_ENABLE_EXTERNAL_TESTS=OFF` 时由
  测试体 `GTEST_SKIP()`，避免默认配置调用真实工具。
- 外部测试新增前必须先明确 skip 条件、外部依赖、产物目录和是否需要私有材料。
- 外部测试产物统一写入 `<项目根>/local/runtime/<工具>_<场景>/`。

## 常用入口

- 外部测试：

```bash
cmake --preset external && cmake --build --preset external && ctest --preset external --output-on-failure
```

- 真实网表端到端（仿真 + 解析）：

```bash
cmake --preset external-libpsf && cmake --build --preset external-libpsf && ctest --preset external-libpsf --output-on-failure
```
