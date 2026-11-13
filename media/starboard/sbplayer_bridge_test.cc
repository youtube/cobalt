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
#include "base/test/metrics/histogram_tester.h"
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

namespace media {

namespace {

using ::testing::_;
using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

// BlockAddID for HDR10+ (ITU-T T.35) dynamic metadata carried in a WebM
// BlockAdditional element. SbPlayerBridge treats alpha_data as an opaque blob,
// so nothing here validates this framing; the tests build a realistically
// shaped payload only so the fixtures read like real data. Production layout
// is owned by webm_cluster_parser.cc.
constexpr uint64_t kHdr10PlusBlockAddId = 4;

// A no-op SbPlayerBridge::Host. These tests drive WriteBuffers() directly and
// never exercise the callback paths, so nothing needs to be recorded here.
// Owned by SbPlayerBridgeSideDataTest and thread-affine to the main test
// thread.
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
  auto [id_span, content_span] = payload.as_span().split_at<id_bytes.size()>();
  id_span.copy_from(id_bytes);
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

// Builds a buffer whose side data exists but carries no BlockAdditional.
// |spatial_layers| is chosen because SbPlayerBridge never reads it, so the only
// thing that changes relative to MakeVideoBuffer() is that side_data() becomes
// non-null.
scoped_refptr<DecoderBuffer> MakeVideoBufferWithUnrelatedSideData(
    base::span<const uint8_t> payload) {
  scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(payload);
  buffer->WritableSideData().spatial_layers = {1u, 2u, 3u};
  return buffer;
}

// Test fixture verifying that SbPlayerBridge forwards DecoderBuffer side data
// (Matroska BlockAdditional / HDR10+ ITU-T T.35 metadata) to
// SbPlayerWriteSamples(). Instantiated per test by GoogleTest and runs on the
// main test thread.
class SbPlayerBridgeSideDataTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_player_ = std::make_unique<MockSbPlayer>();

    // SbPlayerBridge's constructor calls CreatePlayer() synchronously, so the
    // Create() action must be installed before the bridge is built.
    EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
        .WillOnce(Return(reinterpret_cast<SbPlayer>(mock_player_.get())));
    EXPECT_CALL(mock_sbplayer_interface_, Destroy(_))
        .WillRepeatedly(
            Invoke([this](SbPlayer /*player*/) { mock_player_.reset(); }));

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
                if (info.side_data && info.side_data_count > 0 &&
                    info.side_data[0].data) {
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
    const StarboardRendererConfig::ExperimentalFeatures experimental_features{};

    bridge_ = std::make_unique<SbPlayerBridge>(
        &mock_sbplayer_interface_, task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(
            []() -> SbDecodeTargetGraphicsContextProvider* { return nullptr; }),
        AudioDecoderConfig(), TestVideoConfig::NormalHdr(VideoCodec::kVP9),
        kSbWindowInvalid, kSbDrmSystemInvalid, &host_,
        /*allow_resume_after_suspend=*/false, kSbPlayerOutputModePunchOut,
        /*max_video_capabilities=*/"",
        /*max_video_resolution=*/"",
        /*max_video_input_size=*/0, experimental_features
#if BUILDFLAG(IS_ANDROID)
        ,
        /*surface_view=*/nullptr
#endif  // BUILDFLAG(IS_ANDROID)
    );

    ASSERT_TRUE(bridge_->IsValid());
  }

