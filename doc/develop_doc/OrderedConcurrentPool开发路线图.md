# OrderedConcurrentPool 详细开发路线图

更新时间：2026-08-06

本文记录 `OrderedConcurrentPool` 从 SPICEUnion 内部 `SimulatorPool` 中抽离为完整独立项目的开发路线。

当前结论：

- M3.5.0-M3.5.6 已完成；
- SPICEUnion 已装配外部 `OrderedConcurrentPool` 项目；
- `SimulatorPool` 已改为 adapter；
- 独立 `OrderedConcurrentPool` 项目已创建；
- SPICEUnion 内部通用池副本已删除；
- `OrderedConcurrentPool` 已具备 MIT license、`CHANGELOG.md`、最小 benchmark、
  CMake install/export package 与 GitHub Actions CI；
- `OrderedConcurrentPool` 的 `main` 与 `v0.1.0` tag 已发布到 `origin`。

## 1. 目标

`OrderedConcurrentPool` 的目标是成为一个领域无关的 C++17 并发 worker pool：

- 支持固定数量 worker；
- 支持 worker 生命周期管理；
- 支持 batch job 并发执行；
- 输出结果严格保持输入顺序；
- 单个 job 失败不影响同 batch 其他 job；
- startup 失败时清理已启动 worker；
- shutdown 可重复调用；
- 不依赖 SPICEUnion、Spectre、Ngspice、PSF、EDA 业务类型。

它在 SPICEUnion 中的定位：

```text
SPICEUnion Evaluator
  -> SimulatorPool adapter
  -> OrderedConcurrentPool
  -> SimulatorSession worker
```

## 2. 当前事实基础

当前 SPICEUnion 装配方式：

- 外部项目路径：`~/my_lab/projects/OrderedConcurrentPool`；
- 外部头文件：`include/ocp/ordered_concurrent_pool.hpp`；
- CMake cache 变量：`SPICEUNION_ORDERED_POOL_SOURCE_DIR`；
- CMake target：`ocp::ordered_concurrent_pool`；
- SPICEUnion adapter：`src/pool/simulator_pool.hpp` / `src/pool/simulator_pool.cpp`。

已具备的池行为：

- `OrderedConcurrentPool` 位于 `ocp` 命名空间；
- `OrderedConcurrentPool` 只依赖 C++ 标准库；
- `OrderedConcurrentPool` 不包含 `su::*` 业务类型；
- 构造固定数量 worker；
- `start_all()` 并行启动所有 worker；
- `run_batch()` / `evaluate_batch()` 并发执行输入任务；
- 通过 `results[index]` 保证输出与输入同序；
- 通过 idle worker queue 分派可用 worker；
- 单任务异常被转换为 `TaskResult::failure(...)`；
- startup 失败时调用 `shutdown_all()` 清理；
- 析构时调用 `shutdown_all()`。

已完成的发布条件：

- license 已明确为 MIT；
- 已补 `CHANGELOG.md`；
- 已补最小 benchmark target；
- 已形成 `v0.1.0` release 口径；
- 已验证 build / test / benchmark / install；
- 已增加 GitHub Actions CI，首轮运行通过。

## 3. 职责边界

### 3.1 OrderedConcurrentPool 负责

- worker 数量校验；
- worker 创建与持有；
- worker 启动；
- worker 停止；
- 空闲 worker 获取与归还；
- batch job 分派；
- batch result 保序；
- worker startup 异常传播；
- per-job 异常转换为调用方指定的失败结果；
- 重复 shutdown 的安全性；
- 基础线程同步。

### 3.2 SPICEUnion 负责

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
- EDA fixture 与外部仿真器验证。

### 3.3 明确禁止的耦合

`OrderedConcurrentPool` 不应包含：

- `namespace su`；
- `su::TaskResult`；
- `su::ParameterState`；
- `su::EvaluatorOptions`；
- `SimulatorSession`；
- `work_dir` 固定概念；
- Spectre / Ngspice 字符串；
- PSF / raw 结果读取逻辑；
- 电路指标或 optimizer 逻辑。

依赖方向只能是：

```text
SPICEUnion -> OrderedConcurrentPool
```

不能反向依赖。

## 4. 建议公开 API 形态

第一版 API 优先保持小而稳定。

