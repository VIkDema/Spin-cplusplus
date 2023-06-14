#include <gtest/gtest.h>

#include "../src/main/pan_processor.hpp"

TEST(PanProcessorTest, AddRuntimeTest) {
  PanProcessor pan;

  pan.AddRuntime("-biterate3");
  EXPECT_EQ(3, pan.GetItsr());

  pan.AddRuntime("-swarm7");
  EXPECT_EQ(-7, pan.GetItsr());
}

TEST(PanProcessorTest, SetItsr_nTest) {
  PanProcessor pan;

  pan.SetItsr_n("-swarm12,3");
  EXPECT_EQ(3, pan.GetItsrN());
}

TEST(PanProcessorTest, AddComptimeTest) {
  PanProcessor pan;

  pan.AddComptime("-DBFS");
  EXPECT_EQ(" -DBFS", pan.GetPanComptime());

  pan.AddComptime("-DBFS");
  EXPECT_EQ(" -DBFS", pan.GetPanComptime());
}