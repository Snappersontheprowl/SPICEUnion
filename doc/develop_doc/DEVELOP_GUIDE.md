# 开发文档使用指导（DEVELOP_GUIDE）

## 这份文档是什么

面向要动 SPICEUnion 代码或文档的开发者：开始一次功能开发前先读一遍，开发中与
开发后按对应小节对照执行。

与同目录 `README.md` 的分工：

- `README.md`：开发文档的**地图与规则速查**（分层、事实归属、命名、更新规则）；
- `DEVELOP_GUIDE.md`（本文）：**按一次功能开发走完文档参与流程**的操作指导。

## 总体流程

如果按现在这套开发文档分层，一次功能开发在“文档侧”的参与流程应该是：

```text
开发前：定方向、定边界、定完成定义
开发中：让文档约束实现，不让实现乱长
开发后：把已完成事实收口到唯一事实源，清理计划和草案
```

它不是“写完代码后补文档”，而是**文档从功能进入项目那一刻就参与治理**。

下面我按一次真实功能开发来讲，比如后续要做“统一用户工作流 API”。

## 开发前

### 0. 先判断这是不是大改
开发前先判断功能性质。

小改：

```text
修一个 bug
补一个测试
改一个错误说明
调整一条 README 入口
```

这类通常不需要新建专题草案，也不一定需要根 `TODO` 分步骤方案。

大改 / 重构级变更：

```text
新增一层 public API
改变用户主入口
改变目录结构
改变执行链路
改变结果读取语义
改变语言绑定边界
```

这类必须先进入根 [TODO](../../TODO:1)，写清楚分步骤方案。

比如“统一用户工作流 API”显然是大改，因为它会影响：

```text
include/su/
src/
tests/
README.md
当前事实状态
架构总览
开发路线图
Python binding 后续方向
```

所以它必须先有计划。

### 1. 开发前：从路线图认领任务
第一步看 [03_开发路线图.md](./00_项目总览/03_开发路线图.md:1)。

路线图回答：

```text
这个方向是否应该做？
为什么做？
进入条件是什么？
完成定义是什么？
暂缓条件是什么？
```

比如现在路线图里已经有 [统一用户工作流 API](./00_项目总览/03_开发路线图.md:25)，它定义了目标：

```text
用户提供合法网表
  -> 用户声明可变参数
  -> 用户提交参数组合并启动仿真
  -> 用户基于结果对象读取信号
  -> 用户搭建上层指标提取
```

它也明确了不暴露普通用户主路径的内部细节：

```text
并行
backend class
worker directory
.raw
PSF / wrdata 解析细节
```

这一步的文档作用是：**防止还没开始写代码，功能边界就漂了。**

如果路线图里没有这个方向，就先更新路线图或新增草案，而不是直接开写。

### 2. 开发前：根 TODO 写实施计划
如果是大改，下一步写根 [TODO](../../TODO:1)。

TODO 不写长篇解释，只写可执行分步骤计划。例如：

```md
## 当前活跃待办

- [ ] 统一用户工作流 API：
  - [ ] 新增 `include/su/workflow.hpp`，定义 `SimulationOptions`、`Simulation`、`SimulationResult`。
  - [ ] 新增 `src/workflow/`，内部复用 `Evaluator` 与 `result_reader`。
  - [ ] 新增 `tests/workflow_test.cpp`，覆盖参数校验、保序返回、结果读取封装。
  - [ ] 更新 `CMakeLists.txt` 纳入 workflow 实现与测试。
  - [ ] 更新根 README 主示例。
  - [ ] 更新 `00_项目总览/01_当前事实状态.md`。
  - [ ] 更新 `00_项目总览/02_架构总览.md`。
  - [ ] 更新 `include/su/README.md`、`src/README.md`、`tests/README.md`。
  - [ ] 默认构建与测试通过后清理 TODO 并提交。
```

这一步的文档作用是：**把开发拆成可检查的施工清单。**

注意 TODO 不是当前事实，它只是施工现场的白板。功能完成后要清掉活跃项。

### 3. 开发前：必要时写专题草案
如果功能有明显设计不确定性，就在 [20_专题记录](./20_专题记录/README.md:1) 新增草案。

例如：

```text
doc/develop_doc/20_专题记录/02_用户工作流API设计.md
```

草案顶部写：

```md
# 用户工作流 API 设计

状态：`draft`
最后验证：`2026-09-02`
适用范围：`C++ workflow facade / Python binding / future MATLAB bridge`
单一事实来源：

- `include/su/workflow.hpp`（落地后）
- `src/workflow/`
- `tests/workflow_test.cpp`
```

