# Python examples

本目录存放 SPICEUnion Python binding 的最小使用示例。

这些脚本不是 SPICEUnion core 功能的一部分，也不是新的业务层；它们只用于说明当前
pybind11 binding 应该如何被普通 Python 代码调用。

## 前置条件

需要先构建 Python binding：

```bash
cmake --preset python
cmake --build --preset python
```

如果需要真实读取 Spectre PSF fixture，还需要启用 libpsf backend：

```bash
cmake --preset python-libpsf-pic
cmake --build --preset python-libpsf-pic
```

说明：Python extension 是 shared module。若链接静态 libpsf，libpsf 需要以 PIC 方式构建。
本机验证使用 `local/external/libpsf/install-pic`，该目录不进入版本库。

## 运行方式

### 读取 fixture 结果

```bash
PYTHONPATH=build/python/bindings/python \
  ~/anaconda3/bin/python3.10 \
  bindings/python/examples/read_fixture_results.py tests/fixtures
```

启用 libpsf 的构建目录：

```bash
PYTHONPATH=build/python-libpsf-pic/bindings/python \
  ~/anaconda3/bin/python3.10 \
  bindings/python/examples/read_fixture_results.py tests/fixtures
```

这个脚本演示：

- `import spiceunion`；
- 查询 `version()`；
- 查询 `libpsf_reader_enabled()`；
- 读取 DC scalar；
- 读取 DC sweep；
- 读取 AC response；
- 派生 AC magnitude / phase；
- 读取 TRAN waveform；
- 计算 settling time。

### 做最小后处理

```bash
PYTHONPATH=build/python-libpsf-pic/bindings/python \
  ~/anaconda3/bin/python3.10 \
  bindings/python/examples/analyze_fixture_results.py tests/fixtures
```

这个脚本演示：

- DC sweep 的点数、首尾值和输出/输入比例；
- AC response 的最大 / 最小 magnitude；
- TRAN waveform 的起点、终点和 settling time；
- 缺失 signal 的失败状态。

脚本不依赖 numpy、matplotlib 或 pytest。

### 运行 workflow 示例

默认只构造 workflow 并提交空 batch，不调用外部 EDA 工具：

```bash
PYTHONPATH=build/python/bindings/python \
  ~/anaconda3/bin/python3.10 \
  bindings/python/examples/run_workflow.py input.scs
```

若要真实执行，需要提供合法网表和可用 simulator，并显式加 `--execute`：

```bash
PYTHONPATH=build/python/bindings/python \
  ~/anaconda3/bin/python3.10 \
  bindings/python/examples/run_workflow.py input.scs --execute
```

这个脚本演示：

- 创建 `Simulation`；
- 声明参数；
- 提交 batch；
- 遍历 `SimulationResult`；
- 成功时读取 AC signal；
- 失败时打印 `status_text()` 和 `message`。

## 失败语义

普通读取失败不抛异常，而是返回 result 对象：

```python
result = spiceunion.read_dc_value(result_dir, "vout")
if not result.ok():
    print(result.status_text(), result.message)
```

这种约定的目的：

- 读取失败不会和合法的 `0.0` 混淆；
- 批量处理多个结果时，不会因为一个 signal 缺失就打断整个脚本；
- `file_not_found`、`signal_not_found`、`unsupported_format` 等失败原因可被稳定判断。

Python 参数类型错误仍由 pybind11 抛出 Python 异常。

## CTest

当前 fixture 示例脚本纳入 CTest smoke 验证：

- `python_example_read_fixture_results`
- `python_example_analyze_fixture_results`

`run_workflow.py` 需要用户提供真实网表；默认不纳入 CTest。
