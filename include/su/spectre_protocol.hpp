#pragma once

#include "su/core.hpp"

#include <string>

namespace su {

enum class SpectreCompletion {
  kContinue,
  kSucceeded,
  kFailed,
};

std::string format_spectre_run_command(const ParameterState& state);
SpectreCompletion classify_spectre_completion_line(
    const std::string& line,
    bool* seen_resource_stats);

}  // namespace su