草案只写这些：

```text
要解决的问题
不解决的问题
用户心智模型
C++ API 草案
Python API 草案
内部映射
失败语义
完成定义
```

草案不写这些：

```text
已经实现
已经验证
当前支持
测试数字
```

这些要等代码落地后才写进 [01_当前事实状态.md](./00_项目总览/01_当前事实状态.md:1)。

这一步的文档作用是：**先把设计想清楚，但不污染当前事实。**

如果功能很小，草案可以省略。

## 开发中

### 4. 开发中：文档约束实现
实现时，文档不是旁观者，而是约束。

比如路线图说第一版 workflow API 不做：

```text
optimizer
netlist DSL
业务 metric 系统
直接暴露底层 session binding
```

那开发中看到“顺手加一个 metric callback”这种诱惑，就应该挡住。  
因为文档已经定义了边界。

如果开发中发现原计划不合理，要回到文档改计划：

```text
TODO 改施工步骤
专题草案改设计选择
路线图改进入条件或完成定义
```

但不能让代码悄悄越界，然后文档事后被迫追认。

这一步的文档作用是：**让文档参与范围控制。**

### 5. 开发中：目录 README 随结构变化同步
如果新增目录，比如：

```text
src/workflow/
```

就必须新增或更新：

```text
src/README.md
src/workflow/README.md
```

如果新增公开头：

```text
include/su/workflow.hpp
```

就必须更新：

```text
include/su/README.md
```

如果新增测试：

```text
tests/workflow_test.cpp
```

就必须更新：

```text
tests/README.md
```

目录 README 只说明目录职责、文件分工、命名规则和长期约定。不要把功能设计细节全塞进去。

比如 `src/workflow/README.md` 应该写：

```md
# workflow

本目录实现普通用户工作流 facade。

## 本级模块职责

- `simulation.cpp`：实现 `Simulation` 与 `SimulationResult`，内部复用 evaluator 和 result_reader。

## 当前约定

- 本目录不实现 optimizer、metric、netlist DSL。
- 本目录不直接暴露 simulator process/protocol 细节。
```

这一步的文档作用是：**让目录结构自解释，避免新人靠猜。**

## 开发后

### 6. 开发后：事实进入当前事实状态
功能完成、测试通过后，更新 [01_当前事实状态.md](./00_项目总览/01_当前事实状态.md:1)。

这里写的是“当前已经实现什么”，例如：

```md
### 用户工作流层

当前已实现：

- `Simulation` C++ facade；
- `SimulationOptions` 支持 simulator、netlist_path、workers、timeout；
- `SimulationResult` 封装 `TaskResult` 与 result_reader；
- 成功任务可直接 `read_dc()`、`read_ac()`、`read_tran()`；
- 普通用户路径不需要直接调用 `find_result_directory()`。

当前未实现：

- Python workflow binding；
- MATLAB binding；
- optimizer；
- netlist DSL；
- 业务 metric 系统。
```

还要写验证结果：

```md
默认测试：

100% tests passed, 0 tests failed out of XX
```

这一步的文档作用是：**把完成内容写进唯一事实源。**

不要让阶段文档、README、路线图各自维护一套“当前支持能力”。

### 7. 开发后：架构变化进入架构总览
如果功能改变架构，就更新 [02_架构总览.md](./00_项目总览/02_架构总览.md:1)。

比如新增 workflow 层后，架构图应该从：

```text
User -> PublicAPI -> Evaluator -> SimulatorPool
```

变成：

```text
User -> Workflow API -> Evaluator -> SimulatorPool
```

并表达：

```text
SimulationResult -> result_reader
```

这里写模块关系，不写测试数字、不写未来计划。

这一步的文档作用是：**保证架构图只表达已落地结构。**

### 8. 开发后：README 主入口更新
如果功能影响用户入口，就更新根 [README.md](../../README.md:1)。

新增 workflow API 后，根 README 的最小示例就应该从底层 `Evaluator` 改成：

```cpp
#include "su/workflow.hpp"

su::SimulationOptions options;
options.simulator = su::SimulatorKind::kSpectre;
options.netlist_path = "input.scs";
options.workers = 4;

su::Simulation simulation(options);
simulation.add_parameter("wp");
simulation.add_parameter("wn");

auto results = simulation.run({
    {{"wp", 14e-6}, {"wn", 10e-6}},
    {{"wp", 16e-6}, {"wn", 11e-6}},
});

auto ac = results[0].read_ac("out");
```

`Evaluator` 可以保留在高级入口里，但不再是普通用户第一屏。

