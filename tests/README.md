# tests

本目录存放基于 GoogleTest 的测试。

## 测试分组

- Smoke tests 验证构建与链接路径。
- Contract tests 使用 fake 或 scripted session 验证执行层行为。
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

外部测试当前覆盖：

- Spectre interactive handshake 与 stop；
- single-session `(sclRun "all")`；
- 默认 Spectre evaluator multi-worker batch。
