# tests

本目录存放基于 GoogleTest 的测试。

## 测试分组

- Smoke tests 验证构建与链接路径。
- Contract tests 使用 fake 或 scripted session 验证执行层行为。
- Result tests 验证 M2 ResultIR、读取状态和公开 result_reader API 骨架。
- Result reader tests 验证 `.raw` 目录定位、AC 数学 helper、settling time 和
  M2.3 文件读取行为。默认构建下真实 PSF 读取保持禁用；启用 libpsf backend 后额外
  验证 `dcOp.dc` scalar 读取。
- Manual tests 位于 `tests/manual/`，只用于人工 spike，不进入默认 `ctest`。
- 外部测试可能依赖 Spectre 或 PDK 访问权限，默认必须禁用，除非显式启用。

## 命令

默认测试：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

外部 Spectre 生命周期测试：

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external
```

本机 libpsf reader 测试：

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

该测试默认尝试使用 `local/external/libpsf/install` 中的 `henjo/libpsf`，不属于默认
CI 契约。

外部测试当前覆盖：

- Spectre interactive handshake 与 stop；
- single-session `(sclRun "all")`；
- 默认 Spectre evaluator multi-worker batch。
