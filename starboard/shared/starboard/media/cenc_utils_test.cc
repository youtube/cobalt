// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/shared/starboard/media/cenc_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <iterator>
#include <limits>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/span.h"
#include "testing/gtest/include/gtest/gtest.h"

// Copied from media/cdm/cenc_utils_unittest.cc, reduced to the
// ValidatePsshInput() expectations and ported to C++17 without //base.

namespace starboard {
namespace {

// clang-format off
constexpr uint8_t kKey1[] = {
    0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
    0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03
};

constexpr uint8_t kKey2[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
};

constexpr uint8_t kKey3[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x05,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x05,
};

constexpr uint8_t kKey4[] = {
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x06,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x06,
};

constexpr uint8_t kCommonSystemSystemId[] = {
    0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,
    0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B
};
// clang-format on

// Initialize the start of the 'pssh' box (up to key_count)
void InitializePSSHBox(std::vector<uint8_t>* box,
                       uint8_t size,
                       uint8_t version) {
  SB_DCHECK(box->size() == 0);

  box->reserve(size);
  // Add size.
  SB_DCHECK(size < std::numeric_limits<uint8_t>::max());
  box->push_back(0);
  box->push_back(0);
  box->push_back(0);
  box->push_back(size);
  // Add 'pssh'.
  box->push_back('p');
  box->push_back('s');
  box->push_back('s');
  box->push_back('h');
  // Add version.
  box->push_back(version);
  // Add flags.
  box->push_back(0);
  box->push_back(0);
  box->push_back(0);
  // Add Common Encryption SystemID.
  box->insert(box->end(), std::begin(kCommonSystemSystemId),
              std::end(kCommonSystemSystemId));
}

std::vector<uint8_t> MakePSSHBox(uint8_t version) {
  std::vector<uint8_t> box;
  uint8_t size = (version == 0) ? 32 : 36;
  InitializePSSHBox(&box, size, version);
  if (version > 0) {
    // Add key_count (= 0).
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
  }
  // Add data_size (= 0).
  box.push_back(0);
  box.push_back(0);
  box.push_back(0);
  box.push_back(0);
  return box;
}

std::vector<uint8_t> MakePSSHBox(uint8_t version, span<const uint8_t> key1) {
  SB_DCHECK(version > 0);
  SB_DCHECK(key1.size() == 16);

  std::vector<uint8_t> box;
  uint8_t size = 52;
  InitializePSSHBox(&box, size, version);

  // Add key_count (= 1).
  box.push_back(0);
  box.push_back(0);
  box.push_back(0);
  box.push_back(1);

  // Add key1.
  for (size_t i = 0; i < key1.size(); ++i) {
    box.push_back(key1[i]);
  }

  // Add data_size (= 0).
  box.push_back(0);
  box.push_back(0);
  box.push_back(0);
  box.push_back(0);
  return box;
}

std::vector<uint8_t> MakePSSHBox(uint8_t version,
                                 span<const uint8_t> key1,
                                 span<const uint8_t> key2) {
  SB_DCHECK(version > 0);
  SB_DCHECK(key1.size() == 16);
  SB_DCHECK(key2.size() == 16);

  std::vector<uint8_t> box;
  uint8_t size = 68;
  InitializePSSHBox(&box, size, version);

  // Add key_count (= 2).
  box.push_back(0);
  box.push_back(0);
  box.push_back(0);
  box.push_back(2);

  // Add key1.
  for (size_t i = 0; i < key1.size(); ++i) {
    box.push_back(key1[i]);
  }

  // Add key2.
  for (size_t i = 0; i < key2.size(); ++i) {
    box.push_back(key2[i]);
  }

  // Add data_size (= 0).
  box.push_back(0);
  box.push_back(0);
  box.push_back(0);
  box.push_back(0);
  return box;
}

TEST(CencUtilsTest, EmptyPSSH) {
  EXPECT_TRUE(ValidatePsshInput(std::vector<uint8_t>()));
}

TEST(CencUtilsTest, PSSHVersion0) {
  std::vector<uint8_t> box = MakePSSHBox(0);
  EXPECT_TRUE(ValidatePsshInput(box));
}

TEST(CencUtilsTest, PSSHVersion1WithNoKeys) {
  std::vector<uint8_t> box = MakePSSHBox(1);
  EXPECT_TRUE(ValidatePsshInput(box));
}

TEST(CencUtilsTest, PSSHVersion1WithOneKey) {
  std::vector<uint8_t> box = MakePSSHBox(1, kKey1);
  EXPECT_TRUE(ValidatePsshInput(box));
}

TEST(CencUtilsTest, PSSHVersion1WithTwoKeys) {
  std::vector<uint8_t> box = MakePSSHBox(1, kKey1, kKey2);
  EXPECT_TRUE(ValidatePsshInput(box));
}

TEST(CencUtilsTest, PSSHVersion0Plus1) {
  std::vector<uint8_t> box0 = MakePSSHBox(0);
  std::vector<uint8_t> box1 = MakePSSHBox(1, kKey1);

  // Concatenate box1 onto end of box0.
  box0.insert(box0.end(), box1.begin(), box1.end());
  EXPECT_TRUE(ValidatePsshInput(box0));
}

TEST(CencUtilsTest, PSSHVersion1Plus0) {
  std::vector<uint8_t> box0 = MakePSSHBox(0);
  std::vector<uint8_t> box1 = MakePSSHBox(1, kKey1);

  // Concatenate box0 onto end of box1.
  box1.insert(box1.end(), box0.begin(), box0.end());
  EXPECT_TRUE(ValidatePsshInput(box1));
}

TEST(CencUtilsTest, MultiplePSSHVersion1) {
  std::vector<uint8_t> box = MakePSSHBox(1, kKey1, kKey2);
  std::vector<uint8_t> box1 = MakePSSHBox(1, kKey3);
  std::vector<uint8_t> box2 = MakePSSHBox(1, kKey4);

  // Concatenate box1 and box2 onto end of box.
  box.insert(box.end(), box1.begin(), box1.end());
  box.insert(box.end(), box2.begin(), box2.end());
  EXPECT_TRUE(ValidatePsshInput(box));
}

TEST(CencUtilsTest, PsshBoxSmallerThanSize) {
  std::vector<uint8_t> box = MakePSSHBox(1, kKey1, kKey2);

  // Tries every buffer size less than the indicated 'pssh' box size.
  for (size_t i = 1; i < box.size(); ++i) {
    auto truncated = make_span(box).first(i);
    EXPECT_FALSE(ValidatePsshInput(truncated)) << "Failed for length " << i;
  }
}

TEST(CencUtilsTest, PsshBoxLargerThanSize) {
  std::vector<uint8_t> box = MakePSSHBox(1, kKey1, kKey2);

  // Add 20 additional bytes to |box|.
  size_t original_size = box.size();
  for (size_t i = 0; i < 20; ++i) {
    box.push_back(static_cast<uint8_t>(i));
  }

  // Tries every size greater than |original_size|.
  for (size_t i = original_size + 1; i < box.size(); ++i) {
    auto truncated = make_span(box).first(i);
    EXPECT_FALSE(ValidatePsshInput(truncated)) << "Failed for length " << i;
  }
}

TEST(CencUtilsTest, InvalidFlags) {
  std::vector<uint8_t> box = MakePSSHBox(1, kKey1, kKey2);

  // Modify flags.
  box[10] = 3;

  EXPECT_FALSE(ValidatePsshInput(box));
}

TEST(CencUtilsTest, LongSize) {
  constexpr uint8_t data[] = {
      0x00, 0x00, 0x00, 0x01,                          // size = 1
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c,  // longsize
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  EXPECT_TRUE(ValidatePsshInput(data));
}

TEST(CencUtilsTest, SizeIsZero) {
  constexpr uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00,                          // size = 0
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  EXPECT_TRUE(ValidatePsshInput(data));
}

TEST(CencUtilsTest, HugeSize) {
  constexpr uint8_t data[] = {
      0x00, 0x00, 0x00, 0x01,                          // size = 1
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // longsize = big
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

  // This call fails as the box size is huge (0xffffffffffffffff) and there
  // is not enough bytes in |data|.
  EXPECT_FALSE(ValidatePsshInput(data));
}

// Adapted from GetPsshData_Version2: a version 2 box can't be parsed, so it is
// skipped, which leaves no 'pssh' box.
TEST(CencUtilsTest, Version2) {
  std::vector<uint8_t> box = MakePSSHBox(1, kKey1);
  EXPECT_TRUE(ValidatePsshInput(box));

  // Change the version manually, since we don't know what v2 will contain.
  box[8] = 2;
  EXPECT_FALSE(ValidatePsshInput(box));
}

// Adapted from GetPsshData_Version2ThenVersion1: the version 2 box is skipped
// and the version 1 box makes the input valid.
TEST(CencUtilsTest, Version2ThenVersion1) {
  std::vector<uint8_t> box_v1 = MakePSSHBox(1, kKey1);
  std::vector<uint8_t> box_v2 = MakePSSHBox(2, kKey2, kKey3);

  // Concatenate the boxes together (v2 first).
  std::vector<uint8_t> boxes;
  boxes.insert(boxes.end(), box_v2.begin(), box_v2.end());
  boxes.insert(boxes.end(), box_v1.begin(), box_v1.end());
  EXPECT_TRUE(ValidatePsshInput(boxes));
}

TEST(CencUtilsTest, NonPsshData) {
  // Create a non-'pssh' box.
  constexpr std::array<uint8_t, 8> data = {0x00, 0x00, 0x00, 0x08,  // size = 8
                                           'p',  's',  's',  'g'};
  EXPECT_FALSE(ValidatePsshInput(data));

  // Make a valid 'pssh' box.
  std::vector<uint8_t> pssh_box = MakePSSHBox(1, kKey1);
  EXPECT_TRUE(ValidatePsshInput(pssh_box));

  // Concatenate the boxes together (|pssh_box| first).
  std::vector<uint8_t> boxes;
  boxes.insert(boxes.end(), pssh_box.begin(), pssh_box.end());
  boxes.insert(boxes.end(), data.begin(), data.end());
  EXPECT_FALSE(ValidatePsshInput(boxes));

  // Repeat with |non_pssh_box| first.
  boxes.clear();
  boxes.insert(boxes.end(), data.begin(), data.end());
  boxes.insert(boxes.end(), pssh_box.begin(), pssh_box.end());
  EXPECT_FALSE(ValidatePsshInput(boxes));
}

}  // namespace
}  // namespace starboard