建议命名空间：

```cpp
namespace ocp {
}
```

建议核心类型：

```cpp
struct PoolOptions {
  std::size_t worker_count = 1;
};
```

建议 worker 契约：

```cpp
template <class Job, class Result>
class Worker {
 public:
  virtual ~Worker() = default;
  virtual void start() = 0;
  virtual Result run(const Job& job) = 0;
  virtual void stop() noexcept = 0;
};
```

建议 pool 入口：

```cpp
template <class Job, class Result>
class OrderedConcurrentPool {
 public:
  using WorkerFactory = std::function<std::unique_ptr<Worker<Job, Result>>(std::size_t worker_id)>;
  using FailureHandler = std::function<Result(std::size_t worker_id, const Job& job, std::exception_ptr error)>;

  OrderedConcurrentPool(PoolOptions options, WorkerFactory factory, FailureHandler failure_handler);

  void start_all();
  std::vector<Result> run_batch(const std::vector<Job>& jobs);
  void shutdown_all() noexcept;
  bool started() const noexcept;
  std::size_t worker_count() const noexcept;
};
```

这里不把失败结果类型写死，是为了避免把 `TaskResult` 带进独立库。

SPICEUnion adapter 可这样映射：

```text
Job    = su::ParameterState
Result = su::TaskResult
failure_handler(...) -> TaskResult::failure(...)
```

## 5. 不建议第一版支持的能力

以下能力先不做，除非后续出现真实消费者：

- 动态增减 worker；
- job priority；
- job cancellation；
- retry 策略；
- worker 自动重建；
- timeout 强制杀线程；
- 分布式队列；
- coroutine backend；
- task graph；
- progress callback；
- metrics registry。

原因：

- 这些能力会显著扩大 API 面；
- 当前 SPICEUnion 只需要固定 worker 的 batch 保序执行；
- 过早加入会让独立项目变成调度框架，而不是小而可靠的基础组件。

## 6. 阶段路线

### M3.5.0 固化现有池行为契约

状态：已完成。

目标：

- 在不改变现有生产代码的前提下，为当前 `SimulatorPool` 行为补齐直接测试。

建议产出：

- `tests/simulator_pool_contract_test.cpp`
- fake worker / fake session 测试夹具；
- `src/pool/README.md` 更新；
- `doc/develop_doc/当前事实状态.md` 更新。

测试覆盖：

- worker 数量必须大于 0；
- factory 不可为空；
- `start_all()` 启动全部 worker；
- startup 失败后调用已构造 worker 的 stop；
- 未 start 时执行 batch 抛出错误；
- 空 batch 返回空结果；
- batch 输出保持输入顺序；
- job 异常转换为失败结果；
- 单 job 失败不影响其他 job；
- `shutdown_all()` 可重复调用；
- 析构时清理 worker。

完成定义：

- 默认测试通过；
- 不引入新的外部 EDA 依赖；
- 所有测试不依赖任务完成时间顺序；
- 文档明确当前池仍未独立。

### M3.5.1 在 SPICEUnion 内抽出通用池核心

状态：已完成。

目标：

- 在 SPICEUnion 仓库内先建立领域无关的 `OrderedConcurrentPool` 实现；
- 暂不创建独立仓库；
- 暂不改变 `Evaluator` 的公开行为。

阶段内建议位置：

```text
src/pool/ordered_concurrent_pool.hpp
```

M3.5.1 当时实现为 header-only：

```text
src/pool/ordered_concurrent_pool.hpp
```

M3.5.5 后该内部副本已删除，当前源码位于：

```text
~/my_lab/projects/OrderedConcurrentPool/include/ocp/ordered_concurrent_pool.hpp
```

建议要求：

- 命名空间使用 `ocp` 或内部中性命名，不使用 `su`；
- 只依赖 C++ 标准库；
- 不包含 `include/su/*`；
- 不出现 `Spectre`、`Ngspice`、`TaskResult`、`ParameterState`；
- API 使用模板或类型擦除表达 `Job` / `Result`；
- failure handler 由调用方注入。

建议测试：

- 新增独立 `OrderedConcurrentPool` contract tests；
- 测试用 `Job = int` 或小结构体；
- 测试用 `Result = int` 或小结构体；
- 用不同 sleep 时间验证“完成顺序乱、返回顺序不乱”。

