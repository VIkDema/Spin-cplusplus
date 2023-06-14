#include <gtest/gtest.h>

#include "../src/lexer/line_number.hpp"

// Test the Inc function
TEST(LineNumberTest, Inc) {
  
  file::LineNumber::Inc();
  EXPECT_EQ(file::LineNumber::Get(), 1);
  
  file::LineNumber::Inc();
  EXPECT_EQ(file::LineNumber::Get(), 2);
}

// Test the Set and Get functions
TEST(LineNumberTest, SetGet) {
  
  file::LineNumber::Set(10);
  EXPECT_EQ(file::LineNumber::Get(), 10);
  
  file::LineNumber::Set(20);
  EXPECT_EQ(file::LineNumber::Get(), 20);
}
