# conda-forge 发布候选

本目录是 `spiceunion` 发布到 conda-forge 的**参考材料**，不是 feedstock 本体。
conda-forge 的正式入口是独立仓库 `conda-forge/spiceunion-feedstock`（需社区/维护者
创建），配方以那里维护的 `recipe/meta.yaml` 为准。

## 目标体验

```bash
conda create -n spice -c conda-forge spiceunion ngspice python=3.12
conda activate spice
spiceunion doctor      # 同时看到 spiceunion 与 ngspice
python demo.py         # Simulation -> 批量仿真 -> 读结果
```

SPICEUnion 负责自己的库与 doctor 探测；ngspice 由 conda-forge 提供——这符合
“项目不安装真实仿真器”的边界，联合安装只是 conda 生态的能力。

## 提交 feedstock 前的人工核对清单

- [ ] PyPI 首版已发布，`source.url` 的 sha256 来自 PyPI sdist；
- [ ] `meta.yaml` 的 build/host/run 依赖与 `pyproject.toml`、`CMakeLists.txt` 一致；
- [ ] conda-build 本地试构建通过（`meta.yaml` 见同目录）；
- [ ] 在含 ngspice 的干净 conda env 中 `spiceunion doctor` 与 workflow smoke 通过；
- [ ] 版本号与 PyPI 对齐，license = Apache-2.0，license_file 指向仓库 LICENSE。

## 边界

- 默认包不含 libpsf（LGPL-3.0），BINPSF 读取返回 `unsupported_format`；
- Spectre 不随 conda 分发，doctor 负责提示。
