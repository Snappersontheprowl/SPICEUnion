#pragma once

#include "su/result.hpp"

#include <map>
#include <string>

namespace su {

using ParameterState = std::map<std::string, double>;

struct EvaluatorOptions {
  std::string netlist_path;
  int num_workers = 16;
  std::string work_dir_base = "local/runtime/workers";
  std::string workspace_namespace;
  ResultFormat result_format = ResultFormat::kUnknown;
  int timeout_seconds = 60;
  int restart_attempts = 1;
};

}  // namespace su
