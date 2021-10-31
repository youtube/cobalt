// This header is to redirect Chromium gtest_prod consumer code to where
// Cobalt puts it. It is only used by V8 so far.

#ifndef THIRD_PARTY_GOOGLETEST_SRC_GOOGLETEST_INCLUDE_GTEST_GTEST_PROD_H_
#define THIRD_PARTY_GOOGLETEST_SRC_GOOGLETEST_INCLUDE_GTEST_GTEST_PROD_H_

// #include "testing/gtest/include/gtest/gtest_prod.h"

#define FRIEND_TEST(test_case_name, test_name)

#endif  // THIRD_PARTY_GOOGLETEST_SRC_GOOGLETEST_INCLUDE_GTEST_GTEST_PROD_H_