完成定义：

- `OrderedConcurrentPool` 可在不包含任何 SPICEUnion 公开头文件的测试中使用；
- 原 `SimulatorPool` 还未替换也可以接受；
- 默认测试通过。

### M3.5.2 用 adapter 替换 SimulatorPool 内部调度

状态：已完成。

目标：

- 让 `SimulatorPool` 成为 SPICEUnion 到 `OrderedConcurrentPool` 的适配层；
- 保持 `include/su/evaluator.hpp` 对外行为不变。

建议改造方向：

```text
SimulatorPool
  - 负责把 EvaluatorOptions / SessionFactory / work_dir 转成 worker factory
  - 负责把 ParameterState 转给 OrderedConcurrentPool
  - 负责把异常转成 TaskResult::failure
  - 不再自己维护 idle queue 与 result ordering
```

建议产出：

- `SimulatorPool` 实现瘦身；
- `src/pool/README.md` 更新；
- evaluator 契约测试继续通过；
- Spectre / Ngspice 相关测试继续通过。

完成定义：

- `Evaluator` API 不变；
- `TaskResult` 保序不变；
- worker work directory 命名不变；
- startup failure 语义不变；
- per-task failure isolation 不变；
- default / external / libpsf 测试矩阵通过。

### M3.5.3 稳定化与边界检查

状态：已完成。

目标：

- 在 SPICEUnion 内确认通用池足够稳定，可以独立出去。

建议检查项：

- 头文件依赖检查：`ordered_concurrent_pool` 不依赖 `include/su`；
- 命名检查：无 `su`、Spectre、Ngspice 业务词；
- stress test：多 worker、多 job、混合失败；
- sanitizer 可选检查；
- 重复 start / shutdown 语义明确；
- 异常传播与异常转换语义明确。

建议验证：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure

cmake --preset external
cmake --build --preset external
ctest --preset external --output-on-failure
```

libpsf backend 若本地环境可用，也应验证：

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

完成定义：

- SPICEUnion 所有既有测试通过；
- 独立池测试覆盖核心语义；
- 没有未解释的线程竞态；
- 没有未实测的性能数字进入文档。

已记录验证：

| 配置 | 结果 |
|---|---|
| default | `100% tests passed, 0 tests failed out of 80` |
| external | `100% tests passed, 0 tests failed out of 80` |
| libpsf | `100% tests passed, 0 tests failed out of 90` |

已执行边界检查：

```bash
rg -n "namespace su|su::|TaskResult|ParameterState|EvaluatorOptions|SimulatorSession|Spectre|Ngspice|PSF|raw|work_dir" ~/my_lab/projects/OrderedConcurrentPool/include/ocp/ordered_concurrent_pool.hpp
```

结果：无匹配。

### M3.5.4 创建独立项目

状态：已完成。

目标：

- 在 SPICEUnion 之外建立完整独立项目。

建议路径：

```text
~/my_lab/projects/OrderedConcurrentPool
```

建议结构：

```text
OrderedConcurrentPool/
  CMakeLists.txt
  README.md
  LICENSE
  include/
    ocp/
      ordered_concurrent_pool.hpp
  tests/
    CMakeLists.txt
    ordered_concurrent_pool_test.cpp
  examples/
    basic_batch.cpp
  cmake/
    OrderedConcurrentPoolConfig.cmake.in
