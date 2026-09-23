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

#include "media/starboard/sbplayer_bridge.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/numerics/byte_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/starboard/starboard_renderer_config.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "media/starboard/mock_sbplayer_interface.h"
#include "starboard/player.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace media {

namespace {

// BlockAddID for HDR10+ (ITU-T T.35) dynamic metadata carried in a WebM
// BlockAdditional element. WebMClusterParser prepends this as an 8-byte
// big-endian prefix to DecoderBufferSideData::alpha_data, mirroring the
// ffmpeg demuxer's behavior. See webm_cluster_parser.cc and the
// SbPlayerSampleSideData contract in starboard/player.h.
constexpr uint64_t kHdr10PlusBlockAddId = 4;

// A no-op SbPlayerBridge::Host. These tests drive WriteBuffers() directly and
// never exercise the callback paths, so nothing needs to be recorded here.
class FakeSbPlayerBridgeHost : public SbPlayerBridge::Host {
 public:
  FakeSbPlayerBridgeHost() = default;
  ~FakeSbPlayerBridgeHost() = default;

  void OnNeedData(DemuxerStream::Type type,
                  int max_number_of_buffers_to_write) override {}
  void OnPlayerStatus(SbPlayerState state) override {}
  void OnPlayerError(SbPlayerError error, const std::string& message) override {
  }
};

// A deep copy of the parts of SbPlayerSampleInfo under test. The pointers in
// SbPlayerSampleInfo reference storage that SbPlayerBridge clears as soon as
// WriteSamples() returns, so everything must be copied out synchronously
// inside the mock action.
struct CapturedSample {
  bool has_side_data_pointer = false;
  int side_data_count = 0;
  SbPlayerSampleSideDataType side_data_type{};
  std::vector<uint8_t> side_data;
};

// Builds an alpha_data payload in the layout SbPlayerBridge forwards verbatim:
// an 8-byte big-endian BlockAddID followed by the BlockAdditional content.
base::HeapArray<uint8_t> MakeBlockAdditional(
    uint64_t block_add_id,
    base::span<const uint8_t> content) {
  const std::array<uint8_t, 8u> id_bytes = base::U64ToBigEndian(block_add_id);
  auto payload =
      base::HeapArray<uint8_t>::Uninit(id_bytes.size() + content.size());
  auto [id_span, content_span] =
      base::span(payload).split_at<id_bytes.size()>();
  id_span.copy_from(base::span(id_bytes));
  content_span.copy_from(content);
  return payload;
}

scoped_refptr<DecoderBuffer> MakeVideoBuffer(
    base::span<const uint8_t> payload,
    base::span<const uint8_t> block_additional) {
  scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(payload);
  if (!block_additional.empty()) {
    buffer->WritableSideData().alpha_data =
        base::HeapArray<uint8_t>::CopiedFrom(block_additional);
  }
  return buffer;
}

class SbPlayerBridgeSideDataTest : public testing::Test {
 protected:
  SbPlayerBridgeSideDataTest() {
    // SbPlayerBridge's constructor calls CreatePlayer() synchronously, so the
    // Create() action must be installed before the bridge is built.
    EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
        .WillOnce(Return(reinterpret_cast<SbPlayer>(new MockSbPlayer())));

    EXPECT_CALL(mock_sbplayer_interface_, WriteSamples(_, _, _, _))
        .WillRepeatedly(
            Invoke([this](SbPlayer /*player*/, SbMediaType /*sample_type*/,
                          const SbPlayerSampleInfo* sample_infos,
                          int number_of_sample_infos) {
              for (int i = 0; i < number_of_sample_infos; ++i) {
                const SbPlayerSampleInfo& info = sample_infos[i];
                CapturedSample captured;
                captured.has_side_data_pointer = info.side_data != nullptr;
                captured.side_data_count = info.side_data_count;
                if (info.side_data && info.side_data_count > 0) {
                  captured.side_data_type = info.side_data[0].type;
                  captured.side_data.assign(
                      info.side_data[0].data,
                      info.side_data[0].data + info.side_data[0].size);
                }
                captured_samples_.push_back(std::move(captured));
              }
            }));

    // Declared out of line rather than brace-initialized in place: cpplint's
    // whitespace/braces check looks ahead past the closing brace for a
    // delimiter, and the #if below hides it.
    const StarboardRendererConfig::ExperimentalFeatures experimental_features;

    bridge_ = std::make_unique<SbPlayerBridge>(
        &mock_sbplayer_interface_, task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(
            []() -> SbDecodeTargetGraphicsContextProvider* { return nullptr; }),
        AudioDecoderConfig(), TestVideoConfig::NormalHdr(VideoCodec::kVP9),
        kSbWindowInvalid, kSbDrmSystemInvalid, &host_,
        /*allow_resume_after_suspend=*/false, kSbPlayerOutputModePunchOut,
        /*max_video_capabilities=*/"",
        /*max_video_input_size=*/0, experimental_features
#if BUILDFLAG(IS_ANDROID)
        ,
        /*surface_view=*/nullptr
#endif  // BUILDFLAG(IS_ANDROID)
    );

    CHECK(bridge_->IsValid());
  }