  ~SbPlayerBridgeSideDataTest() override = default;

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockSbPlayer> mock_player_;
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

// The write path guards on two conditions: side data must exist *and* carry a
// non-empty alpha_data. DecoderBufferSideData also carries spatial_layers,
// secure_handle and discard_padding, so side data with no BlockAdditional is
// routine in production. Dropping the emptiness check would tag every such
// buffer with a zero-length side data entry.
TEST_F(SbPlayerBridgeSideDataTest, OmitsSideDataWhenAlphaDataEmpty) {
  constexpr uint8_t kPayload[] = {0x01, 0x02, 0x03, 0x04};
  bridge_->WriteBuffers(DemuxerStream::VIDEO,
                        {MakeVideoBufferWithUnrelatedSideData(kPayload)});

  ASSERT_EQ(captured_samples_.size(), 1u);
  EXPECT_FALSE(captured_samples_[0].has_side_data_pointer);
  EXPECT_EQ(captured_samples_[0].side_data_count, 0);
}

// WriteBuffersInternal() pushes a side data entry for every buffer and then
// takes the address of element |i|, so the batch-wide vector must not
// reallocate mid-loop; only its up-front reserve() prevents that. A batch with
// a gap in the middle therefore guards two distinct regressions: losing the
// reserve() (earlier samples left pointing at freed memory), and switching to
// "append only when side data exists" (which desynchronizes index |i| from the
// buffer index and reads out of bounds).
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

constexpr char kSbPlayerCreateHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreate";
constexpr char kSbPlayerCreateSubsequentHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreate.Subsequent";
constexpr char kSbPlayerCreateToInitializedHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreateToInitialized";
constexpr char kSbPlayerCreateToInitializedSubsequentHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreateToInitialized.Subsequent";
// Prefix of all the SbPlayerDestroy histograms.
constexpr char kSbPlayerDestroyHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy";
constexpr char kSbPlayerDestroyBeforeInitializedHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy.BeforeInitialized";
constexpr char kSbPlayerDestroyIdleHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy.Idle";
constexpr char kSbPlayerDestroyActiveHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy.Active";

// Returns the number of samples recorded in histogram |name|.
size_t GetTotalCount(const base::HistogramTester& histogram_tester,
                     const std::string& name) {
  size_t count = 0;
  for (const base::Bucket& bucket : histogram_tester.GetAllSamples(name)) {
    count += bucket.count;
  }
  return count;
}

// Returns the number of samples recorded in |first_call_histogram| and
// |subsequent_call_histogram|. Which of the two a sample lands in depends on
// whether an earlier test in the same process already created an SbPlayer.
size_t GetTotalCountAcrossOccurrences(
    const base::HistogramTester& histogram_tester,
    const std::string& first_call_histogram,
    const std::string& subsequent_call_histogram) {
  return GetTotalCount(histogram_tester, first_call_histogram) +
         GetTotalCount(histogram_tester, subsequent_call_histogram);
}

std::unique_ptr<SbPlayerBridge> CreateTestBridge(
    MockSbPlayerInterface* sbplayer_interface,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    SbPlayerBridge::Host* host) {
  const StarboardRendererConfig::ExperimentalFeatures experimental_features{};
  return std::make_unique<SbPlayerBridge>(
      sbplayer_interface, task_runner,
      base::BindRepeating(
          []() -> SbDecodeTargetGraphicsContextProvider* { return nullptr; }),
      AudioDecoderConfig(), TestVideoConfig::Normal(), kSbWindowInvalid,
      kSbDrmSystemInvalid, host,
      /*allow_resume_after_suspend=*/false, kSbPlayerOutputModePunchOut,
      /*max_video_capabilities=*/"",
      /*max_video_resolution=*/"",
      /*max_video_input_size=*/0, experimental_features
#if BUILDFLAG(IS_ANDROID)
      ,
      /*surface_view=*/nullptr
#endif  // BUILDFLAG(IS_ANDROID)
  );
}

// Exercises the playback statistics reported by SbPlayerBridge::GetInfo().
// Instantiated per test by GoogleTest and runs on the main test thread.
class SbPlayerBridgeStatisticsTest : public testing::Test {
 protected:
  SbPlayer CreateBridge() {
    SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
    // SbPlayerBridge's constructor calls CreatePlayer() synchronously.
    EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
        .WillOnce(Return(player));
    bridge_ =
        CreateTestBridge(&mock_sbplayer_interface_,
                         task_environment_.GetMainThreadTaskRunner(), &host_);
    EXPECT_TRUE(bridge_->IsValid());
    return player;
  }

