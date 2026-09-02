# bindings

本目录存放语言绑定层。

## 本级模块职责

- `python/`：可选 pybind11 Python binding，当前绑定结果读取 helper 与结果类型。

## 当前约定

- Python workflow binding 已完成 M6 开发文档侧工作，后续应优先绑定
  `Simulation` / `SimulationResult`，而不是直接暴露底层 session。
- MATLAB MEX 或 JSON/CLI bridge 暂缓。
