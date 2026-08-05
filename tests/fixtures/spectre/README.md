# Spectre 源 fixture

本目录存放用于生成 Spectre 固化结果 fixture 的源 netlist。

目录职责：

- `tests/fixtures/spectre/`：保存可复现的 Spectre 输入材料，例如 `input.scs`。
- `tests/fixtures/psf/`：保存已经固化进仓库、可由测试读取的 PSF 结果。

源 netlist 用于说明 fixture 的来源与复现方式；默认 `ctest` 不直接运行这里的 Spectre
仿真。需要真实调用 Spectre 的测试必须放在 external 测试路径下，并在外部依赖缺失时 skip。

命名规则：

- 每个 fixture 使用稳定业务语义目录名，例如 `rc_lowpass_ac/`。
- 不使用 `new`、`tmp`、`final`、`v2` 等阶段性名称。
