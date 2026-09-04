#pragma once

#include <unistd.h>
#include <cstdlib>
#include <string>

namespace spiceunion_test {

inline bool ngspice_available() {
  const char* explicit_path = std::getenv("SPICEUNION_NGSPICE");
  if (explicit_path != nullptr && explicit_path[0] != '\0' && ::access(explicit_path, X_OK) == 0) {
    return true;
  }
  return std::system("command -v ngspice >/dev/null 2>&1") == 0 ||
         std::system("command -v ngspice_con >/dev/null 2>&1") == 0;
}

inline bool external_ngspice_environment_is_ready(std::string* reason) {
#if !SPICEUNION_ENABLE_EXTERNAL_TESTS
  *reason =
      "External Ngspice tests are disabled. Reconfigure with "
      "-DSPICEUNION_ENABLE_EXTERNAL_TESTS=ON to run this test.";
  return false;
#endif
  if (!ngspice_available()) {
    *reason = "ngspice executable is not available in PATH";
    return false;
  }
  return true;
}

}  // namespace spiceunion_test
