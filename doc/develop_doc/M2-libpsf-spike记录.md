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

## 6. 初始结论

截至 2026-08-03 初始记录：

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

## 8. 本地 CMake patch 构建验证

时间：2026-08-04

### 8.1 本地实验区

按“项目内、Git 外、边界清楚”的方式新建本地实验区：

```text
local/external/libpsf/
├── src/       # henjo/libpsf clone 与临时 CMake patch
├── build/     # CMake build 目录
├── install/   # 本地安装结果
└── libpsf_probe
```

项目 `.gitignore` 已加入：

```gitignore
/local/
```

因此该目录不会进入 SPICEUnion 版本库。这里保存的是本机 spike 过程，不是正式
third_party 依赖。

### 8.2 上游源码

clone 源码：

```bash
git clone https://github.com/henjo/libpsf.git local/external/libpsf/src
```

当前 commit：

```text
6efc14f7c5fa7e09a07e354cc54b9135ec353d70
2014-11-29T10:53:38+01:00
Fixed problem with PSF files that contain sweep parameters but no traces such as files used
```

### 8.3 最小 CMake patch

在 `local/external/libpsf/src/CMakeLists.txt` 中临时补了最小 CMake 构建脚本。

本 patch 的边界：

- 只构建 C++ reader core；
- 不构建 Python binding；
- 不修改任何 PSF 解析源码；
- 不提交 libpsf 源码或构建产物；
- 不接入 SPICEUnion 默认 CMake。

核心源文件来自上游 `src/Makefile.am`，并在链接 `manual probe` 时补充
`src/psfpropertyblock.cc`。该补充是源文件列表修正，不涉及解析逻辑修改。

### 8.4 构建命令

```bash
cmake -S local/external/libpsf/src \
  -B local/external/libpsf/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/my_lab/projects/SPICEUnion/local/external/libpsf/install

cmake --build local/external/libpsf/build
cmake --install local/external/libpsf/build
```

结果：

```text
local/external/libpsf/install/lib64/libpsf.a
local/external/libpsf/install/include/psf.h
local/external/libpsf/install/include/psfdata.h
```

构建成功，但出现上游旧代码自身 warning：

```text
warning: no return statement in function returning non-void
warning: control reaches end of non-void function
```

这些 warning 暂未修改，因为本阶段原则是“只改构建，不改解析源码”。正式接入前需评估
是否在内部 backend 中规避异常路径，或在 fork/patch 中做兼容性修补。

### 8.5 manual probe 编译

命令：

```bash
g++ -std=c++17 tests/manual/libpsf_probe.cpp \
  -Ilocal/external/libpsf/install/include \
  -Llocal/external/libpsf/install/lib64 \
  -lpsf \
  -o local/external/libpsf/libpsf_probe
```

结果：

```text
编译成功。
```

### 8.6 libpsf 自带 dcOp 样本验证

命令：

```bash
local/external/libpsf/libpsf_probe \
  local/external/libpsf/src/test/data/dcOp.dc vout
```

结果摘要：

```text
file=local/external/libpsf/src/test/data/dcOp.dc
is_swept=0
nsweeps=0
sweep_npoints=0
sweep_values=null
signal_count=2
signal[0]=vin
signal[1]=vout
requested_signal=vout
signal_type=scalar value=2.5
```

结论：

```text
henjo/libpsf 可以读取自带 dcOp scalar 样本。
```

### 8.7 spectre_materials 真实 dcOp 验证

命令：

```bash
local/external/libpsf/libpsf_probe \
  ~/my_lab/projects/spectre_materials/local/runtime/sim_result/input_C11/input_C11.raw/dcOp.dc \
  net6
```

结果摘要：

```text
file=~/my_lab/projects/spectre_materials/local/runtime/sim_result/input_C11/input_C11.raw/dcOp.dc
is_swept=0
nsweeps=0
sweep_npoints=0
sweep_values=null
signal_count=14
signal[0]=net6
signal[1]=net1
signal[2]=net3
signal[3]=net2
signal[4]=net5
signal[5]=net7
requested_signal=net6
signal_type=scalar value=0.8
```

结论：

```text
henjo/libpsf 可以读取 spectre_materials 历史 Spectre 输出中的 dcOp scalar。
```

### 8.8 spectre_materials 真实 stb 验证

命令：

```bash
local/external/libpsf/libpsf_probe \
  ~/my_lab/projects/spectre_materials/local/runtime/sim_result/input_C11/input_C11.raw/stb.stb \
  loopGain
```

结果摘要：

```text
file=~/my_lab/projects/spectre_materials/local/runtime/sim_result/input_C11/input_C11.raw/stb.stb
is_swept=1
nsweeps=1
sweep_npoints=201
sweep_param_names: freq
sweep_type=double_vector size=201 [0]=1 [1]=1.12202 [2]=1.25893 [3]=1.41254 [4]=1.58489
signal_count=1
signal[0]=loopGain
requested_signal=loopGain
signal_type=complex_double_vector size=201 [0]=(-287890,26932.1) [1]=(-287245,30150.6)
```

结论：

```text
henjo/libpsf 可以读取 spectre_materials 历史 Spectre 输出中的 swept complex vector。
这对 M2 的 AC/STB 类频域 ResultIR 具有直接参考价值。
```

### 8.9 当前覆盖缺口