```

建议 CMake target：

```text
ocp::ordered_concurrent_pool
```

建议项目性质：

- C++17；
- header-only 优先；
- GoogleTest 仅用于测试；
- 不依赖 SPICEUnion；
- 不依赖 EDA 工具；
- 不依赖 Python。

README 应说明：

- 项目解决的问题；
- 最小使用示例；
- worker 契约；
- 保序语义；
- 失败处理方式；
- CMake 引入方式；
- 当前不支持的能力。

完成定义：

- 独立项目可以单独 configure / build / test；
- 独立 README 中文为主；
- 示例可编译；
- SPICEUnion 不再是运行独立测试的前提。

已完成事实：

- 项目路径：`~/my_lab/projects/OrderedConcurrentPool`；
- 头文件：`include/ocp/ordered_concurrent_pool.hpp`；
- 测试：`tests/ordered_concurrent_pool_test.cpp`；
- 示例：`examples/basic_batch.cpp`；
- CMake target：`ocp::ordered_concurrent_pool`；
- 安装导出：`OrderedConcurrentPoolConfig.cmake` 与 `OrderedConcurrentPoolTargets.cmake`；
- license：MIT。

已记录验证：

```bash
cmake -S . -B build-clean -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-clean
ctest --test-dir build-clean --output-on-failure
```

```text
100% tests passed, 0 tests failed out of 8
```

示例程序：

```bash
./build/ordered_concurrent_pool_basic_batch
```

安装验证：

```bash
cmake --install build-clean --prefix /tmp/ocp_install_...
```

安装产物包含：

- `include/ocp/ordered_concurrent_pool.hpp`；
- `lib64/cmake/OrderedConcurrentPool/OrderedConcurrentPoolConfig.cmake`；
- `lib64/cmake/OrderedConcurrentPool/OrderedConcurrentPoolConfigVersion.cmake`；
- `lib64/cmake/OrderedConcurrentPool/OrderedConcurrentPoolTargets.cmake`；
- `share/doc/OrderedConcurrentPool/LICENSE`；
- `share/doc/OrderedConcurrentPool/README.md`；
- `share/doc/OrderedConcurrentPool/CHANGELOG.md`。

### M3.5.5 SPICEUnion 装配独立项目

状态：已完成。

目标：

- 让 SPICEUnion 使用独立项目，而不是维护自己的通用池副本。

推荐过渡方式：

```text
SPICEUnion CMake option
  -> 指向 ../OrderedConcurrentPool
  -> add_subdirectory(...)
  -> link ocp::ordered_concurrent_pool
