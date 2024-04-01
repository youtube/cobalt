// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/sample_to_group_iterator.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/cxx17_backports.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp4 {

namespace {
const SampleToGroupEntry kCompactSampleToGroupTable[] =
    {{10, 8}, {9, 5}, {25, 7}, {48, 63}, {8, 2}};
}  // namespace

class SampleToGroupIteratorTest : public testing::Test {
 public:
  SampleToGroupIteratorTest() {
    // Build sample group description index table from kSampleToGroupTable.
    for (size_t i = 0; i < base::size(kCompactSampleToGroupTable); ++i) {
      for (uint32_t j = 0; j < kCompactSampleToGroupTable[i].sample_count;
           ++j) {
        sample_to_group_table_.push_back(
            kCompactSampleToGroupTable[i].group_description_index);
      }
    }

    sample_to_group_.entries.assign(
        kCompactSampleToGroupTable,
        kCompactSampleToGroupTable + base::size(kCompactSampleToGroupTable));
    sample_to_group_iterator_.reset(
        new SampleToGroupIterator(sample_to_group_));
  }

 protected:
  std::vector<uint32_t> sample_to_group_table_;
  SampleToGroup sample_to_group_;
  std::unique_ptr<SampleToGroupIterator> sample_to_group_iterator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SampleToGroupIteratorTest);
};

TEST_F(SampleToGroupIteratorTest, EmptyTable) {
  SampleToGroup sample_to_group;
  SampleToGroupIterator iterator(sample_to_group);
  EXPECT_FALSE(iterator.IsValid());
}

TEST_F(SampleToGroupIteratorTest, Advance) {
  ASSERT_EQ(sample_to_group_table_[0],
            sample_to_group_iterator_->group_description_index());
  for (uint32_t sample = 1; sample < sample_to_group_table_.size(); ++sample) {
    ASSERT_TRUE(sample_to_group_iterator_->Advance());
    ASSERT_EQ(sample_to_group_table_[sample],
              sample_to_group_iterator_->group_description_index());
    ASSERT_TRUE(sample_to_group_iterator_->IsValid());
  }
  ASSERT_FALSE(sample_to_group_iterator_->Advance());
  ASSERT_FALSE(sample_to_group_iterator_->IsValid());
}

}  // namespace mp4
}  // namespace media
