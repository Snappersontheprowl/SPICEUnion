#pragma once

#include "su/evaluator.hpp"

#include <unistd.h>
#include <cstdlib>
#include <string>

namespace {

bool spectre_file_exists(const char* path) {
  return ::access(path, F_OK) == 0;
}

bool spectre_executable_available() {
  return std::system("command -v spectre >/dev/null 2>&1") == 0;
}

std::string spectre_materials_netlist_path(const char* circuit_dir) {
  return std::string(SPICEUNION_SPECTRE_MATERIALS_DIR) + "/external/netlist/" + circuit_dir +
         "/input.scs";
}

std::string spectre_materials_netlist_path() {
  return spectre_materials_netlist_path("AMP/dc");
}

std::string spectre_materials_pdk_toplevel_path() {
  return std::string(SPICEUNION_SPECTRE_MATERIALS_DIR) + "/external/pdk/tsmcN65/toplevel.scs";
}

std::string spectre_materials_gpdk045_entry_path() {
  return std::string(SPICEUNION_SPECTRE_MATERIALS_DIR) + "/external/pdk/gpdk045/gpdk045_mos.scs";
}

std::string spectre_runtime_root(const char* scenario) {
  return std::string(SPICEUNION_PROJECT_ROOT) + "/local/runtime/spectre_" + scenario;
}

bool external_spectre_environment_is_ready(std::string* reason) {
#if !SPICEUNION_ENABLE_EXTERNAL_TESTS
  *reason =
      "External Spectre tests are disabled. Reconfigure with "
      "-DSPICEUNION_ENABLE_EXTERNAL_TESTS=ON to run this test.";
  return false;
#endif
  if (!spectre_executable_available()) {
    *reason = "spectre executable is not available in PATH";
    return false;
  }
  const auto pdk_toplevel = spectre_materials_pdk_toplevel_path();
  if (!spectre_file_exists(pdk_toplevel.c_str())) {
    *reason = "required PDK include is missing: " + pdk_toplevel;
    return false;
  }
  const auto gpdk045 = spectre_materials_gpdk045_entry_path();
  if (!spectre_file_exists(gpdk045.c_str())) {
    *reason = "required gpdk045 entry is missing: " + gpdk045;
    return false;
  }
  const auto netlist = spectre_materials_netlist_path();
  if (!spectre_file_exists(netlist.c_str())) {
    *reason = "baseline Spectre netlist is missing: " + netlist;
    return false;
  }
  return true;
}

}  // namespace
