# develop_doc

本目录存放 SPICEUnion 项目开发文档。

## 文件

- `CPP版本开发计划书.md`：SPICEUnion 的权威开发计划，包含范围、架构、里程碑、
  执行层契约与验收标准。
- `M2结果层职责边界与契约.md`：M2 的 ResultIR、结果读取 helper、失败语义、
  fixture 策略与职责边界。
- `M2-libpsf评估与接入策略.md`：M2 中 libpsf 的可用性、接入边界、CMake
  开关、license 风险与 native parser 决策策略。
- `开发路线图.md`：实现路线图，包含阶段任务、预期文件产出、测试产出、完成定义、
  非目标与建议 commit 边界。
- `简历亮点解析.md`：基于开发计划整理的面试叙事与简历定位。

## 命名规则

- 项目开发文档使用稳定职责名。
- 目录职责说明统一使用 `README.md`。
- 当文档用于描述目录职责时，不使用 `README_GUIDE.md` 这类 guide-only alias。
- 不使用 `new`、`final`、`tmp`、`test2`、`v2` 这类阶段性名称。
- 当某个文档成为某条工作流的权威入口时，需要从根 `README.md` 链接过去。
