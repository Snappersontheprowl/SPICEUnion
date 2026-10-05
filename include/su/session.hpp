#pragma once

#include "su/core.hpp"
#include "su/task_result.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace su {

class SimulatorSession {
 public:
  virtual ~SimulatorSession() = default;

  virtual void start() = 0;
  virtual TaskResult run(const ParameterState& state, std::chrono::seconds timeout) = 0;
  virtual void stop(bool graceful) noexcept = 0;

  virtual std::size_t worker_id() const noexcept = 0;
  virtual const std::string& work_dir() const noexcept = 0;

  // 每 job 目录支持：
  // - 返回 true 表示 session 会把本次 job 的产物直接写进 set_job_work_dir() 指定的目录
  //   （例如每次 run 新起子进程的 Ngspice）；
  // - 返回 false 表示 session 的产物仍落在固定 work_dir()，由池子在 job 结束后快照到
  //   每 job 目录（例如产物目录在启动时绑定的 Spectre）。
  virtual bool writes_into_job_work_dir() const noexcept { return false; }

  // 池子在每个 job 开始前调用（仅当 writes_into_job_work_dir() 为 true 时有意义）。
  virtual void set_job_work_dir(const std::string& /*job_work_dir*/) {}
};

using SimulatorSessionPtr = std::unique_ptr<SimulatorSession>;

}  // namespace su
