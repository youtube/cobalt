// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/splash_video_webm_indexer.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "media/formats/webm/webm_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using Bytes = std::vector<uint8_t>;

// The first bytes of a VP9 key frame and of an inter frame, starting with the
// VP9 frame marker.
constexpr uint8_t kKeyFrameByte = 0x82;
constexpr uint8_t kInterFrameByte = 0x86;

Bytes Concat(std::initializer_list<Bytes> parts) {
  Bytes out;
  for (const Bytes& part : parts) {
    out.insert(out.end(), part.begin(), part.end());
  }
  return out;
}

// An EBML element: its ID, its size as an 8-byte variable-length integer, and
// `payload`.
Bytes Element(int64_t id, const Bytes& payload) {
  Bytes out;
  const int id_length =
      id > 0xFFFFFF ? 4 : (id > 0xFFFF ? 3 : (id > 0xFF ? 2 : 1));
  for (int i = id_length - 1; i >= 0; --i) {
    out.push_back(static_cast<uint8_t>(id >> (8 * i)));
  }
  out.push_back(0x01);
  const uint64_t size = payload.size();
  for (int i = 6; i >= 0; --i) {
    out.push_back(static_cast<uint8_t>(size >> (8 * i)));
  }
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

Bytes UInt(int64_t id, uint64_t value) {
  Bytes payload;
  do {
    payload.insert(payload.begin(), static_cast<uint8_t>(value));
    value >>= 8;
  } while (value);
  return Element(id, payload);
}

Bytes String(int64_t id, const std::string& value) {
  return Element(id, Bytes(value.begin(), value.end()));
}

// The contents of a SimpleBlock or Block of `track_number` (< 127).
Bytes BlockData(int16_t relative_timecode,
                uint8_t flags,
                const Bytes& frame,
                uint8_t track_number = 1) {
  Bytes out = {static_cast<uint8_t>(0x80 | track_number),
               static_cast<uint8_t>(relative_timecode >> 8),
               static_cast<uint8_t>(relative_timecode), flags};
  out.insert(out.end(), frame.begin(), frame.end());
  return out;
}

Bytes SimpleBlock(int16_t relative_timecode,
                  bool is_key_frame,
                  const Bytes& frame) {
  return Element(
      media::kWebMIdSimpleBlock,
      BlockData(relative_timecode, is_key_frame ? 0x80 : 0x00, frame));
}

Bytes TrackEntry(const std::string& codec_id = "V_VP9",
                 uint64_t width = 64,
                 uint64_t height = 36) {
  return Element(
      media::kWebMIdTrackEntry,
      Concat({UInt(media::kWebMIdTrackNumber, 1),
              UInt(media::kWebMIdTrackType, 1),
              String(media::kWebMIdCodecID, codec_id),
              Element(media::kWebMIdVideo,
                      Concat({UInt(media::kWebMIdPixelWidth, width),
                              UInt(media::kWebMIdPixelHeight, height)}))}));
}

Bytes Tracks(const Bytes& track_entries = TrackEntry()) {
  return Element(media::kWebMIdTracks, track_entries);
}

Bytes Info(uint64_t timecode_scale_ns = 1'000'000) {
  return Element(media::kWebMIdInfo,
                 UInt(media::kWebMIdTimecodeScale, timecode_scale_ns));
}

Bytes Cluster(uint64_t timecode, const Bytes& blocks) {
  return Element(media::kWebMIdCluster,
                 Concat({UInt(media::kWebMIdTimecode, timecode), blocks}));
}

Bytes WebM(const Bytes& segment_children) {
  return Concat(
      {Element(media::kWebMIdEBMLHeader, String(media::kWebMIdDocType, "webm")),
       Element(media::kWebMIdSegment, segment_children)});
}

// A key frame at 0ms and an inter frame at 33ms.
Bytes TwoFrameWebM() {
  return WebM(Concat(
      {Info(), Tracks(),
       Cluster(0, Concat({SimpleBlock(0, true, {kKeyFrameByte, 0x49, 0x83}),
                          SimpleBlock(33, false, {kInterFrameByte, 0x00})}))}));
}

TEST(SplashVideoWebMIndexerTest, IndexesFramesInPlace) {
  const Bytes webm = TwoFrameWebM();

  std::optional<SplashVideoIndex> index = IndexSplashVideoWebM(webm);

  ASSERT_TRUE(index);
  EXPECT_EQ(index->width, 64);
  EXPECT_EQ(index->height, 36);
  ASSERT_EQ(index->frames.size(), 2u);

  const SplashVideoFrame& key_frame = index->frames[0];
  EXPECT_EQ(key_frame.size, 3u);
  EXPECT_EQ(webm[key_frame.offset], kKeyFrameByte);
  EXPECT_EQ(webm[key_frame.offset + 2], 0x83);
  EXPECT_EQ(key_frame.timestamp_us, 0);
  EXPECT_TRUE(key_frame.is_key_frame);

  const SplashVideoFrame& inter_frame = index->frames[1];
  EXPECT_EQ(inter_frame.size, 2u);
  EXPECT_EQ(webm[inter_frame.offset], kInterFrameByte);
  EXPECT_EQ(inter_frame.timestamp_us, 33'000);
  EXPECT_FALSE(inter_frame.is_key_frame);
}

TEST(SplashVideoWebMIndexerTest, AppliesClusterTimecodeAndTimecodeScale) {
  // Ticks of 0.5ms.
  const Bytes webm =
      WebM(Concat({Info(/*timecode_scale_ns=*/500'000), Tracks(),
                   Cluster(0, SimpleBlock(0, true, {kKeyFrameByte})),
                   Cluster(2000, SimpleBlock(66, false, {kInterFrameByte}))}));

  std::optional<SplashVideoIndex> index = IndexSplashVideoWebM(webm);

  ASSERT_TRUE(index);
  ASSERT_EQ(index->frames.size(), 2u);
  EXPECT_EQ(index->frames[0].timestamp_us, 0);
  EXPECT_EQ(index->frames[1].timestamp_us, 1'033'000);
}

TEST(SplashVideoWebMIndexerTest, IndexesBlockGroups) {
  const Bytes webm = WebM(Concat(
      {Info(), Tracks(),
       Cluster(
           0,
           Concat({
               // A key frame, without a ReferenceBlock.
               Element(media::kWebMIdBlockGroup,
                       Element(media::kWebMIdBlock,
                               BlockData(0, 0x00, {kKeyFrameByte}))),
               // An inter frame, with a ReferenceBlock of -33.
               Element(media::kWebMIdBlockGroup,
                       Concat({Element(media::kWebMIdBlock,
                                       BlockData(33, 0x00, {kInterFrameByte})),
                               Element(media::kWebMIdReferenceBlock, {0xDF})})),
           }))}));

  std::optional<SplashVideoIndex> index = IndexSplashVideoWebM(webm);

  ASSERT_TRUE(index);
  ASSERT_EQ(index->frames.size(), 2u);
  EXPECT_TRUE(index->frames[0].is_key_frame);
  EXPECT_FALSE(index->frames[1].is_key_frame);
  EXPECT_EQ(index->frames[1].timestamp_us, 33'000);
  EXPECT_EQ(webm[index->frames[1].offset], kInterFrameByte);
}

TEST(SplashVideoWebMIndexerTest, SkipsOtherTopLevelElements) {
  const Bytes webm =
      WebM(Concat({Element(media::kWebMIdVoid, Bytes(16, 0)), Info(), Tracks(),
                   Element(media::kWebMIdTags, Bytes(8, 0xFF)),
                   Cluster(0, SimpleBlock(0, true, {kKeyFrameByte})),
                   Element(media::kWebMIdCues, Bytes(4, 0xAB))}));

  std::optional<SplashVideoIndex> index = IndexSplashVideoWebM(webm);

  ASSERT_TRUE(index);
  EXPECT_EQ(index->frames.size(), 1u);
}

TEST(SplashVideoWebMIndexerTest, RejectsEmptyAndTruncatedFiles) {
  EXPECT_FALSE(IndexSplashVideoWebM({}));

  const Bytes webm = TwoFrameWebM();
  for (size_t size : {size_t{1}, webm.size() / 2, webm.size() - 1}) {
    SCOPED_TRACE(size);
    EXPECT_FALSE(IndexSplashVideoWebM(base::span(webm).first(size)));
  }
}

TEST(SplashVideoWebMIndexerTest, AppliesTheFileSizeLimit) {
  const Bytes webm = TwoFrameWebM();
  EXPECT_TRUE(IndexSplashVideoWebM(webm, webm.size()));
  EXPECT_FALSE(IndexSplashVideoWebM(webm, webm.size() - 1));

  // Over the default limit, e.g., a video that the WebApp cached.
  Bytes frame(kMaxSplashVideoFileSize, 0);
  frame[0] = kKeyFrameByte;
  const Bytes large =
      WebM(Concat({Info(), Tracks(), Cluster(0, SimpleBlock(0, true, frame))}));
  EXPECT_FALSE(IndexSplashVideoWebM(large));
  EXPECT_TRUE(IndexSplashVideoWebM(large, 2 * kMaxSplashVideoFileSize));
}

TEST(SplashVideoWebMIndexerTest, RejectsOtherFormats) {
  const Bytes not_webm(64, 0x42);
  EXPECT_FALSE(IndexSplashVideoWebM(not_webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsCodecsOtherThanVp9) {
  const Bytes webm =
      WebM(Concat({Info(), Tracks(TrackEntry("V_VP8")),
                   Cluster(0, SimpleBlock(0, true, {kKeyFrameByte}))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsMultipleTracks) {
  const Bytes webm =
      WebM(Concat({Info(), Tracks(Concat({TrackEntry(), TrackEntry()})),
                   Cluster(0, SimpleBlock(0, true, {kKeyFrameByte}))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsFramesOfOtherTracks) {
  const Bytes webm =
      WebM(Concat({Info(), Tracks(),
                   Cluster(0, Element(media::kWebMIdSimpleBlock,
                                      BlockData(0, 0x80, {kKeyFrameByte},
                                                /*track_number=*/2)))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsInvalidDimensions) {
  for (const auto& [width, height] :
       {std::pair<uint64_t, uint64_t>{0, 36}, {64, 0}, {8192, 36}}) {
    SCOPED_TRACE(testing::Message() << width << "x" << height);
    const Bytes webm =
        WebM(Concat({Info(), Tracks(TrackEntry("V_VP9", width, height)),
                     Cluster(0, SimpleBlock(0, true, {kKeyFrameByte}))}));
    EXPECT_FALSE(IndexSplashVideoWebM(webm));
  }
}

TEST(SplashVideoWebMIndexerTest, RejectsLacedBlocks) {
  const Bytes webm = WebM(Concat(
      {Info(), Tracks(),
       Cluster(0, Element(media::kWebMIdSimpleBlock,
                          BlockData(0, 0x80 | 0x02, {kKeyFrameByte})))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsFirstFrameThatIsNotAKeyFrame) {
  const Bytes webm = WebM(
      Concat({Info(), Tracks(),
              Cluster(0, Concat({SimpleBlock(0, false, {kInterFrameByte}),
                                 SimpleBlock(33, true, {kKeyFrameByte})}))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsTimestampsThatDontIncrease) {
  const Bytes webm = WebM(Concat(
      {Info(), Tracks(),
       Cluster(0, Concat({SimpleBlock(0, true, {kKeyFrameByte}),
                          SimpleBlock(33, false, {kInterFrameByte}),
                          SimpleBlock(33, false, {kInterFrameByte})}))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsVideosThatDontStartAt0) {
  // The first frame at 33ms, from the block or from the Cluster.
  for (const Bytes& cluster :
       {Cluster(0, SimpleBlock(33, true, {kKeyFrameByte})),
        Cluster(33, SimpleBlock(0, true, {kKeyFrameByte}))}) {
    EXPECT_FALSE(
        IndexSplashVideoWebM(WebM(Concat({Info(), Tracks(), cluster}))));
  }
}

TEST(SplashVideoWebMIndexerTest, RejectsVideosLongerThanTheLimit) {
  const Bytes webm = WebM(Concat(
      {Info(), Tracks(), Cluster(0, SimpleBlock(0, true, {kKeyFrameByte})),
       Cluster(kMaxSplashVideoDurationUs / 1000 + 1,
               SimpleBlock(0, false, {kInterFrameByte}))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsFramesWithoutAVp9FrameMarker) {
  const Bytes webm = WebM(Concat(
      {Info(), Tracks(), Cluster(0, SimpleBlock(0, true, {0x00, 0x82}))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, RejectsVp9ProfilesOtherThan0) {
  // Profiles 1, 2 and 3, from the profile's low and high bits, after the frame
  // marker: the player is configured for 8-bit 4:2:0.
  for (uint8_t profile_bits : {0x20, 0x10, 0x30}) {
    SCOPED_TRACE(static_cast<int>(profile_bits));
    const uint8_t key_frame_byte =
        static_cast<uint8_t>(kKeyFrameByte | profile_bits);
    EXPECT_FALSE(IndexSplashVideoWebM(
        WebM(Concat({Info(), Tracks(),
                     Cluster(0, SimpleBlock(0, true, {key_frame_byte}))}))));
    EXPECT_FALSE(IndexSplashVideoWebM(WebM(Concat(
        {Info(), Tracks(),
         Cluster(0, Concat({SimpleBlock(0, true, {kKeyFrameByte}),
                            SimpleBlock(
                                33, false,
                                {static_cast<uint8_t>(kInterFrameByte |
                                                      profile_bits)})}))}))));
  }
}

TEST(SplashVideoWebMIndexerTest, RejectsBlocksBeforeTheClusterTimecode) {
  const Bytes webm =
      WebM(Concat({Info(), Tracks(),
                   Element(media::kWebMIdCluster,
                           Concat({SimpleBlock(0, true, {kKeyFrameByte}),
                                   UInt(media::kWebMIdTimecode, 0)}))}));
  EXPECT_FALSE(IndexSplashVideoWebM(webm));
}

TEST(SplashVideoWebMIndexerTest, IndexesBuiltInSplashVideos) {
  struct BuiltInVideo {
    const char* name;
    int width;
    int height;
  };
  constexpr BuiltInVideo kBuiltInVideos[] = {
      {"splash.webm", 1920, 1080},
      {"splash_480.webm", 853, 480},
      {"music_splash.webm", 1920, 1080},
      {"music_splash_480.webm", 853, 480},
  };
  for (const BuiltInVideo& video : kBuiltInVideos) {
    SCOPED_TRACE(video.name);
    const base::span<const uint8_t> webm =
        H5vccSchemeURLLoaderFactory::GetBuiltInResource(video.name);
    ASSERT_FALSE(webm.empty());

    std::optional<SplashVideoIndex> index = IndexSplashVideoWebM(webm);

    ASSERT_TRUE(index);
    EXPECT_EQ(index->width, video.width);
    EXPECT_EQ(index->height, video.height);
    ASSERT_GE(index->frames.size(), 30u);
    EXPECT_TRUE(index->frames.front().is_key_frame);
    EXPECT_EQ(index->frames.front().timestamp_us, 0);
    EXPECT_GE(index->frames.back().timestamp_us, 1'000'000);
    EXPECT_LE(index->frames.back().timestamp_us, 2'000'000);
  }
}

TEST(SplashVideoWebMIndexerTest, NoBuiltInResourceForUnknownNames) {
  EXPECT_TRUE(
      H5vccSchemeURLLoaderFactory::GetBuiltInResource("missing.webm").empty());
}

}  // namespace
}  // namespace content
