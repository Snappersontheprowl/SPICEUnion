#include "su/version.hpp"

#include <gtest/gtest.h>

// 与 CMake 的 project(VERSION ...) 比较，避免升级版本时手改测试。
TEST(SmokeTest, VersionIsAvailable) {
  EXPECT_STREQ(su::version(), SPICEUNION_VERSION);
}
