// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/expired_histograms_checker.h"

#include "base/metrics/metrics_hashes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(ExpiredHistogramsCheckerTests, BasicTest) {
  uint64_t expired_hashes[] = {1, 2, 3};
  size_t size = 3;
  std::string whitelist_str = "";
  ExpiredHistogramsChecker checker(expired_hashes, size, whitelist_str);

  EXPECT_TRUE(checker.ShouldRecord(0));
  EXPECT_FALSE(checker.ShouldRecord(3));
}

TEST(ExpiredHistogramsCheckerTests, WhitelistTest) {
  std::string hist1 = "hist1";
  std::string hist2 = "hist2";
  std::string hist3 = "hist3";
  std::string hist4 = "hist4";

  uint64_t expired_hashes[] = {base::HashMetricName(hist1),
                               base::HashMetricName(hist2)};
  size_t size = 2;
  std::string whitelist_str = hist2 + "," + hist4;
  ExpiredHistogramsChecker checker(expired_hashes, size, whitelist_str);

  EXPECT_FALSE(checker.ShouldRecord(base::HashMetricName(hist1)));
  EXPECT_TRUE(checker.ShouldRecord(base::HashMetricName(hist2)));
  EXPECT_TRUE(checker.ShouldRecord(base::HashMetricName(hist3)));
  EXPECT_TRUE(checker.ShouldRecord(base::HashMetricName(hist4)));
}

}  // namespace metrics