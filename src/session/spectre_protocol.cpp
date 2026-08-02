#include "su/spectre_protocol.hpp"

#include <iomanip>
#include <sstream>

namespace su {

std::string format_spectre_run_command(const ParameterState& state) {
  std::ostringstream command;
  command << "(progn";
  command << std::scientific << std::setprecision(6);
  for (const auto& item : state) {
    command << " (sclSetAttribute (sclGetParameter top \"" << item.first
            << "\") \"value\" " << item.second << ")";
  }
  command << " (sclRun \"all\"))\n";
  return command.str();
}

SpectreCompletion classify_spectre_completion_line(
    const std::string& line,
    bool* seen_resource_stats) {
  if (seen_resource_stats != nullptr &&
      line.find("Peak resident memory used") != std::string::npos) {
    *seen_resource_stats = true;
  }

  if (line.find("spectre completes") != std::string::npos &&
      line.find("0 errors") != std::string::npos) {
    return SpectreCompletion::kSucceeded;
  }

  if (seen_resource_stats != nullptr && *seen_resource_stats) {
    if (line == "t") {
      return SpectreCompletion::kSucceeded;
    }
    if (line == "nil") {
      return SpectreCompletion::kFailed;
    }
  }

  return SpectreCompletion::kContinue;
}

}  // namespace su
