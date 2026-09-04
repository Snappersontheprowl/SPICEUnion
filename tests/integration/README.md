# tests/integration

本目录存放默认可运行的跨模块集成测试。

## 使用边界

- 可以读取 `tests/fixtures/` 中已提交的小型固定样本。
- 不启动真实外部 EDA 工具，不依赖私有材料目录。
- 重点验证跨 backend 结果语义、ResultIR 一致性和模块协作契约。

## 子目录

- `semantics/`：使用公共语义 helper 验证 AC、TRAN、DC 等同类电路结果是否满足
  一致物理语义。
