// Copyright 2012 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/media/progressive/mp4_map.h"

#include <stdlib.h>  // for rand and srand

#include <algorithm>  // for std::min
#include <memory>
#include <set>
#include <sstream>
#include <vector>

#include "cobalt/media/base/endian_util.h"
#include "cobalt/media/progressive/mock_data_source_reader.h"
#include "cobalt/media/progressive/mp4_parser.h"
#include "starboard/memory.h"
#include "starboard/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::media::endian_util::load_uint32_big_endian;
using cobalt::media::endian_util::store_uint32_big_endian;
using cobalt::media::endian_util::store_uint64_big_endian;

using cobalt::media::kAtomType_co64;
using cobalt::media::kAtomType_ctts;
using cobalt::media::kAtomType_stco;
using cobalt::media::kAtomType_stsc;
using cobalt::media::kAtomType_stss;
using cobalt::media::kAtomType_stsz;
using cobalt::media::kAtomType_stts;
using cobalt::media::kEntrySize_co64;
using cobalt::media::kEntrySize_ctts;
using cobalt::media::kEntrySize_stco;
using cobalt::media::kEntrySize_stsc;
using cobalt::media::kEntrySize_stss;
using cobalt::media::kEntrySize_stsz;
using cobalt::media::kEntrySize_stts;
using cobalt::media::MockDataSourceReader;
using cobalt::media::MP4Map;

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Ge;
using ::testing::Invoke;
using ::testing::Lt;
using ::testing::Return;
using ::testing::SetArrayArgument;

namespace {

int RandomRange(int min, int max) { return min + rand() % (max - min + 1); }

// Data structure represent a sample inside stbl. It has redundant data for
// easy access.
struct Sample {
  bool is_key_frame;
  int size;
  int offset;
  int chunk_index;
  int dts_duration;
  int dts;
  int cts;
};

typedef std::vector<Sample> SampleVector;

class SampleTable {
 public:
  // All ranges are inclusive at both ends.
  // Set range of composition timestamp to [0, 0] to disable ctts.
  SampleTable(unsigned int seed, int num_of_samples, int min_sample_size,
              int max_sample_size, int min_samples_per_chunk,
              int max_samples_per_chunk, int min_key_frame_gap,
              int max_key_frame_gap, int min_sample_decode_timestamp_offset,
              int max_sample_decode_timestamp_offset,
              int min_sample_composition_timestamp_offset,
              int max_sample_composition_timestamp_offset)
      : read_count_(0), read_bytes_(0) {
    srand(seed);
    CHECK_GT(num_of_samples, 0);
    CHECK_GT(min_sample_size, 0);
    CHECK_LE(min_sample_size, max_sample_size);
    CHECK_GT(min_samples_per_chunk, 0);
    CHECK_LE(min_samples_per_chunk, max_samples_per_chunk);
    CHECK_GT(min_key_frame_gap, 0);
    CHECK_LE(min_key_frame_gap, max_key_frame_gap);
    CHECK_GT(min_sample_decode_timestamp_offset, 0);
    CHECK_LE(min_sample_decode_timestamp_offset,
             max_sample_decode_timestamp_offset);
    CHECK_LE(min_sample_composition_timestamp_offset,
             max_sample_composition_timestamp_offset);

    samples_.resize(num_of_samples);

    int remaining_sample_in_chunk = 0;
    int current_chunk_index = -1;
    int next_key_frame = 0;

    for (int i = 0; i < num_of_samples; ++i) {
      samples_[i].size = RandomRange(min_sample_size, max_sample_size);
      samples_[i].offset =
          i == 0 ? rand() : samples_[i - 1].offset + samples_[i - 1].size;
      samples_[i].dts =
          i == 0 ? 0 : samples_[i - 1].dts + samples_[i - 1].dts_duration;
      samples_[i].dts_duration =
          RandomRange(min_sample_decode_timestamp_offset,
                      max_sample_decode_timestamp_offset);

      samples_[i].cts = samples_[i].dts +
                        RandomRange(min_sample_composition_timestamp_offset,
                                    max_sample_composition_timestamp_offset);
      if (!remaining_sample_in_chunk) {
        ++current_chunk_index;
        remaining_sample_in_chunk =
            RandomRange(min_samples_per_chunk, max_samples_per_chunk);
      }

      if (i >= next_key_frame) {
        samples_[i].is_key_frame = true;
        next_key_frame += RandomRange(min_key_frame_gap, max_key_frame_gap);
      } else {
        samples_[i].is_key_frame = false;
      }

      samples_[i].chunk_index = current_chunk_index;
      --remaining_sample_in_chunk;
    }

    PopulateBoxes();
  }

  int64 GetBoxOffset(uint32 atom_type) const {
    switch (atom_type) {
      case kAtomType_stsz:
        return stsz_offset_;
      case kAtomType_stco:
        return stco_offset_;
      case kAtomType_co64:
        return co64_offset_;
      case kAtomType_stsc:
        return stsc_offset_;
      case kAtomType_ctts:
        return ctts_offset_;
      case kAtomType_stts:
        return stts_offset_;
      case kAtomType_stss:
        return stss_offset_;
      default:
        NOTREACHED();
        return 0;
    }
  }

  int64 GetBoxSize(uint32 atom_type) const {
    switch (atom_type) {
      case kAtomType_stsz:
        return stsz_.size();
      case kAtomType_stco:
        return stco_.size();
      case kAtomType_co64:
        return co64_.size();
      case kAtomType_stsc:
        return stsc_.size();
      case kAtomType_ctts:
        return ctts_.size();
      case kAtomType_stts:
        return stts_.size();
      case kAtomType_stss:
        return stss_.size();
      default:
        NOTREACHED();
        return 0;
    }
  }

  const uint8_t* GetBoxData(uint32 atom_type) const {
    return &combined_[0] + GetBoxOffset(atom_type) - file_offset_;
  }

  size_t sample_count() const { return samples_.size(); }
  const Sample& sample(int i) const { return samples_.at(i); }

