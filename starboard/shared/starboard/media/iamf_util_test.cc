// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/iamf_util.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

TEST(IamfUtilTest, IsValid) {
  std::string mime_type = "iamf.000.000.Opus";
  IamfMimeUtil util(mime_type);
  ASSERT_TRUE(util.is_valid());

  mime_type = "iamf.000.000.fLaC";
  util = IamfMimeUtil(mime_type);
  ASSERT_TRUE(util.is_valid());

  mime_type = "iamf.000.000.ipcm";
  util = IamfMimeUtil(mime_type);
  ASSERT_TRUE(util.is_valid());

  mime_type = "iamf.000.000.mp4a.40.2";
  util = IamfMimeUtil(mime_type);
  ASSERT_TRUE(util.is_valid());

  mime_type = "iamf.000.000.vorbis";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.00.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.999.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.999.000.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iacb.000.000.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.000.Opus.40.2";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.000.mp4a.40.3";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.000.mp4a.40.20";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.000..Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf000.000.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.00.000.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.00.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.00.mp4a.40.20";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "iamf.000.000.mp4a.402.";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());

  mime_type = "ec-3";
  util = IamfMimeUtil(mime_type);
  ASSERT_FALSE(util.is_valid());
}

TEST(IamfUtilTest, SubstreamCodec) {
  std::string mime_type = "iamf.000.000.vorbis";
  IamfMimeUtil util(mime_type);
  ASSERT_EQ(util.substream_codec(), kIamfSubstreamCodecUnknown);

  mime_type = "iamf.000.000";
  util = IamfMimeUtil(mime_type);
  ASSERT_EQ(util.substream_codec(), kIamfSubstreamCodecUnknown);

  mime_type = "iamf.000.000.opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_EQ(util.substream_codec(), kIamfSubstreamCodecUnknown);

  mime_type = "iamf.000.000.flac";
  util = IamfMimeUtil(mime_type);
  ASSERT_EQ(util.substream_codec(), kIamfSubstreamCodecUnknown);

  mime_type = "iamf.000.000.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_EQ(util.substream_codec(), kIamfSubstreamCodecOpus);

  mime_type = "iamf.000.000.fLaC";
  util = IamfMimeUtil(mime_type);
  ASSERT_EQ(util.substream_codec(), kIamfSubstreamCodecFlac);

  mime_type = "iamf.000.000.ipcm";
  util = IamfMimeUtil(mime_type);
  ASSERT_EQ(util.substream_codec(), kIamfSubstreamCodecIpcm);

  mime_type = "iamf.000.000.mp4a.40.2";
  util = IamfMimeUtil(mime_type);
  ASSERT_EQ(util.substream_codec(), kIamfSubstreamCodecMp4a);
}

TEST(IamfUtilTest, Profile) {
  std::string mime_type = "iamf.000.000.Opus";
  IamfMimeUtil util(mime_type);
  ASSERT_EQ(util.profile(), kIamfProfileSimple);

  mime_type = "iamf.001.000.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_EQ(util.profile(), kIamfProfileBase);

  mime_type = "iamf.002.000.Opus";
  util = IamfMimeUtil(mime_type);
  ASSERT_NE(util.profile(), kIamfProfileSimple);
  ASSERT_NE(util.profile(), kIamfProfileBase);
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
