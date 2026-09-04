# tests/unit

本目录存放默认可运行的 C++ 单元测试与契约测试。

## 使用边界

- 不调用真实 Spectre、Ngspice 或其他外部 EDA 工具。
- 可以使用 fake session、临时目录和仓库内小型 fixture。
- 白盒测试可以在必要时 include `src/...` 内部头，但优先测试公开 API。

## 子目录

- `core/`：核心数据结构、`Evaluator` 契约、构建 smoke。
- `parse/`：结果目录定位、ResultIR 数学 helper、PSFASCII / libpsf reader 行为。
- `pool/`：并发池与 `SimulatorPool` adapter 契约。
- `session/`：不依赖真实工具的 session 协议、Ngspice 配置、netlist 渲染和
  `wrdata` parser。
- `workflow/`：用户工作流 facade 契约。

后续新增工具链自动适配与诊断测试时，默认放入 `toolchain/`。