  size_t keyframe_count() const { return (stss_.size() - 8) / kEntrySize_stss; }

  int BlockingRead(int64 position, int size, uint8* data) {
    CHECK_GE(position, file_offset_);
    CHECK_LE(position + size, file_offset_ + combined_.size());
    uint32 offset = position - file_offset_;
    memcpy(data, &combined_[0] + offset, size);
    ++read_count_;
    read_bytes_ += size;
    return size;
  }

  void ClearReadStatistics() {
    read_count_ = 0;
    read_bytes_ = 0;
  }

  int read_count() const { return read_count_; }
  int read_bytes() const { return read_bytes_; }

  void Dump() const {
    std::stringstream ss;
    uint32 boxes[] = {kAtomType_stsz, kAtomType_stco, kAtomType_co64,
                      kAtomType_stsc, kAtomType_ctts, kAtomType_stts,
                      kAtomType_stss};
    const char* names[] = {"stsz", "stco", "co64", "stsc",
                           "ctts", "stts", "stss"};
    for (uint32 i = 0; i < sizeof(boxes) / sizeof(*boxes); ++i) {
      ss << "\n======================== " << names[i]
         << " ========================\n";
      int64 size = GetBoxSize(boxes[i]);
      const uint8_t* data = GetBoxData(boxes[i]);
      for (int64 j = 0; j < size; ++j) {
        ss << static_cast<unsigned int>(data[j]) << ' ';
        if (j != 0 && j % 32 == 0) ss << '\n';
      }
    }
    LOG(INFO) << ss.str();
  }

 private:
  SampleVector samples_;

  int64 file_offset_;
  int64 stsz_offset_;
  int64 stco_offset_;
  int64 co64_offset_;
  int64 stsc_offset_;
  int64 ctts_offset_;
  int64 stts_offset_;
  int64 stss_offset_;

  std::vector<uint8_t> combined_;
  std::vector<uint8_t> stsz_;
  std::vector<uint8_t> stco_;
  std::vector<uint8_t> co64_;
  std::vector<uint8_t> stsc_;
  std::vector<uint8_t> ctts_;
  std::vector<uint8_t> stts_;
  std::vector<uint8_t> stss_;

  int read_count_;
  int read_bytes_;

  static void inc_uint32_big_endian(uint8_t* data) {
    uint32 value = load_uint32_big_endian(data);
    store_uint32_big_endian(value + 1, data);
  }

