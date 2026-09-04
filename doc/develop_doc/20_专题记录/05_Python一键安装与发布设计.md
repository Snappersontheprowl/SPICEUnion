# Python 一键安装与发布设计

状态：`active`（设计定稿，代码待启动）
最后更新：`2026-09-04`
适用范围：`Python 打包 / PyPI / conda-forge / doctor 命令行入口`
关联主线文档：

- `../00_项目总览/01_当前事实状态.md`（当前能力与验证数字的唯一事实源）
- `../00_项目总览/03_开发路线图.md`（本专项的施工入口与完成定义）
- `../10_阶段记录/04_M4_语言绑定.md`（pybind11 绑定边界）
- `04_仿真器自动适配与诊断设计.md`（doctor / 探测基础设施）

## 0. 一句话目标

让一个只习惯 Python 的新用户，能用一条 `pip install spiceunion`（或
`conda install -c conda-forge spiceunion`）装好库并 import 使用；需要真实仿真时，
`python -m spiceunion.doctor` 告诉他仿真器在不在、缺哪个、怎么装。

## 1. 用户故事与目标体验

读结果型用户（无仿真器）：

```bash
pip install spiceunion
python -c "import spiceunion; print(spiceunion.version())"
python read_result.py   # 解析已有 PSF / wrdata，不需要任何 EDA 工具
```

发起仿真型用户（ngspice，conda 路线可全自动）：

```bash
conda install -c conda-forge spiceunion ngspice
python demo.py           # Simulation → 批量 → 读结果
```

发起仿真型用户（Spectre，商业软件无法随包分发）：

```bash
pip install spiceunion
python -m spiceunion.doctor   # 自动发现 /opt/.../spectre 或提示手动安装
```

## 2. 为什么现在做不到

不是技术障碍，而是**打包链路尚未建设**：

- 项目没有 `pyproject.toml` / wheel，pip 找不到包；
- 构建强制 sibling `OrderedConcurrentPool`，`pip install` 无法假设用户先 clone 依赖；
- 没有 CI 出 wheel、没有 conda-forge recipe，`conda install` 无从谈起；
- 没有 Python 侧 doctor 入口，仿真器缺失只能靠用户自己猜。

## 3. 代码与构建前置改造

### 3.1 单仓可构建（P-a 前提）

OrderedConcurrentPool 改为“sibling 优先，缺失时 CMake `FetchContent` 自动拉取”，
使 sdist / wheel 构建与用户环境都不需要第二个仓库。行为边界：

- 本地开发仍可用 sibling（路径覆盖语义不变）；
- 打包与 CI 用 FetchContent 回退，URL 锁定 tag/commit；
- 不改动 OrderedConcurrentPool 上游。

### 3.2 打包配置（P-a）

- `pyproject.toml` + `scikit-build-core`：声明 CMake 构建、wheel 内输出
  `spiceunion` Python 模块；
- 元数据：名称 `spiceunion`、Apache-2.0、Python 版本支持面；
- `python -m spiceunion` 或 console script 暴露 `doctor` 入口；
- 保持“默认 wheel 不依赖 libpsf / 不引入 numpy”。

### 3.3 Python doctor（P-b）

把 toolchain 探测绑定为 Python 入口：

```text
spiceunion.doctor()
  -> [spectre] found / missing + suggestion
  -> [ngspice] found / missing + suggestion
```

复用 `include/su/toolchain.hpp` 与 `SPICEUNION_SPECTRE` /
`SPICEUNION_NGSPICE` 语义，不重复实现探测。

## 4. 分发边界（先讲清“不做什么”）

- **默认 wheel 不捆绑 libpsf**（LGPL-3.0）：BINPSF 读取返回
  `unsupported_format`，消息里说明可用 libpsf 预设自行构建；后续可评估
  `spiceunion[libpsf]` extra 或独立 wheel，但许可证边界要单独成文；
- **Spectre 不可分发**：商业工具，doctor 负责发现与提示；
- **ngspice 可分发但不在 PyPI**：走 conda-forge 的 `ngspice` 包；
- wheel 仅保证“库可装可导入”，真实仿真能力仍由外部工具决定。

边界声明：SPICEUnion **不负责安装/下载/管理真实仿真器**。`pip` / `conda` 安装的是
`spiceunion` 库本身；doctor 只做检测与“用户自行安装”的提示，不替用户安装
ngspice / Spectre。

## 5. 分阶段实施与完成定义

### P-a：打包地基

- [x] OCP FetchContent 回退生效，单仓 `pip install .` 可构建；
- [x] CMake 安装规则输出 `spiceunion` Python 模块；
- [x] 本机干净 venv 中 `pip install .` + `import spiceunion` 通过。

验证（2026-09-04）：`/tmp` 下无 sibling 的干净源码 + 全新 venv，
`pip install .` 自动拉取 OCP 并产出 `spiceunion-0.1.0-cp310-*.whl`，
`import spiceunion` / `Simulation(...)` / `version()` 正常；默认不含 libpsf。

### P-b：一键体验

- [ ] Python `doctor()` / `python -m spiceunion` 输出仿真器报告；
- [ ] CI（云 runner）构建 wheel 并做 import + doctor 冒烟；
- [ ] 根 README 增加“Python 用户 3 分钟上手”小节。

### P-c：发布

- [ ] 首版 wheel 发布到 PyPI（默认不含 libpsf）；
- [ ] conda-forge recipe 候选（`spiceunion` + 依赖说明）；
- [ ] 版本号与升级策略，回归走现有 preset 矩阵。

## 6. 验证矩阵

| 场景 | 命令 | 预期 |
|---|---|---|
| 全新 venv，读结果 | `pip install .` 后 import + 读 fixture | 成功 |
| 全新环境，ngspice 仿真（conda） | `conda install -c conda-forge spiceunion ngspice` | doctor 显示 ngspice found，样例可跑 |
| Spectre 场景 | `python -m spiceunion.doctor` | 报告 found / missing 与建议 |
| 回归 | `verify_all_presets.sh` | 全绿，默认 wheel 不破坏现有测试 |

## 7. 承接关系

- 依赖 `04_仿真器自动适配与诊断` 的 toolchain 探测与 doctor 报告语义；
- 不改变 C++ 用户路径与现有预设；Python 安装只是“多了官方分发方式”；
- 许可证边界（libpsf LGPL）涉及后续专项时单独立项，不在 P-a 默认范围。
