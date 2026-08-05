# tests

本目录存放基于 GoogleTest 的测试。

## 测试分组

- Smoke tests 验证构建与链接路径。
- Contract tests 使用 fake 或 scripted session 验证执行层行为。
- Result tests 验证 M2 ResultIR、读取状态和公开 result_reader API 骨架。
- Result reader tests 验证 `.raw` 目录定位、AC 数学 helper、settling time 和
  M2.3 文件读取行为。默认构建下真实 PSF 读取保持禁用。
- Fixture tests 使用 `tests/fixtures/` 中的小型固定仿真结果样本，验证 reader 解析
  是否稳定。
- Simulation semantic tests 使用 `tests/support/` 中的公共语义 helper，验证不同 backend
  读出的 ResultIR 是否满足同一类电路的物理语义。
- Result reader tests 启用 libpsf backend 后，会读取 `tests/fixtures/psf/` 中的
  `dcOp.dc` fixture 验证 scalar 读取，读取标准 `ac.ac` 与 `stb.stb` fixture 验证
  swept complex response 读取，并读取普通 `tran.tran` fixture 验证 transient
  waveform 读取。
- Spectre fixture 源 netlist 位于 `tests/fixtures/spectre/`；固化后的普通 PSF 结果位于
  `tests/fixtures/psf/`。
- Ngspice session tests 默认验证 RC AC / RC TRAN 配置、netlist 渲染，以及三列 AC /
  两列 TRAN `wrdata` 输出解析；启用外部测试后，会真实调用 `ngspice -b` 并通过
  evaluator / pool 跑 AC 与 TRAN batch。
- Manual tests 位于 `tests/manual/`，只用于人工 spike，不进入默认 `ctest`。
- 外部测试可能依赖 Spectre、PDK 或 Ngspice 访问权限，默认必须禁用，除非显式启用。

## 命令

默认测试：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

外部 Spectre 生命周期测试：

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external
```

该 preset 同时会运行外部 Ngspice 测试；测试会优先检查 `SPICEUNION_NGSPICE`，再检查
`PATH` 中的 `ngspice` / `ngspice_con`，找不到时对应测试会 skip。

本机 libpsf reader 测试：

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

该测试默认尝试使用 `local/external/libpsf/install` 中的 `henjo/libpsf`，并读取
`tests/fixtures/psf/` 中已提交的小型 PSF fixture。启用 libpsf 后还会运行
`spiceunion_simulation_semantics_test`，验证 Spectre RC low-pass AC fixture 读出的
`AcResponse` 满足与 Ngspice RC AC 相同的 -3 dB 频率语义。libpsf 本身仍不属于默认 CI 契约。

外部测试当前覆盖：

- Spectre interactive handshake 与 stop；
- single-session `(sclRun "all")`；
- 默认 Spectre evaluator multi-worker batch。
- Ngspice RC low-pass AC batch session；
- Ngspice RC charging TRAN batch session；
- 默认 Ngspice evaluator multi-worker batch；
- Ngspice TRAN evaluator multi-worker batch。