  // Makes SbPlayerGetInfo() report |video_frames_decoded| for |player|.
  void SetVideoFramesDecoded(SbPlayer player, int video_frames_decoded) {
    EXPECT_CALL(mock_sbplayer_interface_, GetInfo(player, _))
        .WillRepeatedly(Invoke([=](SbPlayer /*player*/, SbPlayerInfo* info) {
          *info = SbPlayerInfo{};
          info->total_video_frames = video_frames_decoded;
          info->duration = SB_PLAYER_NO_DURATION;
        }));
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockSbPlayerInterface> mock_sbplayer_interface_;
  FakeSbPlayerBridgeHost host_;
  std::unique_ptr<SbPlayerBridge> bridge_;
};

// The SbPlayer created on Resume() restarts its counters at 0. The frames
// counted by the SbPlayer destroyed on Suspend() must be kept, even when the
// new SbPlayer's first reported value isn't lower than the old one's.
TEST_F(SbPlayerBridgeStatisticsTest, FrameTotalsContinueAcrossSuspendResume) {
  constexpr int kFramesDecodedBeforeSuspend = 5;
  // Higher than kFramesDecodedBeforeSuspend, so the reset of the counters
  // can't be detected from the reported values alone.
  constexpr int kFramesDecodedAfterResume = 12;
  constexpr uint32_t kTotalFramesDecoded =
      kFramesDecodedBeforeSuspend + kFramesDecodedAfterResume;

  SbPlayer first_player = CreateBridge();
  SetVideoFramesDecoded(first_player, kFramesDecodedBeforeSuspend);
  EXPECT_EQ(bridge_->GetInfo().statistics.total_video_frames_decoded,
            static_cast<uint32_t>(kFramesDecodedBeforeSuspend));

  bridge_->Suspend();
  EXPECT_EQ(bridge_->GetInfo().statistics.total_video_frames_decoded,
            static_cast<uint32_t>(kFramesDecodedBeforeSuspend));

  SbPlayer second_player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(Return(second_player));
  bridge_->Resume(kSbWindowInvalid);
  SetVideoFramesDecoded(second_player, kFramesDecodedAfterResume);
  EXPECT_EQ(bridge_->GetInfo().statistics.total_video_frames_decoded,
            kTotalFramesDecoded);
}

// Repeated kSbPlayerStateInitialized statuses for the same SbPlayer must only
// record Cobalt.Media.LatencyTiming.SbPlayerCreateToInitialized once, in the
// same variant as the SbPlayerCreate() call that created the player.
TEST(SbPlayerBridgeLatencyTest, RecordsCreateToInitializedOncePerPlayer) {
  constexpr int kInitializedStatusCount = 2;

  base::test::TaskEnvironment task_environment;
  NiceMock<MockSbPlayerInterface> mock_sbplayer_interface;
  FakeSbPlayerBridgeHost host;
  base::HistogramTester histogram_tester;

  SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
  SbPlayerStatusFunc player_status_func = nullptr;
  void* context = nullptr;
  EXPECT_CALL(mock_sbplayer_interface, Create(_, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<4>(&player_status_func), SaveArg<6>(&context),
                      Return(player)));
  std::unique_ptr<SbPlayerBridge> bridge =
      CreateTestBridge(&mock_sbplayer_interface,
                       task_environment.GetMainThreadTaskRunner(), &host);
  ASSERT_TRUE(bridge->IsValid());
  ASSERT_TRUE(player_status_func);
  EXPECT_EQ(
      GetTotalCountAcrossOccurrences(histogram_tester, kSbPlayerCreateHistogram,
                                     kSbPlayerCreateSubsequentHistogram),
      1u);

  for (int i = 0; i < kInitializedStatusCount; ++i) {
    player_status_func(player, context, kSbPlayerStateInitialized,
                       SB_PLAYER_INITIAL_TICKET);
    task_environment.RunUntilIdle();
  }
  EXPECT_EQ(GetTotalCountAcrossOccurrences(
                histogram_tester, kSbPlayerCreateToInitializedHistogram,
                kSbPlayerCreateToInitializedSubsequentHistogram),
            1u);
  // Both metrics for this player are recorded in the same variant.
  EXPECT_EQ(
      GetTotalCount(histogram_tester, kSbPlayerCreateHistogram),
      GetTotalCount(histogram_tester, kSbPlayerCreateToInitializedHistogram));
}

// Only the first SbPlayer creation in the process is recorded in the base
// histogram; any later one is recorded in the ".Subsequent" variant.
TEST(SbPlayerBridgeLatencyTest, RecordsLaterCreationsAsSubsequent) {
  constexpr size_t kPlayerCount = 2;

  base::test::TaskEnvironment task_environment;
  NiceMock<MockSbPlayerInterface> mock_sbplayer_interface;
  FakeSbPlayerBridgeHost host;
  base::HistogramTester histogram_tester;

  for (size_t i = 0; i < kPlayerCount; ++i) {
    EXPECT_CALL(mock_sbplayer_interface, Create(_, _, _, _, _, _, _, _))
        .WillOnce(Return(reinterpret_cast<SbPlayer>(new MockSbPlayer())));
    std::unique_ptr<SbPlayerBridge> bridge =
        CreateTestBridge(&mock_sbplayer_interface,
                         task_environment.GetMainThreadTaskRunner(), &host);
    ASSERT_TRUE(bridge->IsValid());
  }

  // At most one creation can be the first in the process, so all the others
  // are recorded as subsequent.
  const size_t first_call_count =
      GetTotalCount(histogram_tester, kSbPlayerCreateHistogram);
  EXPECT_LE(first_call_count, 1u);
  EXPECT_EQ(GetTotalCount(histogram_tester, kSbPlayerCreateSubsequentHistogram),
            kPlayerCount - first_call_count);
}

// Exercises the Cobalt.Media.LatencyTiming.SbPlayerDestroy* histograms, which
// are split by the state of the player when it is destroyed.
// Instantiated per test by GoogleTest and runs on the main test thread.
class SbPlayerBridgeDestroyLatencyTest : public testing::Test {
 protected:
  // Creates |bridge_| with |player|, and saves the player status callback.
  void CreateBridge(SbPlayer player) {
    EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<4>(&player_status_func_), SaveArg<6>(&context_),
                        Return(player)));
    bridge_ =
        CreateTestBridge(&mock_sbplayer_interface_,
                         task_environment_.GetMainThreadTaskRunner(), &host_);
    ASSERT_TRUE(bridge_->IsValid());
  }

