# OrderedConcurrentPool 专项路线图

更新时间：2026-08-11

本文只记录 `OrderedConcurrentPool` 从 SPICEUnion 内部调度实现抽离为独立项目的专项边界、装配关系、阶段状态和后续限制。SPICEUnion 总体计划见 `../00_项目总览/00_项目章程.md`；SPICEUnion 当前事实见 `../00_项目总览/01_当前事实状态.md`。

## 1. 当前结论

M3.5 已收口。

当前事实：

- SPICEUnion 已装配外部 `OrderedConcurrentPool` 项目；
- `SimulatorPool` 已改为 SPICEUnion adapter；
- SPICEUnion 内部通用池副本已删除；
- `OrderedConcurrentPool` 已成为 sibling 独立项目；
- 独立项目已具备 MIT license、`README.md`、`CHANGELOG.md`、示例、测试、benchmark、CMake install/export package 与 GitHub Actions CI；
- 独立项目的 `main` 与 `v0.1.0` tag 已发布到 `origin`。

外部项目路径：

```text
~/my_lab/projects/OrderedConcurrentPool
```

## 2. 拆分目标

`OrderedConcurrentPool` 的目标是成为领域无关的 C++17 ordered concurrent worker pool。

它负责：

- 固定数量 worker；
- worker 生命周期管理；
- batch job 并发执行；
- 输出结果严格保持输入顺序；
- 单个 job 异常由调用方转换为失败结果；
- startup 失败时清理已启动 worker；
- `shutdown_all()` 可重复调用；
- 基础线程同步。

它不负责：

- SPICEUnion 业务类型；
- `EvaluatorOptions`；
- `ParameterState`；
- `TaskResult`；
- `TaskStatus`；
- `SimulatorSession`；
- worker work directory 命名；
- Spectre / Ngspice backend；
- timeout 语义；
- 仿真失败分类；
- 结果读取与 ResultIR；
- EDA fixture；
- optimizer 或电路指标。

依赖方向只能是：

```text
SPICEUnion -> OrderedConcurrentPool
```

不能让 `OrderedConcurrentPool` 反向依赖 SPICEUnion。

## 3. SPICEUnion 装配方式

SPICEUnion 当前通过 source tree 方式装配外部项目。

CMake cache 变量：

```text
SPICEUNION_ORDERED_POOL_SOURCE_DIR
```

默认路径：

```text
${CMAKE_CURRENT_SOURCE_DIR}/../OrderedConcurrentPool
```

链接 target：

```text
ocp::ordered_concurrent_pool
```

SPICEUnion adapter：

```text
src/pool/simulator_pool.hpp
src/pool/simulator_pool.cpp
```

装配原则：

- 不把独立项目源码复制回 SPICEUnion；
- 不在 SPICEUnion 中维护第二份 pool core；
- SPICEUnion 只负责把自身类型映射到 OCP 的 `Job` / `Result` / worker factory / failure handler；
- 后续是否改为 `find_package(OrderedConcurrentPool)` 以真实分发需求为准。

## 4. 阶段状态

| 阶段 | 状态 | 产出 |
|---|---|---|
| M3.5.0 | 已完成 | `SimulatorPool` 行为契约测试 |
| M3.5.1 | 已完成 | SPICEUnion 内部领域无关 `OrderedConcurrentPool` 核心 |
| M3.5.2 | 已完成 | `SimulatorPool` 改为 `OrderedConcurrentPool` adapter |
| M3.5.3 | 已完成 | default / external / libpsf 验证与边界检查 |
| M3.5.4 | 已完成 | 独立 `OrderedConcurrentPool` 项目 |
| M3.5.5 | 已完成 | SPICEUnion 装配外部 `OrderedConcurrentPool` 项目并移除内部副本 |
| M3.5.6 | 已完成 | MIT license、CHANGELOG、benchmark、CMake install/export、GitHub Actions CI、`v0.1.0` 发布 |

详细验证结果见 `../00_项目总览/01_当前事实状态.md`。

## 5. 独立项目结构

```text
OrderedConcurrentPool/
  CMakeLists.txt
  README.md
  LICENSE
  CHANGELOG.md
  include/
    ocp/
      ordered_concurrent_pool.hpp
  tests/
    ordered_concurrent_pool_test.cpp
  examples/
    basic_batch.cpp
  benchmarks/
    ordered_pool_benchmark.cpp
  cmake/
    OrderedConcurrentPoolConfig.cmake.in
  .github/
    workflows/
      ci.yml
  doc/
    study_notes/
      ci_cd_and_release_notes.md
```

独立项目性质：

- C++17；
- header-only；
- 只依赖 C++ 标准库和 Threads；
- GoogleTest 仅用于测试；
- 不依赖 SPICEUnion；
- 不依赖 EDA 工具；
- 不依赖 Python。

## 6. 当前验证入口

独立项目测试：

```bash
cd ~/my_lab/projects/OrderedConcurrentPool
cmake -S . -B build-release-check -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-release-check
ctest --test-dir build-release-check --output-on-failure
```

benchmark：

```bash
cmake -S . -B build-benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DORDERED_CONCURRENT_POOL_BUILD_TESTS=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_EXAMPLES=OFF \
  -DORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS=ON
cmake --build build-benchmark
./build-benchmark/ordered_concurrent_pool_benchmark
```

benchmark 输出为 CSV，只用于本机回归观察，不作为跨机器性能承诺。

install 验证：

```bash
cmake --install build-release-check --prefix /tmp/ocp_release_install_...
```

GitHub Actions 当前覆盖：

- Release build / test；
- install package；
- external consumer smoke test；
- AddressSanitizer；
- ThreadSanitizer。

## 7. 当前不建议扩展的能力

除非出现真实消费者，否则不建议第一阶段继续扩展：

- 动态增减 worker；
- job priority；
- job cancellation；
- retry 策略；
- worker 自动重建；
- timeout 强制杀线程；
- 分布式队列；
- coroutine backend；
- task graph；
- metrics registry。

原因：

- 当前 SPICEUnion 只需要固定 worker 的 batch 保序执行；
- 这些能力会显著扩大 API 面；
- 过早加入会让独立项目变成通用调度框架，削弱当前“小而可靠”的定位。

## 8. 后续边界

如果继续推进 OCP，优先级应为：

1. 根据真实 CI 结果修复构建兼容问题；
2. 在需要发布 patch 版本时整理 `CHANGELOG.md` 并打新 tag；
3. 若出现外部消费需求，再评估 `find_package` 接入 SPICEUnion；
4. 若出现真实功能需求，再设计动态扩缩容、取消、重试或优先级。

如果回到 SPICEUnion 主线，应优先继续 `../00_项目总览/03_开发路线图.md` 中的 M3 收敛或 M4 任务，而不是继续扩张 OCP 功能面。
