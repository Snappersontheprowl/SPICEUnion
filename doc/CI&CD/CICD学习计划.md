# CI/CD 学习计划

状态：`active`
最后更新：`2026-08-23`
适用范围：`GitHub Actions + SPICEUnion 两级流水线`

## 1. 学习目标

用一件真实交付物驱动学习：**给 SPICEUnion 落地两级 CI/CD 流水线**，同时掌握：

- CI/CD 的概念与 GitHub Actions 词汇；
- workflow 语法与第一个可用流水线；
- 工程化能力：矩阵、缓存、artifact、失败诊断；
- （延后）CD：wheel / artifact / tag 发布。

## 2. 起点自检

开始前先回答三问，用于定位起点：

1. 能否不看文档说出 `workflow / job / step / runner / event / action` 各自是什么？
2. 能否说出 `on: push` 与 `on: pull_request` 的触发差异？
3. 能否解释 `actions/checkout@v4` 中 `@v4` 的含义？

全答不上 → 从阶段 0 开始；答出 1-2 题 → 直接进阶段 1，概念边做边补。

## 3. 能力拆解

| # | 能力点 | 性质 |
|---|---|---|
| 1 | CI/CD 概念与 GitHub Actions 词汇 | 核心主线 |
| 2 | workflow 语法与第一个可用流水线 | 核心主线 |
| 3 | 工程化：矩阵、缓存、artifact、日志诊断 | 核心主线 |
| 4 | CD：wheel / artifact 发布、tag 触发 release | 延后，可选 |

## 4. 阶段计划

| 阶段 | 内容 | 建议时长 | 里程碑（交付物） | 验收标准 |
|---|---|---|---|---|
| 0 | 概念地基：CI vs CD vs 本地门禁；词汇表；YAML 基础 | 0.5–1 天 | 5 句总结 + 概念关系图 | 能用自己的话讲清“为什么需要 CI” |
| 1 | 第一个 workflow：checkout、OrderedConcurrentPool 依赖装配、三命令跑绿 | 1–2 天 | `.github/workflows/ci-default.yml` 在真实 repo push 后跑绿 | 故意改坏一个参数，能读日志定位 |
| 2 | 工程化：多预设矩阵（default/libpsf/python）、ccache/CMake 缓存、测试报告 artifact | 1–2 天 | `ci-matrix.yml` + artifact 产物 | 能解释 cache key 与 matrix 展开 |
| 3 | 两级流水线：self-hosted runner、secrets、external 预设接入、验证数字回填 | 1–2 天 | `ci-external.yml`（手动触发/自托管） | 能讲清 external 为什么不能上公共 runner |
| 4 | CD：Python wheel 构建、tag 触发 release、artifact 下载 | 可选 | 发布产物 | 理解 CI 与 CD 的分界 |

## 5. SPICEUnion 专属要点（建议）

- **依赖装配**：默认构建依赖 sibling 项目 `OrderedConcurrentPool`，CI 干净环境中
  不存在，需要在 workflow 里显式获取（checkout 另一仓库或 submodule）——这是
  阶段 1 的第一个真实卡点。
- **两级流水线**：云 runner 跑无 EDA 预设（`default` / `libpsf` / `python` /
  `python-libpsf-pic`）；`external` / `external-libpsf` 依赖 spectre 许可与
  `spectre_materials/external` 材料，只适合自托管 runner 或手动触发。
- **验证数字回填**：CI 落地后，`当前事实状态.md` 中的验证数字应由流水线产物
  回填，而非手工维护。

## 6. 掌握检查（每阶段至少两种证据）

- 复述：讲清 `push` vs `pull_request` 触发差异；
- 改错：修复一段故意写错的 YAML（trigger 拼错、`needs` 引用未定义 job 等）；
- 变式：把 `on: push` 改为 `on: schedule`，或给 matrix 增加编译器维度；
- 迁移：把同一套逻辑对照 GitLab CI 的 `stages/jobs` 讲一遍。

## 7. 下一步

完成阶段 0 微产出（5 句话总结“CI/CD 是什么、为什么存在、与本地门禁的区别”），
据此定位起点后进入阶段 1。

## 8. 参考来源（事实依据）

- GitHub Docs：Workflows — https://docs.github.com/en/actions/concepts/workflows-and-actions/workflows
- GitHub Docs：Workflow syntax — https://docs.github.com/en/actions/reference/workflow-syntax-for-github-actions
- GitHub Docs：About self-hosted runners — https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/about-self-hosted-runners
- GitHub Docs：Caching dependencies — https://docs.github.com/en/actions/using-workflows/caching-dependencies-to-speed-up-workflows
