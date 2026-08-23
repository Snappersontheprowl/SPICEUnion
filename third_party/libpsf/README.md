# third_party/libpsf

该目录存放 SPICEUnion 对第三方依赖 `henjo/libpsf`（PSF 结果解析库）的固定构建配方。

## 背景

上游 https://github.com/henjo/libpsf 是 autotools 工程（`configure.ac` /
`Makefile.am`），仓库本身没有 CMake 构建。本项目本机构建产物
`local/external/libpsf/install[-pic]` 使用的是根目录手写 `CMakeLists.txt`
配方；该文件位于被忽略的 `local/` 下，不入库。

为了让**云 CI 与本机使用同一套构建方式**（构建参数、产物布局一致），把该配方
vendor 到本目录。CI 在 checkout 上游源码后，先把它复制到源码根目录，再执行
常规 CMake configure / build / install。

## 维护规则

- 锁定上游 commit：`6efc14f7c5fa7e09a07e354cc54b9135ec353d70`
  （`LIBPSF_REF`，见 `.github/workflows/ci-eda-free.yml`）。
- 升级 libpsf 时：更新 `LIBPSF_REF`、本目录 `CMakeLists.txt` 的源列表与
  `README.md`，并同步更新 `doc/cicd/CICD学习笔记.md` 第 8 节。
- 本目录只放“能让第三方按我们要求构建”的增量配方，不放第三方源码。
