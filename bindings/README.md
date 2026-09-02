# bindings

本目录存放语言绑定层。

## 本级模块职责

- `python/`：可选 pybind11 Python binding，当前绑定用户 workflow、结果读取 helper 与
  结果类型。

## 当前约定

- Python workflow binding 已完成第一版，用户入口为 `Simulation` / `SimulationResult`，
  不直接暴露底层 session。
- MATLAB MEX 或 JSON/CLI bridge 暂缓。
