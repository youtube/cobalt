// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/vlog.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace logging {

namespace {

class VlogTest : public testing::Test {
};

TEST_F(VlogTest, NoVmodule) {
  EXPECT_EQ(0, VlogInfo("", "").GetVlogLevel("test1"));
  EXPECT_EQ(0, VlogInfo("0", "").GetVlogLevel("test2"));
  EXPECT_EQ(0, VlogInfo("blah", "").GetVlogLevel("test3"));
  EXPECT_EQ(0, VlogInfo("0blah1", "").GetVlogLevel("test4"));
  EXPECT_EQ(1, VlogInfo("1", "").GetVlogLevel("test5"));
  EXPECT_EQ(5, VlogInfo("5", "").GetVlogLevel("test6"));
}

TEST_F(VlogTest, Vmodule) {
  const char kVSwitch[] = "-1";
  const char kVModuleSwitch[] =
      "foo=,bar=0,baz=blah,,qux=0blah1,quux=1,corge=5";
  VlogInfo vlog_info(kVSwitch, kVModuleSwitch);
  EXPECT_EQ(-1, vlog_info.GetVlogLevel("/path/to/grault.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/path/to/foo.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("D:\\Path\\To\\bar-inl.mm"));
  EXPECT_EQ(-1, vlog_info.GetVlogLevel("D:\\path\\to what/bar_unittest.m"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("baz.h"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/another/path/to/qux.h"));
  EXPECT_EQ(1, vlog_info.GetVlogLevel("/path/to/quux"));
  EXPECT_EQ(5, vlog_info.GetVlogLevel("c:\\path/to/corge.h"));
}

#define BENCHMARK(iters, elapsed, code)                         \
  do {                                                          \
    base::TimeTicks start = base::TimeTicks::Now();             \
    for (int i = 0; i < iters; ++i) code;                       \
    base::TimeTicks end = base::TimeTicks::Now();               \
    elapsed = end - start;                                      \
    double cps = iters / elapsed.InSecondsF();                  \
    LOG(INFO) << cps << " cps (" << elapsed.InSecondsF()        \
              << "s elapsed)";                                  \
  } while (0)

double GetSlowdown(const base::TimeDelta& base,
                   const base::TimeDelta& elapsed) {
  return elapsed.InSecondsF() / base.InSecondsF();
}


TEST_F(VlogTest, Perf) {
  const char* kVlogs[] = {
    "/path/to/foo.cc",
    "C:\\path\\to\\bar.h",
    "/path/to/not-matched.mm",
    "C:\\path\\to\\baz-inl.mm",
    "C:\\path\\to\\qux.mm",
    "/path/to/quux.mm",
    "/path/to/another-not-matched.mm",
  };
  const int kVlogCount = arraysize(kVlogs);

  base::TimeDelta null_elapsed;
  {
    VlogInfo null_vlog_info("", "");
    BENCHMARK(10000000, null_elapsed, {
      EXPECT_NE(-1, null_vlog_info.GetVlogLevel(kVlogs[i % kVlogCount]));
    });
  }

  {
    VlogInfo small_vlog_info("0", "foo=1,bar=2,baz=3,qux=4,quux=5");
    base::TimeDelta elapsed;
    BENCHMARK(10000000, elapsed, {
      EXPECT_NE(-1, small_vlog_info.GetVlogLevel(kVlogs[i % kVlogCount]));
    });
    LOG(INFO) << "slowdown = " << GetSlowdown(null_elapsed, elapsed)
              << "x";
  }

  {
    VlogInfo pattern_vlog_info("0", "fo*=1,ba?=2,b*?z=3,*ux=4,?uux=5");
    base::TimeDelta elapsed;
    BENCHMARK(10000000, elapsed, {
      EXPECT_NE(-1, pattern_vlog_info.GetVlogLevel(kVlogs[i % kVlogCount]));
    });
    LOG(INFO) << "slowdown = " << GetSlowdown(null_elapsed, elapsed)
              << "x";
  }
}

#undef BENCHMARK

}  // namespace

}  // namespace logging