  void PopulateBoxes() {
    CHECK(!samples_.empty());
    bool all_key_frames = true;
    bool all_ctts_offset_is_zero = true;
    bool all_sample_has_same_size = true;

    for (SampleVector::const_iterator iter = samples_.begin();
         iter != samples_.end(); ++iter) {
      if (!iter->is_key_frame) all_key_frames = false;
      if (iter->dts != iter->cts) all_ctts_offset_is_zero = false;
      if (iter->size != samples_[0].size) all_sample_has_same_size = false;
    }

    // populate the stsz box: 4 bytes flags + 4 bytes default size
    //     + 4 bytes count + size table if default size is 0
    if (all_sample_has_same_size) {
      stsz_.resize(12);
      store_uint32_big_endian(samples_[0].size, &stsz_[4]);
    } else {
      stsz_.resize(samples_.size() * kEntrySize_stsz + 12);
      store_uint32_big_endian(0, &stsz_[4]);

      uint8_t* table_offset = &stsz_[12];

      for (SampleVector::const_iterator iter = samples_.begin();
           iter != samples_.end(); ++iter) {
        store_uint32_big_endian(iter->size, table_offset);
        table_offset += kEntrySize_stsz;
      }
    }
    store_uint32_big_endian(samples_.size(), &stsz_[8]);

    // populate stco, co64 and stsc
    // stco = 4 bytes count + (4 bytes offset)*
    // co64 = 4 bytes count + (8 bytes offset)*
    // stsc = 4 bytes count + (4 bytes chunk index + 4 bytes sample per chunk
    //                         + 4 bytes sample description index)*
    stco_.resize(8);
    co64_.resize(8);
    stsc_.resize(8);
    uint32 chunk_offset = samples_[0].offset;
    int current_chunk_index = -1;
    for (SampleVector::const_iterator iter = samples_.begin();
         iter != samples_.end(); ++iter) {
      if (current_chunk_index != iter->chunk_index) {
        current_chunk_index = iter->chunk_index;
        stco_.resize(stco_.size() + kEntrySize_stco);
        store_uint32_big_endian(chunk_offset,
                                &stco_[stco_.size() - kEntrySize_stco]);
        co64_.resize(co64_.size() + kEntrySize_co64);
        store_uint64_big_endian(chunk_offset,
                                &co64_[co64_.size() - kEntrySize_co64]);
        stsc_.resize(stsc_.size() + kEntrySize_stsc);
        store_uint32_big_endian(current_chunk_index + 1,  // start from 1
                                &stsc_[stsc_.size() - kEntrySize_stsc]);
      }
      inc_uint32_big_endian(&stsc_[stsc_.size() - kEntrySize_stsc + 4]);
      chunk_offset += iter->size;
    }
    store_uint32_big_endian((stco_.size() - 8) / kEntrySize_stco, &stco_[4]);
    store_uint32_big_endian((co64_.size() - 8) / kEntrySize_co64, &co64_[4]);
    store_uint32_big_endian((stsc_.size() - 8) / kEntrySize_stsc, &stsc_[4]);

    // populate stts and ctts.
    // stts = 4 bytes count + (4 bytes sample count + 4 bytes sample delta)*
    // ctts = 4 bytes count + (4 bytes sample count + 4 bytes sample offset)*
    //        the offset is to stts.
    stts_.resize(8);
    ctts_.resize(all_ctts_offset_is_zero ? 0 : 8);
    int32 last_stts_duration = -1;
    int32 last_ctts_offset = samples_[0].cts - samples_[0].dts - 1;

    for (size_t i = 0; i < samples_.size(); ++i) {
      int32 ctts_offset = samples_[i].cts - samples_[i].dts;
      if (last_stts_duration != samples_[i].dts_duration) {
        stts_.resize(stts_.size() + kEntrySize_stts);
        last_stts_duration = samples_[i].dts_duration;
        store_uint32_big_endian(last_stts_duration, &stts_[stts_.size() - 4]);
      }
      inc_uint32_big_endian(&stts_[stts_.size() - 8]);
      if (!all_ctts_offset_is_zero) {
        if (last_ctts_offset != ctts_offset) {
          ctts_.resize(ctts_.size() + kEntrySize_ctts);
          store_uint32_big_endian(ctts_offset, &ctts_[ctts_.size() - 4]);
          last_ctts_offset = ctts_offset;
        }
        inc_uint32_big_endian(&ctts_[ctts_.size() - 8]);
      }
    }
    store_uint32_big_endian((stts_.size() - 8) / kEntrySize_stts, &stts_[4]);
    if (!all_ctts_offset_is_zero)
      store_uint32_big_endian((ctts_.size() - 8) / kEntrySize_ctts, &ctts_[4]);

    // populate stss box
    // stss = 4 bytes count + (4 bytes sample index)*
    if (!all_key_frames) {
      stss_.resize(8);
      for (size_t i = 0; i < samples_.size(); ++i) {
        if (samples_[i].is_key_frame) {
          stss_.resize(stss_.size() + kEntrySize_stss);
          store_uint32_big_endian(i + 1,
                                  &stss_[stss_.size() - kEntrySize_stss]);
        }
      }
      store_uint32_big_endian((stss_.size() - 8) / kEntrySize_stss, &stss_[4]);
    }

    const int kGarbageSize = 1024;
    std::vector<uint8_t> garbage;
    garbage.reserve(kGarbageSize);
    for (int i = 0; i < kGarbageSize; ++i)
      garbage.push_back(RandomRange(0xef, 0xfe));
    combined_.insert(combined_.end(), garbage.begin(), garbage.end());
    combined_.insert(combined_.end(), stsz_.begin(), stsz_.end());
    combined_.insert(combined_.end(), garbage.begin(), garbage.end());
    combined_.insert(combined_.end(), stco_.begin(), stco_.end());
    combined_.insert(combined_.end(), garbage.begin(), garbage.end());
    combined_.insert(combined_.end(), co64_.begin(), co64_.end());
    combined_.insert(combined_.end(), garbage.begin(), garbage.end());
    combined_.insert(combined_.end(), stsc_.begin(), stsc_.end());
    combined_.insert(combined_.end(), garbage.begin(), garbage.end());
    combined_.insert(combined_.end(), ctts_.begin(), ctts_.end());
    combined_.insert(combined_.end(), garbage.begin(), garbage.end());
    combined_.insert(combined_.end(), stts_.begin(), stts_.end());
    combined_.insert(combined_.end(), garbage.begin(), garbage.end());
    combined_.insert(combined_.end(), stss_.begin(), stss_.end());
    combined_.insert(combined_.end(), garbage.begin(), garbage.end());

    file_offset_ = abs(rand());
    stsz_offset_ = file_offset_ + kGarbageSize;
    stco_offset_ = stsz_offset_ + stsz_.size() + kGarbageSize;
    co64_offset_ = stco_offset_ + stco_.size() + kGarbageSize;
    stsc_offset_ = co64_offset_ + co64_.size() + kGarbageSize;
    ctts_offset_ = stsc_offset_ + stsc_.size() + kGarbageSize;
    stts_offset_ = ctts_offset_ + ctts_.size() + kGarbageSize;
    stss_offset_ = stts_offset_ + stts_.size() + kGarbageSize;
  }
};

class MP4MapTest : public testing::Test {
 protected:
  MP4MapTest() {
    // make a new mock reader
    reader_ = new ::testing::NiceMock<MockDataSourceReader>();
    // make a new map with a mock reader.
    map_ = new MP4Map(reader_);
  }

  virtual ~MP4MapTest() {
    DCHECK(map_->HasOneRef());
    map_ = NULL;

    reader_->Stop();
    DCHECK(reader_->HasOneRef());
    reader_ = NULL;
  }

  void ResetMap() { map_ = new MP4Map(reader_); }

  void CreateTestSampleTable(unsigned int seed, int num_of_samples,
                             int min_sample_size, int max_sample_size,
                             int min_samples_per_chunk,
                             int max_samples_per_chunk, int min_key_frame_gap,
                             int max_key_frame_gap,
                             int min_sample_decode_timestamp_offset,
                             int max_sample_decode_timestamp_offset,
                             int min_sample_composition_timestamp_offset,
                             int max_sample_composition_timestamp_offset) {
    sample_table_.reset(new SampleTable(
        seed, num_of_samples, min_sample_size, max_sample_size,
        min_samples_per_chunk, max_samples_per_chunk, min_key_frame_gap,
        max_key_frame_gap, min_sample_decode_timestamp_offset,
        max_sample_decode_timestamp_offset,
        min_sample_composition_timestamp_offset,
        max_sample_composition_timestamp_offset));
    ON_CALL(*reader_, BlockingRead(_, _, _))
        .WillByDefault(Invoke(sample_table_.get(), &SampleTable::BlockingRead));
  }

  void SetTestTable(uint32 four_cc, uint32 cache_size_entries) {
    map_->SetAtom(four_cc, sample_table_->GetBoxOffset(four_cc),
                  sample_table_->GetBoxSize(four_cc), cache_size_entries,
                  sample_table_->GetBoxData(four_cc));
  }

  const Sample& GetTestSample(uint32 sample_number) const {
    return sample_table_->sample(sample_number);
  }

