# 贡献指南（Contributing）

欢迎参与 SPICEUnion。本仓库面向两类贡献：**功能与代码**（执行层、结果层、
Python 绑定、测试）和**文档与示例**（README、开发文档、fixture 说明）。

## 从哪里开始

- 先读根 [README.md](./README.md) 了解项目定位与快速开始；
- 读 `doc/develop_doc/README.md` 了解文档分层和阅读顺序；
- 当前事实与边界以 `doc/develop_doc/00_项目总览/01_当前事实状态.md` 为唯一
  事实来源；架构看 `02_架构总览.md`，下一步施工看 `03_开发路线图.md` 与根
  `TODO`；
- 想找切入点：GitHub Issues（可加 `good first issue` label）、根 `TODO` 的
  暂缓池中标注可认领的项，或从“边界与不足”里挑一条验证/补齐。

## 协作约定（代码侧请遵守 AGENTS.md）

- 命名使用稳定业务语义，不用 `new`、`tmp`、`final`、`v2` 这类阶段性词；
- 大改 / 重构先写进根 `TODO` 分步方案再实施，完成后同步收口；
- 涉及行为、结构、入口、配置、流程变化时，同步更新对应 README 与开发文档；
- 所有 README 尽量中文；必要的技术名词与 API 名保留英文；
- 不做“兼容性妥协”，只允许成功或回滚。

## 本地开发与验证

默认预设不依赖任何 EDA 工具：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

提交前请尽量跑一键门禁：

```bash
scripts/verify_all_presets.sh
```

说明：

- `external` / `external-libpsf` 预设需要你自己的 Spectre / Ngspice 许可与
  私有网表 / PDK 材料，**不是必须的**；仓库不捆绑也不引用任何私有材料；
- 验证数字等事实统一维护在 `00_项目总览/01_当前事实状态.md`，不要散写进
  README；
- 新增 fixture 前先读 `tests/fixtures/README.md` 的约束（小体积、可重复、
  记录来源与期望值，不提交 PDK 正文）。

## CI/CD 地图

仓库的自动化分三层：云 CI（无需 EDA）、自托管 CI（真实仿真）、发布流水线。

| 流水线 | 触发 | 跑什么 | 本地等价 |
|---|---|---|---|
| `ci-eda-free` · build-test | push / PR / 手动 | ubuntu-latest 上矩阵跑 `default` / `python` / `libpsf` / `python-libpsf-pic`（libpsf 现场构建 + ccache + JUnit artifact） | `scripts/verify_all_presets.sh`（不含 external） |
| `ci-eda-free` · wheel-smoke | push / PR / 手动 | 单仓 `pip install .` + `import spiceunion` + `spiceunion doctor` 冒烟 | 干净 venv 中 `pip install .` |
| `ci-external` | 仅手动 / 定时（周五 22:00 UTC） | 自托管 runner（label `eda`）跑 `external-libpsf`：真实 Spectre / Ngspice + libpsf | `cmake --preset external-libpsf` 三连（需自有许可/材料） |
| `publish-testpypi` | 手动 | 构建 sdist + manylinux wheel 上传 TestPyPI | 无（近似：`pip install .` 冒烟） |
| `publish-pypi` | 打 `v*` tag | 构建并发布到 PyPI（trusted publishing） | 先用 `publish-testpypi` 试跑 |

要点：

- PR 只能触发**无 EDA 的云 CI**；`ci-external` 永不接 PR / fork 代码，真实仿真只在
  自托管 runner 上手动或定时执行（安全取舍）；
- 想跳过本次 push 的 CI：把 `[skip ci]` 写进 commit message（例如
  `git commit -m "docs: ... [skip ci]"`），本次 push 不会触发任何 workflow；
- 仓库配置只引用不写值：`ORDERED_POOL_REPOSITORY`（Variables）、
  `SPECTRE_MATERIALS_DIR` / `LIBPSF_INCLUDE_DIR` / `LIBPSF_LIBRARY`（Secrets，
  仅 `ci-external` 用）；
- fork PR 暂不跑云 CI（`require-config` 依赖仓库 Variables），PR 请从仓库内分支发起；
- 发版流程：改版本三处（`CMakeLists.txt`、`src/core/version.cpp`、
  `pyproject.toml`）+ `CHANGELOG.md` → 先 `publish-testpypi` 试跑 → 打 `vX.Y.Z`
  tag 触发 `publish-pypi`；
- CI 学习与运维细节（runner、SELinux、踩坑）是本地私有材料（`doc/study_notes/cicd/`），
  不入公开仓库。

## 提 Issue / PR

- Issue：先说明“现象 / 复现条件 / 期望”，能附上最小示例最好；边界类问题可以
  引用 `01_当前事实状态.md` 的对应条目；
- PR：小步提交、一条 PR 一个主题；commit message 中英文均可但要能说明意图；
  改动前若与既有设计冲突，先讨论再动手。

## 许可

本仓库采用 Apache-2.0。提交即表示你的贡献按 Apache-2.0 授权；第三方材料与
上游许可边界见根 README「许可证」一节。
