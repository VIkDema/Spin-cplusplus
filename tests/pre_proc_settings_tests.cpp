#include <gtest/gtest.h>

#include "../src/main/pre_proc_settings.hpp"

TEST(PreProcSettingsTest, DefaultCommand) {
  PreProcSettings settings;
  ASSERT_EQ(settings.GetCommand(), "");
}

TEST(PreProcSettingsTest, SetAndGetCommand) {
  PreProcSettings settings;
  settings.SetCommand("CUSTOM_COMMAND");
  ASSERT_EQ(settings.GetCommand(), "CUSTOM_COMMAND");
}

TEST(PreProcSettingsTest, IsDefault) {
  PreProcSettings settings;
  ASSERT_TRUE(settings.IsDefault());

  settings.SetCommand("CUSTOM_COMMAND");
  ASSERT_FALSE(settings.IsDefault());
}