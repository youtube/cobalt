// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/sparse_histogram.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class SparseHistogramTest : public testing::Test {
 protected:
  scoped_ptr<SparseHistogram> NewSparseHistogram(const std::string& name) {
    return scoped_ptr<SparseHistogram>(new SparseHistogram(name));
  }
};

TEST_F(SparseHistogramTest, BasicTest) {
  scoped_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse1"));
  std::map<HistogramBase::Sample, HistogramBase::Count> sample;
  histogram->SnapshotSample(&sample);

  ASSERT_EQ(0u, sample.size());

  histogram->Add(100);
  histogram->SnapshotSample(&sample);
  ASSERT_EQ(1u, sample.size());
  EXPECT_EQ(1, sample[100]);

  histogram->Add(100);
  histogram->Add(101);
  histogram->SnapshotSample(&sample);
  ASSERT_EQ(2u, sample.size());
  EXPECT_EQ(2, sample[100]);
  EXPECT_EQ(1, sample[101]);
}

}  // namespace base
