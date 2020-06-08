// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/vp9_util.h"

#include <vector>

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

std::vector<uint8_t> operator+(const std::vector<uint8_t>& left,
                               const std::vector<uint8_t>& right) {
  std::vector<uint8_t> result(left);
  result.insert(result.end(), right.begin(), right.end());
  return result;
}

uint8_t CreateSuperframeMarker(size_t bytes_of_size,
                               size_t number_of_subframes) {
  SB_DCHECK(bytes_of_size > 0);
  SB_DCHECK(bytes_of_size <= 4);
  SB_DCHECK(number_of_subframes > 0);
  SB_DCHECK(number_of_subframes <= Vp9FrameParser::kMaxNumberOfSubFrames);

  return static_cast<uint8_t>(0b11000000 | ((bytes_of_size - 1) << 3) |
                              (number_of_subframes - 1));
}

std::vector<uint8_t> ConvertSizeToBytes(size_t size, size_t bytes_of_size) {
  SB_DCHECK(bytes_of_size > 0);
  SB_DCHECK(bytes_of_size <= 4);

  std::vector<uint8_t> size_in_bytes;

  while (bytes_of_size > 0) {
    size_in_bytes.push_back(size % 256);
    --bytes_of_size;
    size /= 256;
  }

  SB_DCHECK(size == 0);

  return size_in_bytes;
}

void AddSubframe(const std::vector<uint8_t>& subframe,
                 std::vector<uint8_t>* frame_data,
                 std::vector<uint8_t>* superframe_metadata,
                 size_t bytes_of_size) {
  SB_DCHECK(frame_data);
  SB_DCHECK(superframe_metadata);

  frame_data->insert(frame_data->end(), subframe.begin(), subframe.end());
  auto size_in_bytes = ConvertSizeToBytes(subframe.size(), bytes_of_size);
  superframe_metadata->insert(superframe_metadata->end(), size_in_bytes.begin(),
                              size_in_bytes.end());
}

TEST(Vp9FrameParserTests, EmptyFrame) {
  Vp9FrameParser parser(nullptr, 0);

  ASSERT_EQ(parser.number_of_subframes(), 1);
  EXPECT_EQ(parser.size_of_subframe(0), 0);
}

TEST(Vp9FrameParserTests, NonSuperFrame) {
  std::vector<uint8_t> kFrameData({1, 2, 3, 0});
  Vp9FrameParser parser(kFrameData.data(), kFrameData.size());

  ASSERT_EQ(parser.number_of_subframes(), 1);
  EXPECT_EQ(parser.address_of_subframe(0), kFrameData.data());
  EXPECT_EQ(parser.size_of_subframe(0), kFrameData.size());
}

TEST(Vp9FrameParserTests, SuperFrames) {
  std::vector<uint8_t> kFrameData({1, 2, 3, 4, 5, 6, 7, 8, 9, 0});

  for (size_t bytes_of_size = 1; bytes_of_size <= 4; ++bytes_of_size) {
    for (size_t number_of_subframes = 1;
         number_of_subframes <= Vp9FrameParser::kMaxNumberOfSubFrames;
         ++number_of_subframes) {
      std::vector<uint8_t> frame_data;
      std::vector<uint8_t> superframe_metadata;

      const uint8_t superframe_marker =
          CreateSuperframeMarker(bytes_of_size, number_of_subframes);

      superframe_metadata.push_back(superframe_marker);

      for (size_t subframe_index = 0; subframe_index < number_of_subframes;
           ++subframe_index) {
        AddSubframe(kFrameData, &frame_data, &superframe_metadata,
                    bytes_of_size);
      }

      superframe_metadata.push_back(superframe_marker);

      auto superframe = frame_data + superframe_metadata;

      Vp9FrameParser parser(superframe.data(), superframe.size());

      ASSERT_EQ(parser.number_of_subframes(), number_of_subframes);
      for (size_t subframe_index = 0; subframe_index < number_of_subframes;
           ++subframe_index) {
        EXPECT_EQ(parser.address_of_subframe(subframe_index),
                  superframe.data() + subframe_index * kFrameData.size());
        EXPECT_EQ(parser.size_of_subframe(subframe_index), kFrameData.size());
      }
    }
  }
}

TEST(Vp9FrameParserTests, SuperFramesWithEmptySubframes) {
  std::vector<uint8_t> kEmptyFrameData;

  for (size_t bytes_of_size = 1; bytes_of_size <= 4; ++bytes_of_size) {
    for (size_t number_of_subframes = 1;
         number_of_subframes <= Vp9FrameParser::kMaxNumberOfSubFrames;
         ++number_of_subframes) {
      std::vector<uint8_t> frame_data;
      std::vector<uint8_t> superframe_metadata;

      const uint8_t superframe_marker =
          CreateSuperframeMarker(bytes_of_size, number_of_subframes);

      superframe_metadata.push_back(superframe_marker);

      for (size_t subframe_index = 0; subframe_index < number_of_subframes;
           ++subframe_index) {
        AddSubframe(kEmptyFrameData, &frame_data, &superframe_metadata,
                    bytes_of_size);
      }

      superframe_metadata.push_back(superframe_marker);

      auto superframe = frame_data + superframe_metadata;

      Vp9FrameParser parser(superframe.data(), superframe.size());

      ASSERT_EQ(parser.number_of_subframes(), number_of_subframes);
      for (size_t subframe_index = 0; subframe_index < number_of_subframes;
           ++subframe_index) {
        EXPECT_EQ(parser.address_of_subframe(subframe_index),
                  superframe.data() + subframe_index * kEmptyFrameData.size());
        EXPECT_EQ(parser.size_of_subframe(subframe_index),
                  kEmptyFrameData.size());
      }
    }
  }
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
