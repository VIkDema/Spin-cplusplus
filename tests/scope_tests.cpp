#include <gtest/gtest.h>

#include "../src/lexer/scope.hpp"

class ScopeProcessorTest : public ::testing::Test {
protected:
  void SetUp() override {
    lexer::ScopeProcessor::InitScopeName();
    lexer::ScopeProcessor::SetCurrScopeLevel(0);
  }
};

TEST_F(ScopeProcessorTest, GetCurrScope_Initially_ReturnsUnderscore) {

  std::string currScope = lexer::ScopeProcessor::GetCurrScope();

  EXPECT_EQ(currScope, "_");
}

TEST_F(ScopeProcessorTest,
       AddScope_IncreasesScopeLevelAndUpdatesCurrScopeName) {

  lexer::ScopeProcessor::AddScope();
  std::string currScope = lexer::ScopeProcessor::GetCurrScope();
  int currSegment = lexer::ScopeProcessor::GetCurrSegment();
  int currScopeLevel = lexer::ScopeProcessor::GetCurrScopeLevel();

  EXPECT_EQ(currScope, "_");
  EXPECT_EQ(currSegment, 0);
  EXPECT_EQ(currScopeLevel, 1);
}

TEST_F(ScopeProcessorTest,
       RemoveScope_DecreasesScopeLevelAndUpdatesCurrScopeName) {
  lexer::ScopeProcessor::AddScope();

  lexer::ScopeProcessor::RemoveScope();
  std::string currScope = lexer::ScopeProcessor::GetCurrScope();
  int currSegment = lexer::ScopeProcessor::GetCurrSegment();
  int currScopeLevel = lexer::ScopeProcessor::GetCurrScopeLevel();

  EXPECT_EQ(currScope, "_");
  EXPECT_EQ(currSegment, 2);
  EXPECT_EQ(currScopeLevel, 0);
}

TEST_F(ScopeProcessorTest, SetCurrScopeLevel_ModifiesScopeLevel) {
  lexer::ScopeProcessor::SetCurrScopeLevel(3);
  int currScopeLevel = lexer::ScopeProcessor::GetCurrScopeLevel();

  EXPECT_EQ(currScopeLevel, 3);
}