```

建议 CMake 选项：

```cmake
SPICEUNION_ORDERED_POOL_SOURCE_DIR
```

装配原则：

- 开发期使用 sibling source tree；
- 不把独立项目源码复制进 SPICEUnion；
- 不在 SPICEUnion 内修改独立项目源码；
- 后续稳定后再评估 `find_package(OrderedConcurrentPool)`。

SPICEUnion 侧建议变化：

- `SimulatorPool` include 独立项目头文件；
- `src/pool/README.md` 改为说明 adapter 职责；
- 根 `README.md` 外部依赖增加 OrderedConcurrentPool；
- `doc/develop_doc/当前事实状态.md` 记录集成事实。

完成定义：

- SPICEUnion 默认构建能找到并使用 OrderedConcurrentPool；
- 没有重复维护两套 pool core；
- SPICEUnion 测试矩阵通过；
- OrderedConcurrentPool 独立测试也通过。

已完成事实：

- SPICEUnion 通过 `SPICEUNION_ORDERED_POOL_SOURCE_DIR` 指向外部项目；
- 默认路径为 `${CMAKE_CURRENT_SOURCE_DIR}/../OrderedConcurrentPool`；
- `spiceunion_core` 链接 `ocp::ordered_concurrent_pool`；
- `tests/ordered_concurrent_pool_test.cpp` 通过外部 target 验证池契约；
- `src/pool/ordered_concurrent_pool.hpp` 内部副本已删除；
- `SimulatorPool` include `ocp/ordered_concurrent_pool.hpp`。

已记录验证：

| 配置 | 结果 |
|---|---|
| default | `100% tests passed, 0 tests failed out of 80` |
| external | `100% tests passed, 0 tests failed out of 80` |
| libpsf | `100% tests passed, 0 tests failed out of 90` |

### M3.5.6 发布准备

状态：已完成。

目标：

- 让 OrderedConcurrentPool 具备长期维护形态。

已完成产出：

- 版本号：`v0.1.0`；
- license：MIT；
- `CHANGELOG.md`；
- install/export CMake；
- install docs：`LICENSE`、`README.md`、`CHANGELOG.md`；
- README 使用限制；
- minimal benchmark：`benchmarks/ordered_pool_benchmark.cpp`；
- benchmark CMake 开关：`ORDERED_CONCURRENT_POOL_BUILD_BENCHMARKS`；
- GitHub Actions CI：Release build/test、install consumer、ASan、TSan；
- git tag：`v0.1.0`，已发布到 `origin`。

完成定义：

- 可被第三方 CMake 项目消费；
- SPICEUnion 可作为真实使用案例；
- 文档不包含未验证性能结论。

已记录验证：

```bash
cd ~/my_lab/projects/OrderedConcurrentPool
cmake -S . -B build-release-check -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-release-check
ctest --test-dir build-release-check --output-on-failure
```

```text
100% tests passed, 0 tests failed out of 8
```

benchmark 验证：

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

GitHub Actions 首轮验证：

| Run | Commit | 结果 |
|---|---|---|
| `31111814593` | `0524272` | Release build/test、install consumer、ASan、TSan 均通过 |

## 7. 建议提交边界

推荐按以下粒度提交：

| 提交 | 内容 |
|---|---|
| 1 | 补齐 `SimulatorPool` 现有行为契约测试 |
| 2 | 新增 SPICEUnion 内部 `OrderedConcurrentPool` 通用核心与测试 |
| 3 | `SimulatorPool` 改为 adapter，保持 evaluator 行为不变 |
| 4 | 稳定化测试与文档收口 |
| 5 | 创建独立 `OrderedConcurrentPool` 项目 |
| 6 | SPICEUnion 改为装配独立项目 |
| 7 | 独立项目发布准备 |

不要把“抽通用核心”“替换 adapter”“创建独立仓库”“SPICEUnion 接入外部项目”混在一个提交里。

## 8. 测试矩阵

### 8.1 OrderedConcurrentPool 独立测试

| 测试项 | 期望 |
|---|---|
| zero worker | 构造失败 |
| null factory | 构造失败 |
| null worker | 构造或启动失败 |
| start all | 所有 worker 启动一次 |
| startup failure cleanup | 已启动 worker 被停止 |
| run before start | 抛出明确异常 |
| empty batch | 返回空结果 |
| ordered result | 输出顺序等于输入顺序 |
| out-of-order completion | 完成顺序不影响返回顺序 |
| per-job exception | 由 failure handler 转换 |
| mixed success failure | 成功任务不被失败任务污染 |
| repeated shutdown | 不崩溃、不重复破坏状态 |
| destructor cleanup | worker 被停止 |
| stress batch | 多 worker、多 job 结果稳定 |

### 8.2 SPICEUnion 集成测试

| 测试项 | 期望 |
|---|---|
| evaluator contract | 保序、失败隔离、生命周期不变 |
| Spectre interactive | 现有 Spectre external 测试通过 |
| Ngspice batch | AC / TRAN / DC sweep external 测试通过 |
| libpsf backend | 结果读取测试不受 pool 替换影响 |
| README 示例命令 | 仍可执行 |

## 9. 风险与处理方式

| 风险 | 处理方式 |
|---|---|
| 抽象过度 | 第一版只服务固定 worker、batch 保序、失败隔离 |
| API 被 SPICEUnion 业务污染 | 用纯 `int` / 小结构体测试独立池 |
| 线程竞态 | 增加 stress tests，必要时使用 sanitizer |
| adapter 替换破坏 evaluator 行为 | 替换前先补现有契约测试 |
| 独立仓库后双份代码漂移 | SPICEUnion 只装配独立项目，不复制维护 |
| 构建配置复杂化 | 先用 source dir option，稳定后再做 package install |

## 10. 当前状态与后续边界

M3.5 已收口。

当前事实：

- `OrderedConcurrentPool` 已从 SPICEUnion 内部实现演进为 sibling 独立项目；
- SPICEUnion 默认通过 source tree 装配该独立项目；
- 独立项目已有 MIT license、README、CHANGELOG、示例、测试、benchmark、install/export
  package 与 GitHub Actions CI；
- 独立项目的 `main` 与 `v0.1.0` tag 已发布到 `origin`。

后续若继续推进，需要新的明确目标：

- 如果要进入更完整工程化发布：增加多编译器矩阵、package consumer release smoke test
  和可选 release artifact；
- 如果回到 SPICEUnion 主线：继续评估 M3 收敛任务或进入 M4 C ABI / Python binding。

当前仍不建议提前做：

- 直接把 `TaskResult` 泛化成复杂 result wrapper；
- 提前设计动态扩缩容、取消、重试、优先级；
- 在没有跨机器、跨负载实测前，把 benchmark 数字写成性能承诺。
