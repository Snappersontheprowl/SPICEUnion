# CI/CD 学习笔记

状态：`active`（可持续追加）
最后更新：`2026-08-23`
适用范围：`GitHub Actions + SPICEUnion`

## 1. 核心概念

### CI / CD / 本地门禁

- **本地门禁（pre-commit gate）**：提交前在本机跑自动化验证。我们此前对 SPICEUnion
  的六预设 ctest 回归就是本地门禁——有“持续验证”的习惯，但没有机器部分。
- **CI（持续集成）**：每次 push / PR 由一台干净的机器自动拉代码、构建、跑测试，
  快速暴露集成问题。事实依据：GitHub Docs 定义 workflow 为“由一个或多个 job 组成
  的可配置自动化流程，由 YAML 文件定义，由仓库事件触发”。
- **CD（持续交付/部署）**：在 CI 之上自动产出可发布制品（wheel、二进制、release）。
- 一句话区分：本地门禁靠人自觉，CI 靠机器强制，CD 把“能跑”变成“能发”。

### GitHub Actions 词汇表

| 词 | 含义 |
|---|---|
| workflow | 一个自动化流程，对应一个 YAML 文件（`.github/workflows/`） |
| event | 触发条件：`push` / `pull_request` / `schedule` / `workflow_dispatch` |
| job | workflow 内的一次任务（可并行、可依赖） |
| step | job 内的一条具体命令或 action |
| runner | 执行 job 的机器（云托管或自托管） |
| action | 可复用的步骤包，如 `actions/checkout@v4` |
| artifact | job 结束后保存的产物（测试报告、构建结果） |
| cache | 跨运行复用的依赖/构建缓存 |
| secret | 仓库级加密变量（许可、token），日志中自动打码 |

## 2. 最小 workflow 示例（阶段 1 起点）

```yaml
name: ci-default
on:
  push:
  pull_request:
jobs:
  build-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake --preset default
      - name: Build
        run: cmake --build --preset default
      - name: Test
        run: ctest --preset default --output-on-failure
```

逐行要点：

- `on` 下的 `push` / `pull_request` 表示事件触发；二者都会跑，但语义不同
  （push 是提交后，PR 是合并前验证）。
- `runs-on` 选择 runner 镜像；`ubuntu-latest` 是最常用的云托管镜像。
- `uses: actions/checkout@v4` 把仓库代码拉到 runner；`@v4` 是 action 的版本 tag，
  建议锁定主版本。
- `run` 里的命令与本地 shell 一致；SPICEUnion 的 preset 设计让 CI 命令与本地
  完全复用。

## 3. SPICEUnion 的已知卡点（阶段 1 必踩）

- **OrderedConcurrentPool 不在 CI 环境**：默认构建要求 sibling 源树，需要
  `actions/checkout` 额外获取该仓库（或改用 submodule / install 包）。
- **外部预设不能上公共 runner**（建议）：`external` / `external-libpsf` 需要
  spectre 许可与 `spectre_materials/external` 材料，公共 runner 没有；方案是自托管
  runner + secrets。

## 4. 常见误区

- 把 CI 等同于“写了 YAML”：核心是“机器在干净环境强制重跑”，YAML 只是载体。
- `push` 与 `pull_request` 触发混用导致同一提交跑两遍：需要按分支策略选择。
- 在 CI 里依赖本机已有依赖/缓存：干净环境必须显式安装或 checkout。
- 把密钥写进 `run` 命令：必须用 `secrets` 并在日志中打码。

## 5. 待验证问题（随进度更新）

- [ ] OrderedConcurrentPool 在 CI 中最稳的装配方式（checkout 另一仓库 vs submodule）。
- [ ] ccache / CMake 缓存的 key 设计（preset 变化时如何失效）。
- [ ] external 预设的自托管 runner 是否需要独立 label 与并发限制。
- [ ] 验证数字回填的最小实现（从 ctest 输出解析并写入文档）。

## 6. 落地记录（2026-08-23）

### 采用的方案（业界主流折中，已确认）

| 层 | 执行位置 | 内容 | 触发 |
|---|---|---|---|
| 云 CI | GitHub 托管 runner | `default` / `python`（阶段 1）；libpsf 预设待阶段 2 | push + PR |
| 自托管 CI | 本机 runner（label `eda`） | `external-libpsf`（spectre + PDK + libpsf） | 手动 / 定时 |
| 本机脚本 | 本机 | `scripts/verify_all_presets.sh` 一键六预设 | 手动 |

### 已交付的仓库侧产物

- `.github/workflows/ci-eda-free.yml`：云 runner 流水线，matrix 覆盖 default / python；
  `OrderedConcurrentPool` 通过仓库变量 `ORDERED_POOL_REPOSITORY` 装配（sibling 依赖
  在干净环境的第一个卡点）。
- `.github/workflows/ci-external.yml`：自托管流水线，`runs-on: [self-hosted, linux, eda]`，
  仅手动/定时触发；`SPECTRE_MATERIALS_DIR`、`LIBPSF_INCLUDE_DIR`、`LIBPSF_LIBRARY`
  经 secrets 注入。
- `scripts/verify_all_presets.sh`：本机一键 configure/build/test 六个预设，
  已在本机实测跑通（全部通过）。

### workflow 逐段解读（学习要点）

- `on:`：`push` / `pull_request` / `workflow_dispatch` / `schedule`（cron 定时）；
- `needs:`：`build-test` 等 `require-config` 完成后才跑，先校验配置缺失并给出中文提示；
- `strategy.matrix`：一个 job 模板展开多个预设，互不阻塞（`fail-fast: false`）；
- `vars.` 与 `secrets.`：仓库级配置与机密，分别用于可公开变量和必须打码的路径/许可；
- `runs-on: [self-hosted, linux, eda]`：多个 label 是“且”关系，只有带 `eda` label
  的自托管 runner 能接这个 job。

### GitHub 侧待办清单（需要人工操作）

1. 把 SPICEUnion 与 OrderedConcurrentPool 推送到 GitHub；
2. 仓库 Settings → Secrets and variables → Actions → **Variables**：
   `ORDERED_POOL_REPOSITORY`（如 `owner/OrderedConcurrentPool`）；
3. 同上 → **Secrets**：`SPECTRE_MATERIALS_DIR`、
   `LIBPSF_INCLUDE_DIR`、`LIBPSF_LIBRARY`（自托管机器上的绝对路径）；
4. 仓库 Settings → Actions → **Runners**：在本机注册 runner 并添加 `eda` label；
5. push 后观察 `ci-eda-free` 首次运行，读日志、修失败。

### 已知限制 / 阶段 2 待办

- libpsf 预设（`libpsf` / `python-libpsf-pic`）需要先在 runner 上构建
  `henjo/libpsf`（本机 `local/external/libpsf` 不入库），matrix 再扩展；
- ngspice 外部测试上云需要先拆分 spectre / ngspice 门控（当前 external 预设
  configure 依赖 `spectre_materials/external` 材料）；
- 公共仓库使用自托管 runner 有任意代码执行风险（GitHub 官方警告）：只允许
  `push` 到 main / `workflow_dispatch` 触发，不用 `pull_request` 接自托管 job。
