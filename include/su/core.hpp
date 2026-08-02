#pragma once

#include <map>
#include <string>

namespace su {

using ParameterState = std::map<std::string, double>;

struct EvaluatorOptions {
  std::string netlist_path;
  int num_workers = 16;
  std::string work_dir_base = "/dev/shm/spiceunion_workers";
  std::string workspace_namespace;
  int timeout_seconds = 60;
  int restart_attempts = 1;
};

}  // namespace su
