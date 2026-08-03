# M2 libpsf spike 记录

时间：2026-08-03

## 1. 目标

本 spike 用于判断 `henjo/libpsf` 是否适合作为 SPICEUnion M2 的可选内部 backend。

本记录只服务 M2.3 决策，不表示 libpsf 已经进入默认构建或公开 API。

边界：

- 不把 `psf.h` 暴露到 `include/su/`。
- 不把 `PSFDataSet`、`PSFVector`、`PSFScalar` 暴露给 SPICEUnion 用户。
- 不让默认 CMake / CTest 依赖 libpsf。
- 不复制 libpsf 源码到 SPICEUnion core。
- 不在本阶段承诺 native parser。

## 2. 本机环境探测

### 2.1 pkg-config

命令：

```bash
pkg-config --exists libpsf
pkg-config --cflags --libs libpsf
```

结果：

```text
pkg_config_exit=1
Package libpsf was not found in the pkg-config search path.
Package 'libpsf', required by 'virtual:world', not found
```

结论：

```text
当前机器没有可通过 pkg-config 发现的 henjo/libpsf。
```

### 2.2 文件系统搜索

命令：

```bash
find /usr /usr/local /opt ~ \
  -name 'psf.h' -o -name 'libpsf.so*' -o -name 'libpsf.a' -o -name 'libpsf.pc'
```

结果：

```text
/opt/cadence/IC231/tools.lnx86/lib/64bit/libpsf.so
/opt/cadence/SPECTRE231/tools.lnx86/lib/64bit/libpsf.so
```

结论：

```text
本机只发现 Cadence 自带 libpsf.so，没有发现 henjo/libpsf 的 psf.h、libpsf.pc 或
可直接使用的开源 libpsf install。
Cadence 自带 libpsf.so 不是 henjo/libpsf API，不能按 PSFDataSet 接口直接使用。
```

### 2.3 构建工具

命令：

```bash
command -v autoconf automake libtoolize aclocal make g++ gcc pkg-config
g++ --version
```

结果：

```text
/usr/bin/make
/usr/bin/g++
/usr/bin/gcc
/usr/bin/pkg-config
g++ (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
```

未发现：

```text
autoconf
automake
libtoolize
aclocal
```

结论：

```text
当前机器缺少构建 henjo/libpsf autotools 主线所需的 autoconf / automake / libtoolize。
因此暂不能在本机直接用上游 README 的 ./autogen.sh && make 路径完成构建。
```

## 3. 上游源码观察

临时源码位置：

```text
/tmp/tmp.ysVPub9iQ5/libpsf
```

上游 commit：

```text
6efc14f7c5fa7e09a07e354cc54b9135ec353d70
2014-11-29T10:53:38+01:00
Fixed problem with PSF files that contain sweep parameters but no traces such as files used
```

源码结构包含：

```text
include/psf.h
include/psfdata.h
src/*.cc
bindings/python/
test/
examples/
```

公开 C++ API 包含 `PSFDataSet`，概念上可以读取：

- signal names；
- sweep values；
- scalar signal；
- vector signal；
- complex vector signal。

## 4. 当前可用 fixture 候选

在 `spectre_materials` 中发现历史 `.raw` 输出候选：

```text
~/my_lab/projects/spectre_materials/local/runtime/output_test_run/introspection_output/simulation_input.raw
~/my_lab/projects/spectre_materials/local/runtime/output_test_run/stress_test_output/simulation_input.raw
~/my_lab/projects/spectre_materials/local/runtime/sim_result/input_C0/input_C0.raw
~/my_lab/projects/spectre_materials/local/runtime/sim_result/input_C1/input_C1.raw
~/my_lab/projects/spectre_materials/local/runtime/sim_result/results_dc_stb_muco/input_multicorner.raw
```

这些只是候选路径，尚未固定为 SPICEUnion fixture。

M2.3 后续需要确认：

- 哪些目录体积适合进入 `tests/fixtures/`；
- 哪些需要用脚本生成而不是直接提交；
- 哪些 signal 适合作为参考值；
- 是否有 dc/ac/tran/sens 四类完整覆盖。

## 5. 新增 manual probe

新增：

```text
tests/manual/libpsf_probe.cpp
```

用途：

- 手动验证 `henjo/libpsf` C++ API 能否打开指定 PSF 文件；
- 打印 sweep 信息；
- 打印 signal names；
- 可选读取指定 signal 并打印类型、长度和前几个值。

该 probe 默认不进入 CMake，也不进入 `ctest`。

预期手动编译方式：

```bash
g++ -std=c++17 tests/manual/libpsf_probe.cpp \
  -I/path/to/libpsf/include -L/path/to/libpsf/lib -lpsf \
  -o /tmp/libpsf_probe
```

预期运行方式：

```bash
/tmp/libpsf_probe /path/to/result.raw/dcOp.dc vout
```

## 6. 当前结论

截至本记录：

```text
1. 本机尚未安装可直接使用的 henjo/libpsf。
2. pkg-config 无法发现 libpsf。
3. 本机缺少构建上游 autotools 主线所需的 autoconf / automake / libtoolize。
4. Cadence 自带 libpsf.so 不能按 henjo/libpsf API 使用。
5. 已建立 manual probe 框架，但尚未完成 probe 编译和真实 PSF 文件读取。
```

因此，当前状态是：

```text
libpsf backend 尚不能进入正式实现。
下一步需要先解决 henjo/libpsf 的本地构建 / 安装 / 指定 include-library 路径问题，
或选择一个现代化 fork / CMake patch 做进一步评估。
```

## 7. 下一步建议

优先级：

1. 确定是否允许为本机安装 autotools 依赖，或使用已有现代化 fork。
2. 若可构建，先在 `/tmp` 或外部 scratch 目录构建 henjo/libpsf，不写入 SPICEUnion。
3. 用 `tests/manual/libpsf_probe.cpp` 读取一个最小 `dcOp.dc`。
4. 成功后再扩展到 `ac.ac`、`tran.tran`、sensitivity。
5. 只有 probe 成功后，才设计 `SPICEUNION_ENABLE_LIBPSF_READER` 和内部 backend。

暂不建议：

- 在 SPICEUnion 中自动下载 / 构建 libpsf。
- 直接链接 Cadence 自带 `libpsf.so`。
- 直接开始 native parser。
- 直接实现 `read_dc_value()` 的正式文件读取逻辑。
