# libpsf 上游候选备注

状态：`active`
最后验证：`2026-08-13`
适用范围：`M2 ResultIR / result_reader / 可选 libpsf backend`
单一事实来源：

- `CMakeLists.txt`
- `src/parse/libpsf_backend.cpp`
- `doc/develop_doc/M2.md`

## 背景

SPICEUnion M2 当前通过可选 `SPICEUNION_ENABLE_LIBPSF_READER=ON` 接入 `henjo/libpsf`，
用于读取普通 Spectre PSF 结果 fixture。公开 C++ API 与 Python binding 均不暴露
libpsf 类型，libpsf 只作为内部可替换 backend。

本备注用于保存一次关于 libpsf 上游来源的外部调研结论，避免这些判断散落到 M2 主文档中。

## 当前本机基线

当前本机 `local/external/libpsf/src` 的 remote 是：

```text
https://github.com/henjo/libpsf.git
```

本地基线已经验证通过：

- `cmake --preset libpsf && cmake --build --preset libpsf && ctest --preset libpsf --output-on-failure`
- `cmake --preset python-libpsf-pic && cmake --build --preset python-libpsf-pic && ctest --preset python-libpsf-pic --output-on-failure`

当前接入策略保持不变：

- 默认构建不依赖 libpsf；
- libpsf backend 可选启用；
- 公开 API 不暴露 libpsf 类型；
- 第三方源码和构建产物不进入 SPICEUnion 版本库。

## GitLab 相关来源

### `libpsf/libpsf-core`

链接：

- <https://gitlab.com/libpsf/libpsf-core>
- <https://gitlab.com/api/v4/projects/libpsf%2Flibpsf-core>

观察结果：

- GitLab 项目描述为 `PSF simulation data c++ library`，并注明 fork 自 `https://github.com/henjo/libpsf`。
- 默认分支为 `master`。
- 项目创建时间为 `2022-01-11`。
- GitLab 项目元数据中 `last_activity_at` 为 `2022-01-11`。
- 仓库仍然是 autotools 构建形态，README 中主要入口仍是 `./autogen.sh`、`make`、`sudo make install`。
- 未看到现代 CMake、活跃 release/tag 或持续维护迹象。

判断：

- 这是最接近 SPICEUnion 当前 C++ backend 需求的 GitLab 来源。
- 但它不应被直接视为比当前 `henjo/libpsf` 明显更适合的替代源。
- 可以作为候选源验证，但不建议立刻替换当前本机 baseline。

### `libpsf/libpsf-python`

链接：

- <https://gitlab.com/libpsf/libpsf-python>
- <https://gitlab.com/api/v4/projects/libpsf%2Flibpsf-python>

观察结果：

- 该项目更像 `libpsf` 的 Python 包 / Python extension 分发工程。
- 其 README 主要入口是 `import libpsf` 与 `libpsf.PSFDataSet(...)`。
- `setup.py` 使用 Boost.Python、numpy 和底层 `psf` library。
- `.gitlab-ci.yml` 重点维护 manylinux wheel 构建与发布流程。
- GitLab 项目元数据中 `last_activity_at` 到 `2025-11-05`，但默认分支最近提交主要集中在 2022 年 wheel 发布相关工作。

判断：

- 该项目维护形态比原始 C++ 仓库更现代，但主要价值在 Python wheel 分发。
- 不建议把它直接接入 SPICEUnion C++ core。
- 可作为外部对照工具，用来确认某些 PSF 文件是否能被 libpsf 系列读取。

## GitHub 相关候选

### `lekez2005/libpsf` 与 `henjo/libpsf` PR #19

链接：

- <https://github.com/henjo/libpsf/pull/19>

观察结果：

- PR 标题为 `Fix compilation error and invalid chunk bug for large simulation data`。
- 该线索包含对大仿真数据、chunk/padding、segfault、CMake 构建等问题的修复讨论。
- PR 仍未合入原始 `henjo/libpsf`。
- 后续评论中还出现过约 3GB transient 文件触发整数溢出或大文件问题的讨论。

判断：

- 虽然它不在 GitLab，但对 SPICEUnion 的 C++ result reader 稳健性可能比 `libpsf-python` 更有实际价值。
- 若后续出现大 PSF、异常 chunk、PSFXL 或崩溃类问题，应优先把该 fork/PR 纳入验证。

## 对 SPICEUnion 的接入判断

当前不建议改变 M2 正式策略。

推荐继续保持：

```text
SPICEUnion ResultReader API
  -> 可选内部 libpsf backend
  -> 不暴露第三方 libpsf 类型
  -> 以后可替换为其他 backend 或 native parser
```

不建议：

- 默认启用 libpsf；
- 把 `libpsf-python` 作为 C++ core 依赖；
- 把第三方 libpsf 源码直接 vendoring 进 SPICEUnion；
- 为了对齐某个上游仓库而改变 SPICEUnion 公开 API。

## 后续验证建议

如果后续重新推进 M2 result reader 的上游评估，建议做一个本地 spike，而不是直接替换依赖。

候选验证顺序：

| 顺序 | 候选 | 目的 |
|---:|---|---|
| 1 | 当前 `henjo/libpsf` baseline | 保留已验证基线 |
| 2 | GitLab `libpsf/libpsf-core` | 验证 GitLab C++ fork 是否更适合当前 fixture |
| 3 | `lekez2005/libpsf` / PR #19 | 验证 CMake、大文件和 chunk 修复价值 |
| 4 | GitLab `libpsf/libpsf-python` | 仅作为 Python 对照工具，不作为 C++ backend |

建议验证表：

| 项目 | 需要记录 |
|---|---|
| 来源 | URL、remote、commit |
| license | 是否仍为 LGPL-3.0 语境 |
| 构建方式 | autotools / CMake / 本地 patch |
| PIC 支持 | 是否能构建可链接进 Python shared module 的静态库 |
| C++ backend 验证 | 是否通过 `ctest --preset libpsf` |
| Python binding 验证 | 是否通过 `ctest --preset python-libpsf-pic` |
| fixture 覆盖 | DC scalar、DC sweep、AC、TRAN |
| 改善点 | 是否改善当前 unsupported 或失败场景 |
| 风险 | 新依赖、构建复杂度、license、接口差异 |

## 当前结论

GitLab 上的 libpsf 相关项目值得记录，但还不足以支持 SPICEUnion 立即替换当前 libpsf baseline。

当前最稳妥的工程选择是：

- 继续把 libpsf 保持为可选内部 backend；
- 保留当前已验证 fixture 和测试矩阵；
- 若出现真实格式覆盖问题，再用本备注中的候选源做对照验证；
- 只有当候选源在同一测试矩阵下稳定优于当前 baseline，才考虑更新本机默认验证来源。
