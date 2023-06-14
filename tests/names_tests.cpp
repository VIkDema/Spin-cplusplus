#include <gtest/gtest.h>

#include "../src/lexer/names.hpp"


TEST(TokensTest, ParseLtlToken) {
  std::string value1 = "U";
  std::string value2 = "always";
  std::string value3 = "invalid";

  std::optional<int> result1 = helpers::ParseLtlToken(value1);
  std::optional<int> result2 = helpers::ParseLtlToken(value2);
  std::optional<int> result3 = helpers::ParseLtlToken(value3);

  EXPECT_TRUE(result1.has_value());
  EXPECT_EQ(result1.value(), UNTIL);

  EXPECT_TRUE(result2.has_value());
  EXPECT_EQ(result2.value(), ALWAYS);

  EXPECT_FALSE(result3.has_value());
}

TEST(TokensTest, ParseNameToken) {
  std::string value1 = "active";
  std::string value2 = "bit";
  std::string value3 = "invalid";

  std::optional<helpers::Name> result1 = helpers::ParseNameToken(value1);
  std::optional<helpers::Name> result2 = helpers::ParseNameToken(value2);
  std::optional<helpers::Name> result3 = helpers::ParseNameToken(value3);

  EXPECT_TRUE(result1.has_value());
  EXPECT_EQ(result1.value().token, ACTIVE);

  EXPECT_TRUE(result2.has_value());
  EXPECT_EQ(result2.value().token, TYPE);
  EXPECT_EQ(result2.value().value, 1);

  EXPECT_FALSE(result3.has_value());
}