  // Makes |player| report kSbPlayerStateInitialized to |bridge_|.
  void ReportInitialized(SbPlayer player) {
    ASSERT_TRUE(player_status_func_);
    player_status_func_(player, context_, kSbPlayerStateInitialized,
                        SB_PLAYER_INITIAL_TICKET);
    task_environment_.RunUntilIdle();
  }

  // Writes one video sample to the player of |bridge_|.
  void WriteSample() {
    constexpr uint8_t kPayload[] = {0x01, 0x02, 0x03, 0x04};
    bridge_->WriteBuffers(DemuxerStream::VIDEO,
                          {MakeVideoBuffer(kPayload, /*block_additional=*/{})});
  }

  // Suspends |bridge_|, which destroys its player, then resumes it with
  // |player| as the new player.
  void SuspendAndResumeWith(SbPlayer player) {
    bridge_->Suspend();
    EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
        .WillOnce(Return(player));
    bridge_->Resume(kSbWindowInvalid);
  }

  // Returns the number of samples recorded in each SbPlayerDestroy histogram.
  base::HistogramTester::CountsMap GetDestroyCounts() const {
    return histogram_tester_.GetTotalCountsForPrefix(kSbPlayerDestroyHistogram);
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockSbPlayerInterface> mock_sbplayer_interface_;
  FakeSbPlayerBridgeHost host_;
  base::HistogramTester histogram_tester_;
  const std::unique_ptr<MockSbPlayer> first_mock_player_ =
      std::make_unique<MockSbPlayer>();
  const SbPlayer first_player_ =
      reinterpret_cast<SbPlayer>(first_mock_player_.get());
  SbPlayerStatusFunc player_status_func_ = nullptr;
  void* context_ = nullptr;
  std::unique_ptr<SbPlayerBridge> bridge_;
};

// A player destroyed before it reports kSbPlayerStateInitialized is recorded
// as ".BeforeInitialized".
TEST_F(SbPlayerBridgeDestroyLatencyTest, RecordsTeardownBeforeInitialized) {
  CreateBridge(first_player_);

  EXPECT_CALL(mock_sbplayer_interface_, Destroy(first_player_)).Times(1);
  bridge_.reset();

  const base::HistogramTester::CountsMap expected_counts = {
      {kSbPlayerDestroyBeforeInitializedHistogram, 1}};
  EXPECT_THAT(GetDestroyCounts(), ContainerEq(expected_counts));
}

// An initialized player without any sample written to it, the state measured
// by the Starboard BM_PlayerDestroy benchmark, is recorded as ".Idle".
TEST_F(SbPlayerBridgeDestroyLatencyTest, RecordsTeardownWhenIdle) {
  CreateBridge(first_player_);
  ReportInitialized(first_player_);

  bridge_.reset();

  const base::HistogramTester::CountsMap expected_counts = {
      {kSbPlayerDestroyIdleHistogram, 1}};
  EXPECT_THAT(GetDestroyCounts(), ContainerEq(expected_counts));
}

// An initialized player with samples written to it is recorded as ".Active".
TEST_F(SbPlayerBridgeDestroyLatencyTest, RecordsTeardownWhenActive) {
  CreateBridge(first_player_);
  ReportInitialized(first_player_);
  WriteSample();

  bridge_.reset();

  const base::HistogramTester::CountsMap expected_counts = {
      {kSbPlayerDestroyActiveHistogram, 1}};
  EXPECT_THAT(GetDestroyCounts(), ContainerEq(expected_counts));
}

// Suspend() records the latency of the SbPlayerDestroy() call it makes, in the
// same histograms as when SbPlayerBridge is destroyed. Destroying the suspended
// SbPlayerBridge doesn't record anything else, as there is no player left to
// destroy.
TEST_F(SbPlayerBridgeDestroyLatencyTest, RecordsSuspendOnce) {
  CreateBridge(first_player_);
  ReportInitialized(first_player_);
  WriteSample();

  EXPECT_CALL(mock_sbplayer_interface_, Destroy(first_player_)).Times(1);
  bridge_->Suspend();
  bridge_.reset();

  const base::HistogramTester::CountsMap expected_counts = {
      {kSbPlayerDestroyActiveHistogram, 1}};
  EXPECT_THAT(GetDestroyCounts(), ContainerEq(expected_counts));
}

// The player created on Resume() is recorded as ".BeforeInitialized" until it
// reports kSbPlayerStateInitialized, even though the player destroyed on
// Suspend() was initialized.
TEST_F(SbPlayerBridgeDestroyLatencyTest,
       ResetsInitializationForPlayerCreatedOnResume) {
  CreateBridge(first_player_);
  ReportInitialized(first_player_);
  WriteSample();

  auto second_mock_player = std::make_unique<MockSbPlayer>();
  const SbPlayer second_player =
      reinterpret_cast<SbPlayer>(second_mock_player.get());
  SuspendAndResumeWith(second_player);

  EXPECT_CALL(mock_sbplayer_interface_, Destroy(second_player)).Times(1);
  bridge_.reset();

  const base::HistogramTester::CountsMap expected_counts = {
      {kSbPlayerDestroyActiveHistogram, 1},
      {kSbPlayerDestroyBeforeInitializedHistogram, 1}};
  EXPECT_THAT(GetDestroyCounts(), ContainerEq(expected_counts));
}

// Once initialized, the player created on Resume() is recorded as ".Idle"
// until samples are written to it, even though samples were written to the
// player destroyed on Suspend().
TEST_F(SbPlayerBridgeDestroyLatencyTest,
       ResetsWrittenSamplesForPlayerCreatedOnResume) {
  CreateBridge(first_player_);
  ReportInitialized(first_player_);
  WriteSample();

  auto second_mock_player = std::make_unique<MockSbPlayer>();
  const SbPlayer second_player =
      reinterpret_cast<SbPlayer>(second_mock_player.get());
  SuspendAndResumeWith(second_player);
  // The status callback and its context are the same for every player created
  // by |bridge_|.
  ReportInitialized(second_player);

  EXPECT_CALL(mock_sbplayer_interface_, Destroy(second_player)).Times(1);
  bridge_.reset();

  const base::HistogramTester::CountsMap expected_counts = {
      {kSbPlayerDestroyActiveHistogram, 1}, {kSbPlayerDestroyIdleHistogram, 1}};
  EXPECT_THAT(GetDestroyCounts(), ContainerEq(expected_counts));
}

// No SbPlayerDestroy() latency is recorded if the player was never created.
TEST(SbPlayerBridgeLatencyTest, DoesNotRecordDestroyForInvalidPlayer) {
  base::test::TaskEnvironment task_environment;
  NiceMock<MockSbPlayerInterface> mock_sbplayer_interface;
  FakeSbPlayerBridgeHost host;
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_sbplayer_interface, Create(_, _, _, _, _, _, _, _))
      .WillOnce(Return(kSbPlayerInvalid));
  EXPECT_CALL(mock_sbplayer_interface, Destroy(_)).Times(0);
  std::unique_ptr<SbPlayerBridge> bridge =
      CreateTestBridge(&mock_sbplayer_interface,
                       task_environment.GetMainThreadTaskRunner(), &host);
  ASSERT_FALSE(bridge->IsValid());

  bridge.reset();
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix(kSbPlayerDestroyHistogram),
      testing::IsEmpty());
}

}  // namespace

}  // namespace media
