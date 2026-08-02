#include "su/version.hpp"

#include <gtest/gtest.h>

TEST(SmokeTest, VersionIsAvailable) {
  EXPECT_STREQ(su::version(), "0.1.0");
}