  ~SbPlayerBridgeSideDataTest() override = default;

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockSbPlayerInterface> mock_sbplayer_interface_;
  FakeSbPlayerBridgeHost host_;
  std::vector<CapturedSample> captured_samples_;
  std::unique_ptr<SbPlayerBridge> bridge_;
};

// A buffer carrying BlockAdditional side data (the carriage mechanism for
// HDR10+ dynamic metadata in WebM) must reach SbPlayerWriteSamples() with the
// payload byte-for-byte intact and tagged kMatroskaBlockAdditional.
TEST_F(SbPlayerBridgeSideDataTest, ForwardsBlockAdditionalSideData) {
  constexpr uint8_t kMetadata[] = {0xb5, 0x00, 0x3c, 0x00, 0x01, 0x04, 0x40};
  base::HeapArray<uint8_t> block_additional =
      MakeBlockAdditional(kHdr10PlusBlockAddId, kMetadata);
  const std::vector<uint8_t> expected(block_additional.begin(),
                                      block_additional.end());

  constexpr uint8_t kPayload[] = {0x01, 0x02, 0x03, 0x04};
  bridge_->WriteBuffers(DemuxerStream::VIDEO,
                        {MakeVideoBuffer(kPayload, block_additional)});

  ASSERT_EQ(captured_samples_.size(), 1u);
  EXPECT_TRUE(captured_samples_[0].has_side_data_pointer);
  EXPECT_EQ(captured_samples_[0].side_data_count, 1);
  EXPECT_EQ(captured_samples_[0].side_data_type, kMatroskaBlockAdditional);
  EXPECT_EQ(captured_samples_[0].side_data, expected);
}

// A buffer with no BlockAdditional must not fabricate an empty side data
// entry, which Starboard would otherwise have to defend against.
TEST_F(SbPlayerBridgeSideDataTest, OmitsSideDataWhenBlockAdditionalAbsent) {
  constexpr uint8_t kPayload[] = {0x01, 0x02, 0x03, 0x04};
  bridge_->WriteBuffers(DemuxerStream::VIDEO,
                        {MakeVideoBuffer(kPayload, /*block_additional=*/{})});

  ASSERT_EQ(captured_samples_.size(), 1u);
  EXPECT_FALSE(captured_samples_[0].has_side_data_pointer);
  EXPECT_EQ(captured_samples_[0].side_data_count, 0);
}

// WriteBuffersInternal() indexes into a batch-wide side data vector, so a
// mixed batch guards against the side data of one sample being attributed to
// another, and against pointer invalidation if that vector is ever resized.
TEST_F(SbPlayerBridgeSideDataTest, AttributesSideDataToCorrectSampleInBatch) {
  constexpr uint8_t kFirstMetadata[] = {0xaa, 0xbb};
  constexpr uint8_t kThirdMetadata[] = {0xcc, 0xdd, 0xee};
  base::HeapArray<uint8_t> first_block_additional =
      MakeBlockAdditional(kHdr10PlusBlockAddId, kFirstMetadata);
  base::HeapArray<uint8_t> third_block_additional =
      MakeBlockAdditional(kHdr10PlusBlockAddId, kThirdMetadata);
  const std::vector<uint8_t> expected_first(first_block_additional.begin(),
                                            first_block_additional.end());
  const std::vector<uint8_t> expected_third(third_block_additional.begin(),
                                            third_block_additional.end());

  constexpr uint8_t kPayload[] = {0x01, 0x02, 0x03, 0x04};
  bridge_->WriteBuffers(DemuxerStream::VIDEO,
                        {MakeVideoBuffer(kPayload, first_block_additional),
                         MakeVideoBuffer(kPayload, /*block_additional=*/{}),
                         MakeVideoBuffer(kPayload, third_block_additional)});

  ASSERT_EQ(captured_samples_.size(), 3u);

  EXPECT_EQ(captured_samples_[0].side_data_count, 1);
  EXPECT_EQ(captured_samples_[0].side_data_type, kMatroskaBlockAdditional);
  EXPECT_EQ(captured_samples_[0].side_data, expected_first);

  EXPECT_FALSE(captured_samples_[1].has_side_data_pointer);
  EXPECT_EQ(captured_samples_[1].side_data_count, 0);

  EXPECT_EQ(captured_samples_[2].side_data_count, 1);
  EXPECT_EQ(captured_samples_[2].side_data_type, kMatroskaBlockAdditional);
  EXPECT_EQ(captured_samples_[2].side_data, expected_third);
}

}  // namespace

}  // namespace media
