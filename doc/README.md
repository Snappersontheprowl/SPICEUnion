# doc

本目录存放 SPICEUnion 文档。

## 读者与目录职责

| 读者 | 入口 | 内容 |
|---|---|---|
| 使用者（Python / C++ 嵌入） | `usage/` | 安装、读取结果、发起仿真、doctor 与边界 |
| 开发者 / 贡献者 | `develop_doc/`、根 `CONTRIBUTING.md` | 当前事实、架构、路线、阶段/专题、文档参与流程 |
| 项目规范维护者 | 根 `AGENTS.md`、`doc/README_GUIDE.md` | 协作约定、README 写作规范 |
| 内部学习 | `cicd/` | CI/CD 学习档案 |
| 本人（私有） | `resume/` | 简历与面试材料（不入库） |

`doc/README_GUIDE.md` 是所有 README 的写作参考；`develop_doc/README.md` 是
开发文档地图与维护规范。

`resume/` 与 `study_notes/` 均为本地私有材料，不入库；如本地保留请勿推送到公开
仓库。

## 规则

- `README.md` 默认使用中文。
- 新增文档先判断读者再选目录：使用说明放 `usage/`，开发文档放 `develop_doc/`，
  不要把内部开发细节写进 `usage/`。
- 开发文档保留当前事实、有效计划和后续路线，不保留阶段流水账。
- 学习笔记只记录可复用工程实践，不记录一次性操作过程。