这一步的文档作用是：**让用户文档跟随主入口变化。**

### 9. 开发后：路线图从“计划”变成“状态”
更新 [03_开发路线图.md](./00_项目总览/03_开发路线图.md:1)。

如果统一 workflow API 完成，就把它从“候选下一阶段”移走或改成已完成阶段摘要：

```md
| M5 用户工作流 API | 已完成 | C++ workflow facade，详见 `../20_专题记录/02_用户工作流API设计.md` |
```

然后保留下一个阶段，例如：

```text
Python workflow binding
MATLAB bridge 评估
```

路线图不复制所有实现细节，只指向事实状态和专题文档。

这一步的文档作用是：**防止路线图长期挂着已完成任务。**

### 10. 开发后：专题草案收口
如果前面写过草案，有三种处理方式：

第一种，功能已落地，草案仍有长期设计价值：

```text
状态：draft -> active
```

标题可以继续保留，内容删掉过时争论，只保留最终设计和边界。

第二种，功能已落地，草案内容已经提炼进 L1 文档：

```text
移动到 90_归档备注/
```

第三种，草案没有独立价值：

```text
删除
```

不要让 `draft` 文档永远漂在 `20_专题记录/` 里。草案不收口，文档系统迟早会重新乱起来。

### 11. 开发后：根 TODO 清理
根 [TODO](../../TODO:1) 里活跃项要删除。

完成事项不要长期留在“当前活跃待办”。可以在归档里只留一句：

```md
- 统一用户工作流 API：C++ workflow facade 已落地，主入口见 `include/su/workflow.hpp`，
  验证见 `doc/develop_doc/00_项目总览/01_当前事实状态.md`（2026-xx-xx）。
```

这一步的文档作用是：**让 TODO 永远只表示未完成，而不是墓地。**

### 12. 开发后：验证并提交
最后至少做：

```bash
git diff --check
cmake --build --preset default
ctest --preset default --output-on-failure
```

如果改了 Python binding，再做：

```bash
cmake --preset python
cmake --build --preset python
ctest --preset python --output-on-failure
```

如果改了外部仿真或 libpsf，按路线图里的验证矩阵跑对应 preset。

提交前确认：

```text
旧路径没有残留
TODO 已收口
当前事实状态已更新
架构总览只画已落地内容
README 主入口和实际 API 一致
```

最后本地 commit。

## 一张流程图
完整文档参与流程可以这样记：

```text
需求出现
  -> 查 03_开发路线图：是否属于当前路线
  -> 大改写根 TODO：拆施工步骤
  -> 有不确定性写 20_专题记录 draft
  -> 实现代码和测试
  -> 新目录/新 API 同步目录 README
  -> 测试通过
  -> 01_当前事实状态 收口当前能力和验证结果
  -> 02_架构总览 收口已落地架构变化
  -> 根 README 收口用户入口
  -> 03_开发路线图 移除已完成计划、留下下一步
  -> 草案 active / archive / delete
  -> 清理根 TODO
  -> commit
```

## 不同规模功能的文档参与程度
不是每次都要动所有文档。可以按影响范围判断。

小 bug：

```text
代码 + 测试
必要时更新对应目录 README 或当前事实中的边界
TODO 通常不用动
```

新增 parser 能力：

```text
TODO
代码 + fixture + 测试
01_当前事实状态
02_架构总览，如果解析链路变化
03_开发路线图，如果原来是候选/暂缓项
M2 阶段文档，如果结果层边界改变
README，如果用户可见
```

新增 backend：

```text
TODO
可能需要 20_专题记录草案
代码 + 外部测试 + fixture
01_当前事实状态
02_架构总览
03_开发路线图
M3 阶段文档
README
tests/README.md
```

新增用户主入口：

```text
TODO
20_专题记录草案
代码 + API contract test
README 主示例
01_当前事实状态
02_架构总览
03_开发路线图
include/su/README.md
src/README.md
tests/README.md
可能影响 M4，如果涉及语言绑定
```

## 最重要的纪律
以后每次功能开发，文档侧要守住这几个纪律：

1. **开发前，路线图和 TODO 管计划。**
2. **开发中，草案和目录 README 管边界。**
3. **开发后，当前事实状态管结果。**
4. **架构总览只画已经落地的结构。**
5. **README 只讲用户主入口。**
6. **TODO 完成后必须清理。**
7. **草案完成后必须收口。**

这样文档就不是“代码的附属品”，而是整个项目的控制面板。功能从哪里进入、做到什么程度、完成后算不算当前能力、下一步还剩什么，都有固定位置承接。
