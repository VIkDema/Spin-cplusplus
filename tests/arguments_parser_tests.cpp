#include <gtest/gtest.h>

#include "../src/main/arguments_parser.hpp"

TEST(ArgumentsParserTest, NoArguments) {
  int argc1 = 1;
  char *argv1[] = {
      (char *)"program",
  };
  char **argv1_ptr = (char **)argv1;
  ArgumentsParser parser1;
  LaunchSettings result1 = parser1.Parse(argc1, argv1_ptr);
  EXPECT_FALSE(result1.need_export_ast);
  EXPECT_FALSE(result1.need_to_analyze);
}

TEST(ArgumentsParserTest, SingleArgument) {
  // Test case 2: Single argument
  int argc2 = 2;
  char *argv2[] = {(char *)"program", (char *)"-a"};
  char **argv2_ptr = (char **)argv2;
  ArgumentsParser parser2;
  LaunchSettings result2 = parser2.Parse(argc2, argv2_ptr);
  EXPECT_FALSE(result2.need_export_ast);
  EXPECT_TRUE(result2.need_to_analyze);
}

TEST(ArgumentsParserTest, MultipleArguments) {
  // Test case 3: Multiple arguments
  int argc3 = 4;
  char *argv3[] = {(char *)"program", (char *)"-a", (char *)"-b", (char *)"-c"};
  char **argv3_ptr = (char **)argv3;
  ArgumentsParser parser3;
  LaunchSettings result3 = parser3.Parse(argc3, argv3_ptr);
  EXPECT_FALSE(result3.need_export_ast);
  EXPECT_TRUE(result3.need_to_analyze);
  EXPECT_TRUE(result3.need_dont_execute_printfs_in_sumulation);
  EXPECT_TRUE(result3.need_columnated_output);
}