#pragma once

#include "su/result.hpp"

#include <string>

namespace su::parse {

ReadResult<ScalarResult> read_dc_value_with_libpsf(const std::string& result_dir,
                                                   const std::string& signal_name);

}  // namespace su::parse
