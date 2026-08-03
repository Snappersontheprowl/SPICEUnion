# src

本目录存放实现文件。

## 目录结构

- `core/`：面向 evaluator 的核心类型与编排实现。
- `pool/`：有序 worker 调度与 session pool 实现。
- `session/`：simulator session 实现与 protocol helper。
- `parse/`：结果读取与辅助计算。

## 命名规则

- 目录名描述职责。
- 条件允许时，实现文件应与公开头文件职责保持对应。
