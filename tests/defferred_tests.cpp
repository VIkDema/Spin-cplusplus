#include <gtest/gtest.h>

#include "../src/lexer/deferred.hpp"

TEST(DeferredTest, Constructor) {
  helpers::Deferred deferred;
  EXPECT_FALSE(deferred.IsDiferred());
}

TEST(DeferredTest, SetGetDeferred) {
  helpers::Deferred deferred;
  
  deferred.SetDiferred(true);
  EXPECT_TRUE(deferred.IsDiferred());
  
  deferred.SetDiferred(false);
  EXPECT_FALSE(deferred.IsDiferred());
}

TEST(DeferredTest, GetDeferred_NoDeferFd) {
  helpers::Deferred deferred;
  
  bool result = deferred.GetDeferred();
  EXPECT_FALSE(result);
}