当前在 `spectre_materials/local/runtime` 历史输出中没有找到明显的 `.ac` / `.tran` 文件：

```bash
find ~/my_lab/projects/spectre_materials/local/runtime \
  -path '*.raw/*' -type f \( -name '*.ac' -o -name '*.tran' -o -name '*tran*' -o -name '*ac*' \)
```

结果为空。

因此：

- dcOp scalar 路径已验证；
- swept complex vector 路径已通过 `stb.stb` 验证；
- 标准 AC 文件读取仍需专门 fixture；
- transient waveform 文件读取仍需专门 fixture；
- sensitivity 文件读取仍需专门 fixture。

## 9. 更新后的 M2.3 判断

截至 2026-08-04：

```text
libpsf 作为 M2 可选内部 backend 的可行性明显提高。
它已经能在本机用临时 CMake patch 构建，并能读取 dcOp scalar 与 STB swept
complex vector。
```

下一步建议从“能不能构建”转为“如何正式、干净地接入”：

1. 固化一个很小的 PSF fixture 策略，优先 dcOp + stb/AC。
2. 增加 `SPICEUNION_ENABLE_LIBPSF_READER` CMake option。
3. 增加内部 backend 文件：

   ```text
   src/parse/libpsf_backend.hpp
   src/parse/libpsf_backend.cpp
   ```

4. 保持公开 API 不变：

   ```text
   include/su/result_reader.hpp
   ```

5. 先实现 `read_dc_value()` 的真实读取。
6. 再实现 swept complex vector 到 `AcResponse` 的转换。

仍然不建议：

- 把 `local/external/libpsf/src` 纳入 Git；
- 把 `psf.h` 暴露到 `include/su/`；
- 让默认 CMake 强依赖 libpsf；
- 在 fixture 不足时一次性承诺 dc/ac/tran/sens 全部完成。

## 10. 可选 libpsf backend 首次落地

时间：2026-08-04

在 spike 证明 libpsf 可用后，M2.3 先落地最小正式读取链路：

```text
read_dc_value(result_dir, signal_name)
  -> src/parse/result_reader.cpp
  -> src/parse/libpsf_backend.cpp
  -> henjo/libpsf PSFDataSet
  -> ScalarResult
```

### 10.1 CMake 开关

新增：

```cmake
SPICEUNION_ENABLE_LIBPSF_READER
SPICEUNION_LIBPSF_INCLUDE_DIR
SPICEUNION_LIBPSF_LIBRARY
```

默认：

```text
SPICEUNION_ENABLE_LIBPSF_READER=OFF
```

默认构建不寻找、不编译、不链接 libpsf。

启用时，CMake 会优先尝试：

```text
local/external/libpsf/install/include
local/external/libpsf/install/lib
local/external/libpsf/install/lib64
```

也可以手动传入：

```bash
-DSPICEUNION_LIBPSF_INCLUDE_DIR=/path/to/libpsf/include
-DSPICEUNION_LIBPSF_LIBRARY=/path/to/libpsf.a
```

libpsf include 目录在 CMake 中按 `SYSTEM` include 处理，避免第三方 header warning
被 SPICEUnion 自身 `-Wall -Wextra -Wpedantic` 放大。

### 10.2 内部 backend 文件

新增：

```text
src/parse/libpsf_backend.hpp
src/parse/libpsf_backend.cpp
```

边界：

- 文件位于 `src/parse/`，不在 `include/su/`；
- `psf.h` 只出现在 `.cpp` 内部；
- `PSFDataSet`、`PSFBase`、`PSFScalar` 不进入公开 API；
- 第三方异常被翻译成 `ResultStatus`。

当前错误语义：

```text
result_dir 为空       -> kInvalidInput
signal_name 为空     -> kInvalidInput
dcOp.dc 不存在       -> kFileNotFound
signal 不存在        -> kSignalNotFound
非 scalar signal     -> kUnsupportedFormat
PSF 文件无效/异常    -> kParseError
```

### 10.3 测试

默认构建：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

结果：

```text
100% tests passed, 0 tests failed out of 40
```

libpsf 构建：

```bash
cmake -S . -B cmake-build-libpsf \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSPICEUNION_BUILD_TESTS=ON \
  -DSPICEUNION_ENABLE_EXTERNAL_TESTS=OFF \
  -DSPICEUNION_ENABLE_LIBPSF_READER=ON
cmake --build cmake-build-libpsf
ctest --test-dir cmake-build-libpsf --output-on-failure
```

结果：

```text
100% tests passed, 0 tests failed out of 43
```

新增 libpsf 测试覆盖：

- 读取 libpsf 自带 `dcOp.dc` 中的 `vout=2.5`；
- 缺失 signal 返回 `kSignalNotFound`；
- 读取 `spectre_materials` 历史真实 `dcOp.dc` 中的 `net6=0.8`。

### 10.4 更新后的下一步

M2.3 后续不再是“libpsf 能不能用”，而是：

1. 固化最小 fixture 策略，避免长期依赖 `local/` 或 `spectre_materials/local/runtime`。
2. 增加 swept complex vector 到 `AcResponse` 的内部转换。
3. 补标准 `ac.ac` fixture，或明确以 `stb.stb` 作为第一批频域 fixture。
4. 补 `tran.tran` fixture。
5. 补 legacy sensitivity fixture。