  // ==== Test Fixture Members
  scoped_refptr<MP4Map> map_;
  scoped_refptr<MockDataSourceReader> reader_;
  std::unique_ptr<SampleTable> sample_table_;
};

// ==== SetAtom() Tests ========================================================
/*
TEST_F(MP4MapTest, SetAtomWithZeroDefaultSize) {
  // SetAtom() should fail with a zero default size on an stsc.
  NOTIMPLEMENTED();
}
*/
// ==== GetSize() Tests ========================================================

TEST_F(MP4MapTest, GetSizeWithDefaultSize) {
  CreateTestSampleTable(100, 1000, 0xb0df00d, 0xb0df00d, 5, 10, 5, 10, 10, 20,
                        10, 20);
  sample_table_->ClearReadStatistics();

  for (int i = 13; i < 21; ++i) {
    ResetMap();
    SetTestTable(kAtomType_stsz, i);

    uint32 returned_size;
    ASSERT_TRUE(map_->GetSize(0, &returned_size));
    ASSERT_EQ(returned_size, 0xb0df00d);
    ASSERT_FALSE(map_->GetSize(2000, &returned_size));
    ASSERT_TRUE(map_->GetSize(2, &returned_size));
    ASSERT_EQ(returned_size, 0xb0df00d);
    ASSERT_TRUE(map_->GetSize(120, &returned_size));
    ASSERT_EQ(returned_size, 0xb0df00d);
  }

  ASSERT_EQ(sample_table_->read_count(), 0);
}

TEST_F(MP4MapTest, GetSizeIterationWithHugeCache) {
  for (int max_sample_size = 10; max_sample_size < 20; ++max_sample_size) {
    CreateTestSampleTable(200 + max_sample_size, 1000, 10, max_sample_size, 5,
                          10, 5, 10, 10, 20, 10, 20);
    for (int i = 1500; i < 10000; i = i * 2 + 1) {
      ResetMap();
      sample_table_->ClearReadStatistics();
      SetTestTable(kAtomType_stsz, i);

      for (uint32 j = 0; j < sample_table_->sample_count(); j++) {
        uint32 map_reported_size = 0;
        ASSERT_TRUE(map_->GetSize(j, &map_reported_size));
        uint32 table_size = GetTestSample(j).size;
        // reported size should match table size
        ASSERT_EQ(map_reported_size, table_size);
      }

      // call to a sample past the size of the table should fail
      uint32 failed_size = 0;
      ASSERT_FALSE(map_->GetSize(sample_table_->sample_count(), &failed_size));
      ASSERT_LE(sample_table_->read_count(), 1);
      ASSERT_LE(sample_table_->read_bytes(),
                sample_table_->GetBoxSize(kAtomType_stsz));
    }
  }
}

TEST_F(MP4MapTest, GetSizeIterationTinyCache) {
  for (int max_sample_size = 10; max_sample_size < 20; ++max_sample_size) {
    CreateTestSampleTable(300 + max_sample_size, 1000, 10, max_sample_size, 5,
                          10, 5, 10, 10, 20, 10, 20);
    for (int i = 5; i < 12; ++i) {
      ResetMap();
      SetTestTable(kAtomType_stsz, i);
      sample_table_->ClearReadStatistics();
      for (uint32 j = 0; j < sample_table_->sample_count(); j++) {
        uint32 map_reported_size = 0;
        ASSERT_TRUE(map_->GetSize(j, &map_reported_size));
        uint32 table_size = GetTestSample(j).size;
        ASSERT_EQ(map_reported_size, table_size);
      }
      ASSERT_LE(sample_table_->read_count(),
                sample_table_->sample_count() / i + 1);
      if (sample_table_->read_count())
        ASSERT_LE(sample_table_->read_bytes() / sample_table_->read_count(),
                  (i + 1) * kEntrySize_stsz);
      // call to sample past the table size should still fail
      uint32 failed_size = 0;
      ASSERT_FALSE(map_->GetSize(sample_table_->sample_count(), &failed_size));
    }
  }
}

TEST_F(MP4MapTest, GetSizeRandomAccess) {
  CreateTestSampleTable(101, 2000, 20, 24, 5, 10, 5, 10, 10, 20, 10, 20);
  for (int i = 24; i < 27; ++i) {
    ResetMap();
    SetTestTable(kAtomType_stsz, i);
    sample_table_->ClearReadStatistics();
    // test first sample query somewhere later in the table, sample 105
    uint32 map_reported_size = 0;
    ASSERT_TRUE(map_->GetSize(i * 4 + 5, &map_reported_size));
    uint32 table_size = GetTestSample(i * 4 + 5).size;
    ASSERT_EQ(map_reported_size, table_size);
    ASSERT_EQ(sample_table_->read_count(), 1);
    ASSERT_LE(sample_table_->read_bytes(), (i + 1) * kEntrySize_stsz);

    // now jump back to sample 0
    sample_table_->ClearReadStatistics();
    ASSERT_TRUE(map_->GetSize(0, &map_reported_size));
    table_size = GetTestSample(0).size;
    ASSERT_EQ(map_reported_size, table_size);
    ASSERT_EQ(sample_table_->read_count(), 1);
    ASSERT_LE(sample_table_->read_bytes(), (i + 1) * kEntrySize_stsz);

    // now seek well past the end, this query should fail but not break
    // subsequent queries or issue a recache
    ASSERT_FALSE(
        map_->GetSize(sample_table_->sample_count(), &map_reported_size));

    // a query back within the first table should not cause recache
    ASSERT_TRUE(map_->GetSize(10, &map_reported_size));
    table_size = GetTestSample(10).size;
    ASSERT_EQ(map_reported_size, table_size);

    // check sample queries right on cache boundaries out-of-order
    sample_table_->ClearReadStatistics();
    ASSERT_TRUE(map_->GetSize(2 * i, &map_reported_size));
    table_size = GetTestSample(2 * i).size;
    ASSERT_EQ(map_reported_size, table_size);
    ASSERT_EQ(sample_table_->read_count(), 1);
    ASSERT_TRUE(map_->GetSize(2 * i - 1, &map_reported_size));
    table_size = GetTestSample(2 * i - 1).size;
    ASSERT_EQ(map_reported_size, table_size);
    ASSERT_TRUE(map_->GetSize(2 * i - 2, &map_reported_size));
    table_size = GetTestSample(2 * i - 2).size;
    ASSERT_EQ(map_reported_size, table_size);
    ASSERT_EQ(sample_table_->read_count(), 2);
  }
}

// ==== GetOffset() Tests ======================================================

TEST_F(MP4MapTest, GetOffsetIterationHugeCache) {
  for (int coindex = 0; coindex < 2; ++coindex) {
    CreateTestSampleTable(102 + coindex, 1000, 20, 25, 5, 10, 5, 10, 10, 20, 10,
                          20);
    ResetMap();
    SetTestTable(kAtomType_stsz, 1000);
    SetTestTable(kAtomType_stsc, 1000);
    SetTestTable(coindex ? kAtomType_stco : kAtomType_co64, 1000);

    // no expectations on reader_, all tables should now be in memory
    for (uint32 i = 0; i < sample_table_->sample_count(); ++i) {
      uint64 map_reported_offset = 0;
      ASSERT_TRUE(map_->GetOffset(i, &map_reported_offset));
      uint64 table_offset = GetTestSample(i).offset;
      ASSERT_EQ(map_reported_offset, table_offset);
    }

    // calls to sample numbers outside file range should fail non-fatally
    uint64 failed_offset;
    ASSERT_FALSE(
        map_->GetOffset(sample_table_->sample_count(), &failed_offset));
  }
}

TEST_F(MP4MapTest, GetOffsetIterationTinyCache) {
  for (int coindex = 0; coindex < 2; ++coindex) {
    CreateTestSampleTable(103, 30, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
    for (int i = 1; i < 12; ++i) {
      ResetMap();
      SetTestTable(kAtomType_stsz, i);
      SetTestTable(kAtomType_stsc, i);
      SetTestTable(coindex ? kAtomType_stco : kAtomType_co64, i);

      // iterate through all samples in range
      for (uint32 j = 0; j < sample_table_->sample_count(); j += 2) {
        uint64 map_reported_offset = 0;
        ASSERT_TRUE(map_->GetOffset(j, &map_reported_offset));
        uint64 table_offset = GetTestSample(j).offset;
        ASSERT_EQ(map_reported_offset, table_offset);
      }

      // calls to sample numbers outside file range should fail non-fatally
      uint64 failed_offset;
      ASSERT_FALSE(
          map_->GetOffset(sample_table_->sample_count(), &failed_offset));
    }
  }
}

// Random access within cache should just result in correct re-integration
// through the stsc.
TEST_F(MP4MapTest, GetOffsetRandomAccessHugeCache) {
  for (int coindex = 0; coindex < 2; ++coindex) {
    CreateTestSampleTable(104, 300, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
    ResetMap();
    SetTestTable(kAtomType_stsz, 300);
    SetTestTable(kAtomType_stsc, 300);
    SetTestTable(coindex ? kAtomType_stco : kAtomType_co64, 300);

    for (int i = 0; i < 1000; ++i) {
      uint32 sample_number = rand() % sample_table_->sample_count();
      uint64 map_reported_offset = 0;
      ASSERT_TRUE(map_->GetOffset(sample_number, &map_reported_offset));
      uint64 table_offset = GetTestSample(sample_number).offset;
      ASSERT_EQ(map_reported_offset, table_offset);
    }
  }
}

// Random access across cache boundaries should not break computation of
// offsets.
TEST_F(MP4MapTest, GetOffsetRandomAccessTinyCache) {
  for (int coindex = 0; coindex < 2; ++coindex) {
    CreateTestSampleTable(105, 300, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
    ResetMap();
    SetTestTable(kAtomType_stsz, 7);
    SetTestTable(kAtomType_stsc, 7);
    SetTestTable(coindex ? kAtomType_stco : kAtomType_co64, 7);

    // calls to sample numbers outside file range should fail non-fatally
    uint64 failed_offset;
    ASSERT_FALSE(
        map_->GetOffset(sample_table_->sample_count(), &failed_offset));

    // second sample in the file
    uint32 sample_number = 1;
    uint64 map_reported_offset = 0;
    ASSERT_TRUE(map_->GetOffset(sample_number, &map_reported_offset));
    uint64 table_offset = GetTestSample(sample_number).offset;
    ASSERT_EQ(map_reported_offset, table_offset);

    for (int i = 1; i < 15; ++i) {
      ResetMap();
      SetTestTable(kAtomType_stsz, 7);
      SetTestTable(kAtomType_stsc, 7);
      SetTestTable(kAtomType_stco, 7);

      sample_number = sample_table_->sample_count() - i;
      ASSERT_TRUE(map_->GetOffset(sample_number, &map_reported_offset));
      table_offset = GetTestSample(sample_number).offset;
      ASSERT_EQ(map_reported_offset, table_offset);

      sample_number--;
      ASSERT_TRUE(map_->GetOffset(sample_number, &map_reported_offset));
      table_offset = GetTestSample(sample_number).offset;
      ASSERT_EQ(map_reported_offset, table_offset);

      // now iterate through a few samples in the middle
      sample_number /= 2;
      for (int j = 0; j < 40; j++) {
        ASSERT_TRUE(map_->GetOffset(sample_number + j, &map_reported_offset));
        table_offset = GetTestSample(sample_number + j).offset;
        ASSERT_EQ(map_reported_offset, table_offset);
      }

      // now iterate backwards from the same starting point
      for (int j = 0; j < 40; j++) {
        ASSERT_TRUE(map_->GetOffset(sample_number - j, &map_reported_offset));
        table_offset = GetTestSample(sample_number - j).offset;
        ASSERT_EQ(map_reported_offset, table_offset);
      }
    }
  }
}

TEST_F(MP4MapTest, GetOffsetRandomAccessWithDefaultSize) {
  for (int coindex = 0; coindex < 2; ++coindex) {
    CreateTestSampleTable(106, 300, 20, 20, 5, 10, 5, 10, 10, 20, 10, 20);
    ResetMap();
    SetTestTable(kAtomType_stsz, 7);
    SetTestTable(kAtomType_stsc, 7);
    SetTestTable(coindex ? kAtomType_stco : kAtomType_co64, 7);

    // Calculating offset of an out-of-range sample should still return an
    // error.
    uint64 map_reported_offset = 0;
    ASSERT_FALSE(map_->GetOffset(sample_table_->sample_count() + 2,
                                 &map_reported_offset));

    // First sample in file should still work, though.
    ASSERT_TRUE(map_->GetOffset(0, &map_reported_offset));
    uint64 table_offset = GetTestSample(0).offset;
    ASSERT_EQ(map_reported_offset, table_offset);

    // Last sample should also work.
    ASSERT_TRUE(map_->GetOffset(sample_table_->sample_count() - 1,
                                &map_reported_offset));
    table_offset = GetTestSample(sample_table_->sample_count() - 1).offset;
    ASSERT_EQ(map_reported_offset, table_offset);

    // Skip by 3 through the file a few times
    for (int i = 0; i < sample_table_->sample_count(); ++i) {
      int sample_index = (i * 3) % sample_table_->sample_count();
      ASSERT_TRUE(map_->GetOffset(sample_index, &map_reported_offset));
      table_offset = GetTestSample(sample_index).offset;
      ASSERT_EQ(map_reported_offset, table_offset);
    }
  }
}

// ==== GetDuration() Tests ====================================================

TEST_F(MP4MapTest, GetDurationIteration) {
  CreateTestSampleTable(107, 60, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  ResetMap();
  SetTestTable(kAtomType_stts, 2);

  for (uint32 i = 0; i < sample_table_->sample_count(); ++i) {
    uint32 map_reported_duration = 0;
    ASSERT_TRUE(map_->GetDuration(i, &map_reported_duration));
    uint32 table_duration = GetTestSample(i).dts_duration;
    ASSERT_EQ(map_reported_duration, table_duration);
  }

  // entries past end of table should fail
  uint32 failed_duration = 0;
  ASSERT_FALSE(
      map_->GetDuration(sample_table_->sample_count(), &failed_duration));
}

TEST_F(MP4MapTest, GetDurationRandomAccess) {
  CreateTestSampleTable(108, 60, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  ResetMap();
  SetTestTable(kAtomType_stts, 3);

  // first sample in table
  uint32 map_reported_duration = 0;
  ASSERT_TRUE(map_->GetDuration(0, &map_reported_duration));
  uint32 table_duration = GetTestSample(0).dts_duration;
  ASSERT_EQ(map_reported_duration, table_duration);

  // last sample in table
  ASSERT_TRUE(map_->GetDuration(sample_table_->sample_count() - 1,
                                &map_reported_duration));
  table_duration =
      GetTestSample(sample_table_->sample_count() - 1).dts_duration;
  ASSERT_EQ(map_reported_duration, table_duration);

  // sample just past end should fail
  ASSERT_FALSE(
      map_->GetDuration(sample_table_->sample_count(), &map_reported_duration));

  // but shouldn't break other sample lookups
  ASSERT_TRUE(map_->GetDuration(2, &map_reported_duration));
  table_duration = GetTestSample(2).dts_duration;
  ASSERT_EQ(map_reported_duration, table_duration);

  // now iterate backwards through entire table back to first sample
  for (int i = sample_table_->sample_count() - 1; i >= 1; i--) {
    ASSERT_TRUE(map_->GetDuration(i, &map_reported_duration));
    table_duration = GetTestSample(i).dts_duration;
    ASSERT_EQ(map_reported_duration, table_duration);
  }
}

// ==== GetTimestamp() Tests ===================================================

TEST_F(MP4MapTest, GetTimestampIterationNoCompositionTime) {
  CreateTestSampleTable(109, 60, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  ResetMap();
  SetTestTable(kAtomType_stts, 7);

  for (uint32 i = 0; i < sample_table_->sample_count(); ++i) {
    uint64 map_reported_timestamp = 0;
    ASSERT_TRUE(map_->GetTimestamp(i, &map_reported_timestamp));
    uint64 table_timestamp = GetTestSample(i).dts;
    ASSERT_EQ(map_reported_timestamp, table_timestamp);
  }

  // entries past end of table should fail
  uint64 failed_timestamp = 0;
  ASSERT_FALSE(
      map_->GetTimestamp(sample_table_->sample_count(), &failed_timestamp));
}

TEST_F(MP4MapTest, GetTimestampRandomAccessNoCompositionTime) {
  CreateTestSampleTable(110, 60, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  ResetMap();
  SetTestTable(kAtomType_stts, 10);

  // skip by sevens through the file, seven times
  for (int i = 0; i < sample_table_->sample_count(); ++i) {
    uint32 sample_number = (i * 7) % sample_table_->sample_count();
    uint64 map_reported_timestamp = 0;
    ASSERT_TRUE(map_->GetTimestamp(sample_number, &map_reported_timestamp));
    uint64 table_timestamp = GetTestSample(sample_number).dts;
    ASSERT_EQ(map_reported_timestamp, table_timestamp);
  }

  // check a failed entry
  uint64 failed_timestamp = 0;
  ASSERT_FALSE(
      map_->GetTimestamp(sample_table_->sample_count() * 2, &failed_timestamp));

  // should still be able to recover with valid input, this time skip by 21s
  // backward through the file 21 times
  for (int i = sample_table_->sample_count() - 1; i >= 0; i--) {
    uint32 sample_number = (i * 21) % sample_table_->sample_count();
    uint64 map_reported_timestamp = 0;
    ASSERT_TRUE(map_->GetTimestamp(sample_number, &map_reported_timestamp));
    uint64 table_timestamp = GetTestSample(sample_number).dts;
    ASSERT_EQ(map_reported_timestamp, table_timestamp);
  }
}

TEST_F(MP4MapTest, GetTimestampIteration) {
  CreateTestSampleTable(111, 300, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  for (int i = 1; i < 20; ++i) {
    ResetMap();
    SetTestTable(kAtomType_ctts, i);
    SetTestTable(kAtomType_stts, i);

    for (int j = 0; j < sample_table_->sample_count(); ++j) {
      uint64 map_reported_timestamp = 0;
      ASSERT_TRUE(map_->GetTimestamp(j, &map_reported_timestamp));
      uint64 table_timestamp = GetTestSample(j).cts;
      ASSERT_EQ(map_reported_timestamp, table_timestamp);

      uint32 map_reported_duration = 0;
      ASSERT_TRUE(map_->GetDuration(j, &map_reported_duration));
      uint32 table_duration = GetTestSample(j).dts_duration;
      ASSERT_EQ(map_reported_duration, table_duration);
    }
  }
}

TEST_F(MP4MapTest, GetTimestampRandomAccess) {
  CreateTestSampleTable(112, 300, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  for (int i = 1; i < 20; ++i) {
    ResetMap();
    SetTestTable(kAtomType_ctts, i);
    SetTestTable(kAtomType_stts, i);

    for (int j = 0; j < 100; ++j) {
      uint32 sample_number = rand() % sample_table_->sample_count();
      uint64 map_reported_timestamp = 0;
      ASSERT_TRUE(map_->GetTimestamp(sample_number, &map_reported_timestamp));
      uint64 table_timestamp = GetTestSample(sample_number).cts;
      ASSERT_EQ(map_reported_timestamp, table_timestamp);

      uint32 map_reported_duration = 0;
      ASSERT_TRUE(map_->GetDuration(sample_number, &map_reported_duration));
      uint32 table_duration = GetTestSample(sample_number).dts_duration;
      ASSERT_EQ(map_reported_duration, table_duration);
    }
  }
}

// ==== GetIsKeyframe() Tests ==================================================

// the map should consider every valid sample number a keyframe without an stss
TEST_F(MP4MapTest, GetIsKeyframeNoKeyframeTable) {
  ResetMap();
  bool is_keyframe_out = false;
  ASSERT_TRUE(map_->GetIsKeyframe(100, &is_keyframe_out));
  ASSERT_TRUE(is_keyframe_out);

  is_keyframe_out = false;
  ASSERT_TRUE(map_->GetIsKeyframe(5, &is_keyframe_out));
  ASSERT_TRUE(is_keyframe_out);

  for (int i = 17; i < 174; i += 3) {
    is_keyframe_out = false;
    ASSERT_TRUE(map_->GetIsKeyframe(i, &is_keyframe_out));
    ASSERT_TRUE(is_keyframe_out);
  }
}

TEST_F(MP4MapTest, GetIsKeyframeIteration) {
  CreateTestSampleTable(113, 1000, 0xb0df00d, 0xb0df00d, 5, 10, 5, 10, 10, 20,
                        10, 20);
  ResetMap();
  sample_table_->ClearReadStatistics();
  SetTestTable(kAtomType_stss, sample_table_->keyframe_count() / 2 + 5);

  for (uint32 i = 0; i < sample_table_->sample_count(); ++i) {
    bool map_is_keyframe_out = false;
    ASSERT_TRUE(map_->GetIsKeyframe(i, &map_is_keyframe_out));
    bool table_is_keyframe = GetTestSample(i).is_key_frame;
    ASSERT_EQ(map_is_keyframe_out, table_is_keyframe);
  }
}

TEST_F(MP4MapTest, GetIsKeyframeRandomAccess) {
  CreateTestSampleTable(114, 1000, 0xb0df00d, 0xb0df00d, 5, 10, 5, 10, 10, 20,
                        10, 20);
  ResetMap();
  sample_table_->ClearReadStatistics();
  SetTestTable(kAtomType_stss, sample_table_->keyframe_count() / 2 + 5);

  // pick a keyframe about halfway
  uint32 sample_number = sample_table_->sample_count() / 2;
  while (!GetTestSample(sample_number).is_key_frame) ++sample_number;
  // sample one past it should not be a keyframe
  bool map_is_keyframe_out = false;
  ASSERT_TRUE(map_->GetIsKeyframe(sample_number + 1, &map_is_keyframe_out));
  ASSERT_FALSE(map_is_keyframe_out);
  // sample one before keyframe should not be a keyframe either
  ASSERT_TRUE(map_->GetIsKeyframe(sample_number - 1, &map_is_keyframe_out));
  ASSERT_FALSE(map_is_keyframe_out);
  // however it should be a keyframe
  ASSERT_TRUE(map_->GetIsKeyframe(sample_number, &map_is_keyframe_out));
  ASSERT_TRUE(map_is_keyframe_out);

  // first keyframe
  sample_number = 0;
  while (!GetTestSample(sample_number).is_key_frame) ++sample_number;
  // next sample should not be a keyframe
  ASSERT_TRUE(map_->GetIsKeyframe(sample_number + 1, &map_is_keyframe_out));
  ASSERT_FALSE(map_is_keyframe_out);
  // but it should be
  ASSERT_TRUE(map_->GetIsKeyframe(sample_number, &map_is_keyframe_out));
  ASSERT_TRUE(map_is_keyframe_out);

  // iterate backwards from end of file to beginning
  for (int i = sample_table_->sample_count() - 1; i >= 0; --i) {
    ASSERT_TRUE(map_->GetIsKeyframe(i, &map_is_keyframe_out));
    ASSERT_EQ(map_is_keyframe_out, GetTestSample(i).is_key_frame);
  }

  // iterate backwards through keyframes only
  for (int i = sample_table_->sample_count() - 1; i >= 0; --i) {
    if (GetTestSample(i).is_key_frame) {
      ASSERT_TRUE(map_->GetIsKeyframe(i, &map_is_keyframe_out));
      ASSERT_TRUE(map_is_keyframe_out);
    }
  }

  // iterate forwards but skip all keyframes
  for (int i = sample_table_->sample_count() - 1; i >= 0; --i) {
    if (!GetTestSample(i).is_key_frame) {
      ASSERT_TRUE(map_->GetIsKeyframe(i, &map_is_keyframe_out));
      ASSERT_FALSE(map_is_keyframe_out);
    }
  }

  ResetMap();
  sample_table_->ClearReadStatistics();
  SetTestTable(kAtomType_stss, 7);

  // random access
  for (int i = 0; i < 1000; ++i) {
    sample_number = rand() % sample_table_->sample_count();
    ASSERT_TRUE(map_->GetIsKeyframe(sample_number, &map_is_keyframe_out));
    ASSERT_EQ(map_is_keyframe_out, GetTestSample(sample_number).is_key_frame);
  }
}

// ==== GetKeyframe() Tests ====================================================

// every frame should be returned as a keyframe. This tests if our computation
// of timestamps => sample numbers is equivalent to sample numbers => timestamps
TEST_F(MP4MapTest, GetKeyframeNoKeyframeTableIteration) {
  CreateTestSampleTable(115, 30, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  ResetMap();
  SetTestTable(kAtomType_stts, 7);

  for (int i = 0; i < sample_table_->sample_count(); ++i) {
    // get actual timestamp and duration of this sample
    uint64 sample_timestamp = GetTestSample(i).dts;
    uint32 sample_duration = GetTestSample(i).dts_duration;
    // add a bit of time to sample timestamp, but keep time within this frame
    sample_timestamp += i % sample_duration;
    uint32 map_keyframe = 0;
    ASSERT_TRUE(map_->GetKeyframe(sample_timestamp, &map_keyframe));
    ASSERT_EQ(map_keyframe, i);
  }
}

TEST_F(MP4MapTest, GetKeyframeNoKeyframeTableRandomAccess) {
  CreateTestSampleTable(116, 30, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  ResetMap();
  SetTestTable(kAtomType_stts, 5);

  // backwards through the middle third of samples
  for (int i = (sample_table_->sample_count() * 2) / 3;
       i >= sample_table_->sample_count() / 3; --i) {
    uint64 sample_timestamp = GetTestSample(i).dts;
    uint32 sample_duration = GetTestSample(i).dts_duration;
    sample_timestamp += sample_duration - 1 - (i % sample_duration);
    uint32 map_keyframe = 0;
    ASSERT_TRUE(map_->GetKeyframe(sample_timestamp, &map_keyframe));
    ASSERT_EQ(map_keyframe, i);
  }

  // highest valid timestamp in file
  uint64 highest_timestamp =
      GetTestSample(sample_table_->sample_count() - 1).dts;
  highest_timestamp +=
      GetTestSample(sample_table_->sample_count() - 1).dts_duration - 1;
  uint32 map_keyframe = 0;
  ASSERT_TRUE(map_->GetKeyframe(highest_timestamp, &map_keyframe));
  ASSERT_EQ(map_keyframe, sample_table_->sample_count() - 1);

  // lowest valid timestamp in file
  ASSERT_TRUE(map_->GetKeyframe(0, &map_keyframe));
  ASSERT_EQ(map_keyframe, 0);

  // should fail on higher timestamps
  ASSERT_FALSE(map_->GetKeyframe(highest_timestamp + 1, &map_keyframe));
}

// GetKeyframe is not normally called iteratively, so we test random access
TEST_F(MP4MapTest, GetKeyframe) {
  CreateTestSampleTable(117, 60, 20, 25, 5, 10, 5, 10, 10, 20, 10, 20);
  ResetMap();
  SetTestTable(kAtomType_stss, 3);
  SetTestTable(kAtomType_stts, 7);

  // find first keyframe in file, should be first frame
  uint32 map_keyframe = 0;
  ASSERT_TRUE(map_->GetKeyframe(0, &map_keyframe));
  ASSERT_EQ(map_keyframe, 0);

  // find a first quarter keyframe in file
  uint32 qtr_keyframe = sample_table_->sample_count() / 4;
  while (!GetTestSample(qtr_keyframe).is_key_frame) ++qtr_keyframe;
  uint32 next_keyframe = qtr_keyframe + 1;
  while (!GetTestSample(next_keyframe).is_key_frame) ++next_keyframe;
  uint32 prev_keyframe = qtr_keyframe - 1;
  while (!GetTestSample(prev_keyframe).is_key_frame) --prev_keyframe;
  uint32 last_keyframe = sample_table_->sample_count() - 1;
  while (!GetTestSample(last_keyframe).is_key_frame) --last_keyframe;
  // midway between this keyframe and the next one
  uint32 test_frame = qtr_keyframe + ((next_keyframe - qtr_keyframe) / 2);
  // get time for this frame
  uint64 test_frame_timestamp = GetTestSample(test_frame).dts;
  // get duration for this frame
  uint32 test_frame_duration = GetTestSample(test_frame).dts_duration;
  // midway through this frame
  test_frame_timestamp += test_frame_duration / 2;
  // find lower bound keyframe, should be qtr_keyframe
  ASSERT_TRUE(map_->GetKeyframe(test_frame_timestamp, &map_keyframe));
  ASSERT_EQ(map_keyframe, qtr_keyframe);

  // timestamp one tick before qtr_keyframe should find previous keyframe
  test_frame_timestamp = GetTestSample(qtr_keyframe).dts - 1;
  ASSERT_TRUE(map_->GetKeyframe(test_frame_timestamp, &map_keyframe));
  ASSERT_EQ(map_keyframe, prev_keyframe);

  // very highest timestamp in file should return last keyframe
  uint64 highest_timestamp =
      GetTestSample(sample_table_->sample_count() - 1).dts;
  highest_timestamp +=
      GetTestSample(sample_table_->sample_count() - 1).dts_duration - 1;
  ASSERT_TRUE(map_->GetKeyframe(highest_timestamp, &map_keyframe));
  ASSERT_EQ(map_keyframe, last_keyframe);
}

}  // namespace
