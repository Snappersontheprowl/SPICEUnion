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
};

using SimulatorSessionPtr = std::unique_ptr<SimulatorSession>;

}  // namespace su
