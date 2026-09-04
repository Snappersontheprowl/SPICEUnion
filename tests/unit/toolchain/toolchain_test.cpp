#include "su/toolchain.hpp"

#include <gtest/gtest.h>

#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>

namespace {

namespace fs = std::filesystem;

constexpr const char* kNgspiceEnv = "SPICEUNION_NGSPICE";
constexpr const char* kSpectreEnv = "SPICEUNION_SPECTRE";
constexpr const char* kPathEnv = "PATH";

class ScopedEnv {
 public:
  ScopedEnv() {
    for (const char* name : {kNgspiceEnv, kSpectreEnv, kPathEnv}) {
      const char* value = std::getenv(name);
      saved_[name] = value == nullptr ? std::optional<std::string>{}
                                      : std::optional<std::string>(value);
    }
  }

  ~ScopedEnv() {
    for (const auto& [name, value] : saved_) {
      if (value.has_value()) {
        ::setenv(name.c_str(), value->c_str(), 1);
      } else {
        ::unsetenv(name.c_str());
      }
    }
  }

  void set(const char* name, const std::string& value) {
    ::setenv(name, value.c_str(), 1);
  }

  void clear(const char* name) { ::unsetenv(name); }

 private:
  std::map<std::string, std::optional<std::string>> saved_;
};

class TempDir {
 public:
  TempDir() {
    static int counter = 0;
    path_ = fs::temp_directory_path() /
            ("su_toolchain_test_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter++));
    fs::create_directories(path_);
  }

  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

void make_fake_executable(const fs::path& dir, const std::string& name,
                          const std::string& version_line) {
  const auto file = dir / name;
  std::ofstream out(file);
  out << "#!/bin/sh\n";
  out << "if [ \"$1\" = \"--version\" ]; then\n";
  out << "  echo '" << version_line << "'\n";
  out << "else\n  echo fake-run\nfi\n";
  out.close();
  fs::permissions(file, fs::perms::owner_all | fs::perms::group_all |
                            fs::perms::others_read | fs::perms::others_exec,
                  fs::perm_options::replace);
}

}  // namespace

TEST(ToolchainTest, EnvVarNamesAreStable) {
  EXPECT_STREQ(su::simulator_env_var(su::SimulatorKind::kSpectre),
               "SPICEUNION_SPECTRE");
  EXPECT_STREQ(su::simulator_env_var(su::SimulatorKind::kNgspice),
               "SPICEUNION_NGSPICE");
}

TEST(ToolchainTest, NgspiceEnvOverrideWinsOverPath) {
  ScopedEnv env;
  TempDir env_dir;
  TempDir path_dir;

  make_fake_executable(env_dir.path(), "ngspice",
                       "** ngspice-41 : Circuit level simulation program");
  make_fake_executable(path_dir.path(), "ngspice",
                       "ngspice compiled from ngspice revision 27");

  env.set(kNgspiceEnv, (env_dir.path() / "ngspice").string());
  env.set(kPathEnv, path_dir.path().string());

  const auto handle = su::find_simulator(su::SimulatorKind::kNgspice);
  EXPECT_TRUE(handle.found);
  EXPECT_EQ(handle.discovered_from, "env");
  EXPECT_EQ(handle.executable_path, (env_dir.path() / "ngspice").string());
  EXPECT_EQ(handle.version_number, "41");
}

TEST(ToolchainTest, NgspicePathDiscoveryParsesRevisionStyleVersion) {
  ScopedEnv env;
  TempDir dir;
  make_fake_executable(dir.path(), "ngspice",
                       "ngspice compiled from ngspice revision 27");

  env.clear(kNgspiceEnv);
  env.set(kPathEnv, dir.path().string());

  const auto handle = su::find_simulator(su::SimulatorKind::kNgspice);
  EXPECT_TRUE(handle.found);
  EXPECT_EQ(handle.discovered_from, "path");
  EXPECT_EQ(handle.executable_path, (dir.path() / "ngspice").string());
  EXPECT_EQ(handle.version_number, "27");
}

TEST(ToolchainTest, SpectreEnvOverrideIsHonored) {
  ScopedEnv env;
  TempDir dir;
  make_fake_executable(dir.path(), "spectre",
                       "Spectre 23.1.1 : Circuit simulation");

  env.set(kSpectreEnv, (dir.path() / "spectre").string());
  env.clear(kNgspiceEnv);
  env.set(kPathEnv, dir.path().string());

  const auto handle = su::find_simulator(su::SimulatorKind::kSpectre);
  EXPECT_TRUE(handle.found);
  EXPECT_EQ(handle.discovered_from, "env");
  EXPECT_EQ(handle.version_number, "23.1.1");
}

TEST(ToolchainTest, NotFoundWhenEnvAndPathHaveNoSimulator) {
  ScopedEnv env;
  TempDir empty_dir;

  env.clear(kNgspiceEnv);
  env.clear(kSpectreEnv);
  env.set(kPathEnv, empty_dir.path().string());

  const auto ngspice = su::find_simulator(su::SimulatorKind::kNgspice);
  EXPECT_FALSE(ngspice.found);
  EXPECT_TRUE(ngspice.executable_path.empty());

  const auto spectre = su::find_simulator(su::SimulatorKind::kSpectre);
  EXPECT_FALSE(spectre.found);
  EXPECT_TRUE(spectre.executable_path.empty());
}
