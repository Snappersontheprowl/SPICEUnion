# tests/external/ngspice 说明

## 功能

本目录存放依赖本机 Ngspice 可执行文件的真实工具测试。

## 本级模块职责

- `ngspice_session_external_test.cpp`：真实调用 `ngspice -b` 跑内置 AC / TRAN / DC
  示例，并用结果读取 helper 验证物理语义。
- `ngspice_evaluator_external_test.cpp`：通过 `Evaluator` 批量运行 Ngspice 内置任务，
  验证真实 batch 执行、worker 目录隔离和结果交付。

## 当前约定

- 未启用 `SPICEUNION_ENABLE_EXTERNAL_TESTS` 时，用例统一 `GTEST_SKIP()`。
- 启用外部测试后，测试会优先接受 `SPICEUNION_NGSPICE` 指定的可执行文件；否则从
  `PATH` 查找 `ngspice` 或 `ngspice_con`。
- 真实仿真产物写入 `<项目根>/local/runtime/ngspice_<场景>_<pid>/`，测试结束后由
  用例清理。
