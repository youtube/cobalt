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

#include "cobalt/starboard_wasm/wasm_player_host.h"

#include <string.h>

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "cobalt/starboard_wasm/event_queue.h"
#include "cobalt/starboard_wasm/guest_memory.h"
#include "cobalt/starboard_wasm/surface_video_size.h"
#include "cobalt/starboard_wasm/wasm_starboard_abi.h"
#include "media/base/starboard/sbmedia_interface.h"
#include "media/starboard/mock_sbplayer_interface.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace starboard_wasm {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

constexpr uint32_t kParam = 0x100;
constexpr uint32_t kMime = 0x400;
constexpr uint32_t kInfos = 0x800;
constexpr uint32_t kBuffer = 0x4000;
constexpr uint32_t kOut = 0x6000;
constexpr uint32_t kMemorySize = 65536;
constexpr uint32_t kParamSize = sizeof(WasmSbPlayerCreationParam);
constexpr int kMaxSamplesPerWrite = 8;
constexpr char kAudioMime[] = "audio/webm; codecs=\"opus\"";

class MockSbMediaInterface : public ::media::SbMediaInterface {
 public:
  MOCK_METHOD(SbMediaSupportType,
              CanPlayMimeAndKeySystem,
              (const char*, const char*),
              (const, override));
  MOCK_METHOD(bool,
              CanChangeType,
              (const char*, const char*),
              (const, override));
  MOCK_METHOD(int, GetAudioOutputCount, (), (const, override));
  MOCK_METHOD(bool,
              GetAudioConfiguration,
              (int, SbMediaAudioConfiguration*),
              (const, override));
  MOCK_METHOD(int, GetBufferAllocationUnit, (), (const, override));
  MOCK_METHOD(int, GetAudioBufferBudget, (), (const, override));
  MOCK_METHOD(int64_t,
              GetBufferGarbageCollectionDurationThreshold,
              (),
              (const, override));
  MOCK_METHOD(int, GetInitialBufferCapacity, (), (const, override));
  MOCK_METHOD(bool, IsBufferPoolAllocateOnDemand, (), (const, override));
  MOCK_METHOD(int,
              GetVideoBufferBudget,
              (SbMediaVideoCodec, int, int, int),
              (const, override));
};

std::vector<std::map<std::string, int64_t>>& FeatureCalls() {
  static base::NoDestructor<std::vector<std::map<std::string, int64_t>>> calls;
  return *calls;
}

void SetFeatures(const StarboardExtensionExperimentalFeatures* features) {
  std::map<std::string, int64_t> map;
  for (size_t i = 0; i < features->entry_count; ++i) {
    map[features->entries[i].key] = features->entries[i].value;
  }
  FeatureCalls().push_back(std::move(map));
}

constexpr StarboardExtensionExperimentalFeaturesConfigurationApi kFeaturesApi =
    {kStarboardExtensionExperimentalFeaturesConfigurationName, 1, &SetFeatures};

// Stands in for the compositor's VideoGeometrySetterService.
class FakeGeometrySubscriber
    : public cobalt::media::mojom::VideoGeometryChangeSubscriber {
 public:
  void SubscribeToVideoGeometryChange(
      const base::UnguessableToken& plane,
      mojo::PendingRemote<cobalt::media::mojom::VideoGeometryChangeClient>
          client,
      SubscribeToVideoGeometryChangeCallback callback) override {
    clients[plane.ToString()].Bind(std::move(client));
    std::move(callback).Run();
  }

  mojo::ReceiverSet<cobalt::media::mojom::VideoGeometryChangeSubscriber>
      receivers;
  std::map<std::string,
           mojo::Remote<cobalt::media::mojom::VideoGeometryChangeClient>>
      clients;
};

// The PlayerSettings calls of the current test, or nullopt for none.
std::optional<int>& MaxVideoInputSize() {
  static std::optional<int> size;
  return size;
}
std::optional<std::string>& MaxVideoResolution() {
  static base::NoDestructor<std::optional<std::string>> resolution;
  return *resolution;
}

constexpr StarboardExtensionPlayerSettingsApi kPlayerSettingsApi = {
    kStarboardExtensionPlayerSettingsName, 1,
    [](int size) { MaxVideoInputSize() = size; },
    [](const char* resolution) {
      MaxVideoResolution() = resolution ? resolution : "<null>";
    },
    nullptr};

class WasmPlayerHostTest : public testing::Test {
 protected:
  WasmPlayerHostTest()
      : memory_(kMemorySize),
        events_(base::MakeRefCounted<EventQueue>(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::DoNothing())),
        host_(std::make_unique<WasmPlayerHost>(events_, sbplayer_)) {
    ON_CALL(sbplayer_, Create)
        .WillByDefault(Invoke(this, &WasmPlayerHostTest::OnCreate));
    // Overrides MockSbPlayerInterface's own expectation.
    EXPECT_CALL(sbplayer_, GetMaximumNumberOfSamplesPerWrite)
        .WillRepeatedly(Return(kMaxSamplesPerWrite));
    ::media::SetSbMediaInterfaceForTesting(&sbmedia_);
    WasmPlayerHost::SetExperimentalFeaturesApiForTesting(&kFeaturesApi);
    FeatureCalls().clear();
    WasmPlayerHost::SetPlayerSettingsApiForTesting(&kPlayerSettingsApi);
    MaxVideoInputSize().reset();
    MaxVideoResolution().reset();
    WasmPlayerHost::SetVideoGeometrySubscriberBinder(base::BindLambdaForTesting(
        [this](mojo::PendingReceiver<
               cobalt::media::mojom::VideoGeometryChangeSubscriber> receiver) {
          geometry_.receivers.Add(&geometry_, std::move(receiver));
        }));
  }

  ~WasmPlayerHostTest() override {
    host_.reset();
    task_environment_.RunUntilIdle();
    WasmPlayerHost::SetVideoGeometrySubscriberBinder({});
    WasmPlayerHost::SetPlayerSettingsApiForTesting(nullptr);
    WasmPlayerHost::SetExperimentalFeaturesApiForTesting(nullptr);
    ::media::SetSbMediaInterfaceForTesting(nullptr);
  }

  SbPlayer OnCreate(SbWindow,
                    const SbPlayerCreationParam* param,
                    SbPlayerDeallocateSampleFunc deallocate_sample,
                    SbPlayerDecoderStatusFunc decoder_status,
                    SbPlayerStatusFunc player_status,
                    SbPlayerErrorFunc player_error,
                    void* context,
                    SbDecodeTargetGraphicsContextProvider*) {
    created_audio_codec_ = param->audio_stream_info.codec;
    created_audio_mime_ = param->audio_stream_info.mime;
    deallocate_sample_ = deallocate_sample;
    decoder_status_ = decoder_status;
    player_status_ = player_status;
    player_error_ = player_error;
    context_ = context;
    sb_player_ = reinterpret_cast<SbPlayer>(new ::media::MockSbPlayer());
    return sb_player_;
  }

  GuestMemory Memory() { return GuestMemory(base::span(memory_)); }

  template <typename T>
  void Write(uint32_t offset, const T& value) {
    memcpy(memory_.data() + offset, &value, sizeof(value));
  }

  template <typename T>
  T ReadBack(uint32_t offset) {
    T value;
    memcpy(&value, memory_.data() + offset, sizeof(value));
    return value;
  }

  static WasmSbPlayerCreationParam AudioCreationParam() {
    WasmSbPlayerCreationParam param = {};
    param.audio_stream_info.codec = kSbMediaAudioCodecOpus;
    param.audio_stream_info.mime = kMime;
    param.audio_stream_info.mime_size = strlen(kAudioMime);
    param.audio_stream_info.number_of_channels = 2;
    param.audio_stream_info.samples_per_second = 48000;
    param.output_mode = kSbPlayerOutputModePunchOut;
    return param;
  }

  uint32_t Create(const WasmSbPlayerCreationParam& param,
                  uint32_t surface = 0) {
    memcpy(memory_.data() + kMime, kAudioMime, strlen(kAudioMime));
    Write(kParam, param);
    GuestMemory memory = Memory();
    return host_->Create(memory, surface, kParam, kParamSize);
  }

  uint32_t CreateAudioPlayer() { return Create(AudioCreationParam()); }

  static WasmSbPlayerSampleInfo AudioSample(uint32_t i) {
    WasmSbPlayerSampleInfo info = {};
    info.type = kSbMediaTypeAudio;
    info.buffer = kBuffer + i;
    info.buffer_size = 1;
    info.timestamp = 20000 * i;
    info.audio_sample_info.stream_info.codec = kSbMediaAudioCodecOpus;
    info.audio_sample_info.stream_info.number_of_channels = 2;
    info.audio_sample_info.stream_info.samples_per_second = 48000;
    return info;
  }

  // Writes `count` one-byte audio samples at kInfos.
  void WriteAudioSamples(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
      memory_[kBuffer + i] = static_cast<uint8_t>(0xa0 + i);
      Write(kInfos + i * sizeof(WasmSbPlayerSampleInfo), AudioSample(i));
    }
  }

  // Reports that Starboard wants data of `type`.
  void NeedData(SbMediaType type) {
    decoder_status_(sb_player_, context_, type, kSbPlayerDecoderStateNeedsData,
                    0);
  }

  bool WriteSamples(uint32_t player, int32_t type, uint32_t count) {
    GuestMemory memory = Memory();
    return host_->WriteSamples(memory, player, type, kInfos, count);
  }

  std::vector<WasmSbEvent> PollEvents() {
    std::vector<WasmSbEvent> events(16);
    events.resize(events_->Poll(base::as_writable_bytes(base::span(events))));
    return events;
  }

  // Reports `rect` as the geometry of `surface`'s hole.
  void MoveHole(uint32_t surface, const gfx::RectF& rect) {
    task_environment_.RunUntilIdle();
    auto it = geometry_.clients.find(host_->SurfaceToken(surface));
    ASSERT_NE(it, geometry_.clients.end());
    it->second->OnVideoGeometryChange(rect, gfx::OVERLAY_TRANSFORM_NONE);
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  FakeGeometrySubscriber geometry_;
  NiceMock<::media::MockSbPlayerInterface> sbplayer_;
  NiceMock<MockSbMediaInterface> sbmedia_;
  std::vector<uint8_t> memory_;
  scoped_refptr<EventQueue> events_;
  std::unique_ptr<WasmPlayerHost> host_;

  SbPlayer sb_player_ = kSbPlayerInvalid;
  SbMediaAudioCodec created_audio_codec_ = kSbMediaAudioCodecNone;
  std::string created_audio_mime_;
  SbPlayerDeallocateSampleFunc deallocate_sample_ = nullptr;
  SbPlayerDecoderStatusFunc decoder_status_ = nullptr;
  SbPlayerStatusFunc player_status_ = nullptr;
  SbPlayerErrorFunc player_error_ = nullptr;
  void* context_ = nullptr;
};

TEST_F(WasmPlayerHostTest, CreatesAndDestroysPlayer) {
  const uint32_t player = CreateAudioPlayer();
  ASSERT_NE(player, 0u);
  EXPECT_EQ(created_audio_codec_, kSbMediaAudioCodecOpus);
  EXPECT_EQ(created_audio_mime_, kAudioMime);

  EXPECT_CALL(sbplayer_, Destroy(sb_player_)).WillOnce(Invoke([](SbPlayer p) {
    delete reinterpret_cast<::media::MockSbPlayer*>(p);
  }));
  host_->Destroy(player);
  task_environment_.RunUntilIdle();
  EXPECT_NE(CreateAudioPlayer(), player);
}

TEST_F(WasmPlayerHostTest, SetsExperimentalFeatures) {
  memcpy(memory_.data() + kMime, "ab", 3);
  Write(kOut, WasmStarboardExperimentalFeatureEntry{kMime, 1, 3});
  Write(kOut + 16, WasmStarboardExperimentalFeatureEntry{kMime, 2, -1});
  GuestMemory memory = Memory();
  ASSERT_TRUE(
      WasmPlayerHost::SetExperimentalFeaturesForCurrentThread(memory, kOut, 2));
  ASSERT_EQ(FeatureCalls().size(), 1u);
  EXPECT_EQ(FeatureCalls()[0],
            (std::map<std::string, int64_t>{{"a", 3}, {"ab", -1}}));

  ASSERT_NE(CreateAudioPlayer(), 0u);
  EXPECT_EQ(FeatureCalls().size(), 1u);
}

TEST_F(WasmPlayerHostTest, RejectsOutOfBoundsExperimentalFeatures) {
  Write(kOut, WasmStarboardExperimentalFeatureEntry{kMemorySize - 1, 2, 3});
  GuestMemory memory = Memory();
  EXPECT_FALSE(
      WasmPlayerHost::SetExperimentalFeaturesForCurrentThread(memory, kOut, 1));
  EXPECT_FALSE(WasmPlayerHost::SetExperimentalFeaturesForCurrentThread(
      memory, kMemorySize - 8, 1));
  EXPECT_TRUE(FeatureCalls().empty());
}

TEST_F(WasmPlayerHostTest, CreateSetsEmptyFeaturesIfNoneWereSet) {
  ASSERT_NE(CreateAudioPlayer(), 0u);
  ASSERT_EQ(FeatureCalls().size(), 1u);
  EXPECT_TRUE(FeatureCalls()[0].empty());
  ASSERT_NE(CreateAudioPlayer(), 0u);
  EXPECT_EQ(FeatureCalls().size(), 1u);
}

TEST_F(WasmPlayerHostTest, SetsPlayerSettings) {
  ASSERT_TRUE(WasmPlayerHost::SetMaxVideoInputSizeForCurrentThread(1024));
  EXPECT_EQ(MaxVideoInputSize(), 1024);

  memcpy(memory_.data() + kMime, "1920x1080", 9);
  GuestMemory memory = Memory();
  ASSERT_TRUE(
      WasmPlayerHost::SetMaxVideoResolutionForCurrentThread(memory, kMime, 9));
  EXPECT_EQ(MaxVideoResolution(), "1920x1080");
  ASSERT_TRUE(
      WasmPlayerHost::SetMaxVideoResolutionForCurrentThread(memory, kMime, 0));
  EXPECT_EQ(MaxVideoResolution(), "<null>");
  EXPECT_FALSE(WasmPlayerHost::SetMaxVideoResolutionForCurrentThread(
      memory, kMemorySize - 1, 2));

  constexpr StarboardExtensionPlayerSettingsApi kEmptyApi = {
      kStarboardExtensionPlayerSettingsName, 1, nullptr, nullptr, nullptr};
  WasmPlayerHost::SetPlayerSettingsApiForTesting(&kEmptyApi);
  EXPECT_FALSE(WasmPlayerHost::SetMaxVideoInputSizeForCurrentThread(1));
  EXPECT_FALSE(
      WasmPlayerHost::SetMaxVideoResolutionForCurrentThread(memory, kMime, 9));
}

TEST_F(WasmPlayerHostTest, ResetsExperimentalFeatures) {
  Write(kOut, WasmStarboardExperimentalFeatureEntry{kMime, 1, 3});
  memory_[kMime] = 'a';
  GuestMemory memory = Memory();
  ASSERT_TRUE(
      WasmPlayerHost::SetExperimentalFeaturesForCurrentThread(memory, kOut, 1));
  ASSERT_TRUE(WasmPlayerHost::SetMaxVideoInputSizeForCurrentThread(1024));
  WasmPlayerHost::ResetForCurrentThread();
  ASSERT_EQ(FeatureCalls().size(), 2u);
  EXPECT_TRUE(FeatureCalls()[1].empty());
  EXPECT_EQ(MaxVideoInputSize(), 0);
  EXPECT_EQ(MaxVideoResolution(), "<null>");
  // The next document's first player sets features again.
  ASSERT_NE(CreateAudioPlayer(), 0u);
  EXPECT_EQ(FeatureCalls().size(), 3u);
}

TEST_F(WasmPlayerHostTest, RejectsInvalidCreationParams) {
  using Mutation = std::function<void(WasmSbPlayerCreationParam&)>;
  const auto vp9 = [](int32_t width, int32_t height, uint32_t bits) {
    return [=](WasmSbPlayerCreationParam& p) {
      p.video_stream_info.codec = kSbMediaVideoCodecVp9;
      p.video_stream_info.frame_width = width;
      p.video_stream_info.frame_height = height;
      p.video_stream_info.color_metadata.bits_per_channel = bits;
    };
  };
  const std::vector<Mutation> mutations = {
      [](auto& p) { p.drm_system = 1; },
      [](auto& p) { p.audio_stream_info.codec = 99; },
      [](auto& p) { p.audio_stream_info.samples_per_second = 0; },
      [](auto& p) { p.audio_stream_info.samples_per_second = 0x80000000u; },
      [](auto& p) { p.audio_stream_info.number_of_channels = 0; },
      [](auto& p) { p.audio_stream_info.mime = kMemorySize; },
      [](auto& p) { p.output_mode = 99; },
      vp9(1920, 1080, 9),
      vp9(0, 1080, 8),
      vp9(1920, -1, 10),
      [](auto& p) { p.output_mode = kSbPlayerOutputModeDecodeToTexture; },
  };
  EXPECT_CALL(sbplayer_, Create).Times(0);
  for (size_t i = 0; i < mutations.size(); ++i) {
    WasmSbPlayerCreationParam param = AudioCreationParam();
    mutations[i](param);
    EXPECT_EQ(Create(param), 0u) << "mutation " << i;
  }
  Write(kParam, AudioCreationParam());
  GuestMemory memory = Memory();
  EXPECT_EQ(host_->Create(memory, 0, kParam, 4), 0u);
  EXPECT_EQ(host_->Create(memory, 0, kMemorySize - 8, kParamSize), 0u);
  EXPECT_EQ(host_->Create(memory, /*surface=*/7, kParam, kParamSize), 0u);
}

TEST_F(WasmPlayerHostTest, ReturnsZeroWhenStarboardFails) {
  EXPECT_CALL(sbplayer_, Create)
      .WillOnce(
          Invoke([](SbWindow, const SbPlayerCreationParam*,
                    SbPlayerDeallocateSampleFunc, SbPlayerDecoderStatusFunc,
                    SbPlayerStatusFunc, SbPlayerErrorFunc player_error,
                    void* context, SbDecodeTargetGraphicsContextProvider*) {
            player_error(kSbPlayerInvalid, context, kSbPlayerErrorDecode, "x");
            return kSbPlayerInvalid;
          }));
  EXPECT_EQ(CreateAudioPlayer(), 0u);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(PollEvents().empty());
}

TEST_F(WasmPlayerHostTest, PassesSeekThrough) {
  const uint32_t player = CreateAudioPlayer();
  EXPECT_CALL(sbplayer_, Seek(sb_player_, base::Microseconds(1500), 5));
  EXPECT_TRUE(host_->Seek(player, 1500, 5));
  EXPECT_CALL(sbplayer_, Seek(sb_player_, base::Microseconds(-1), 5));
  EXPECT_TRUE(host_->Seek(player, -1, 5));

  EXPECT_CALL(sbplayer_, Seek).Times(0);
  EXPECT_FALSE(host_->Seek(player, std::nan(""), 6));
  EXPECT_FALSE(host_->Seek(player, 1e17, 6));
  EXPECT_FALSE(host_->Seek(player + 1, 0, 6));
}

TEST_F(WasmPlayerHostTest, QueuesRawStarboardEvents) {
  const uint32_t player = CreateAudioPlayer();
  player_status_(sb_player_, context_, kSbPlayerStatePresenting, 3);
  decoder_status_(sb_player_, context_, kSbMediaTypeAudio,
                  kSbPlayerDecoderStateNeedsData, 3);
  player_error_(sb_player_, context_, kSbPlayerErrorCapabilityChanged, "x");
  task_environment_.RunUntilIdle();

  const std::vector<WasmSbEvent> events = PollEvents();
  ASSERT_EQ(events.size(), 3u);
  EXPECT_EQ(events[0].kind, kWasmSbEventPlayerStatus);
  EXPECT_EQ(events[0].player, player);
  EXPECT_EQ(events[0].a, kSbPlayerStatePresenting);
  EXPECT_EQ(events[0].b, 3);
  EXPECT_EQ(events[1].kind, kWasmSbEventDecoderStatus);
  EXPECT_EQ(events[1].a, kSbMediaTypeAudio);
  EXPECT_EQ(events[1].b, kSbPlayerDecoderStateNeedsData);
  EXPECT_EQ(events[1].c, 3);
  EXPECT_EQ(events[2].kind, kWasmSbEventPlayerError);
  EXPECT_EQ(events[2].a, kSbPlayerErrorCapabilityChanged);
}

TEST_F(WasmPlayerHostTest, KeepsLastErrorMessage) {
  const uint32_t player = CreateAudioPlayer();
  GuestMemory memory = Memory();
  EXPECT_EQ(host_->GetErrorMessage(memory, player, kOut, 16), 0u);

  player_error_(sb_player_, context_, kSbPlayerErrorDecode, "first");
  player_error_(sb_player_, context_, kSbPlayerErrorDecode, "boom!");
  memset(memory_.data() + kOut, 0, 8);
  EXPECT_EQ(host_->GetErrorMessage(memory, player, kOut, 3), 5u);
  EXPECT_EQ(std::string(reinterpret_cast<char*>(memory_.data() + kOut), 4),
            std::string("boo\0", 4));
  EXPECT_EQ(host_->GetErrorMessage(memory, player, 0, 0), 5u);
  EXPECT_EQ(host_->GetErrorMessage(memory, player, kMemorySize - 1, 16), 0u);
  EXPECT_EQ(host_->GetErrorMessage(memory, player + 1, kOut, 16), 0u);
}

TEST_F(WasmPlayerHostTest, ReportsOnlyDestroyedEventAfterDestroy) {
  const uint32_t player = CreateAudioPlayer();
  SbPlayer sb_player = sb_player_;
  void* context = context_;
  player_status_(sb_player, context, kSbPlayerStatePresenting, 0);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(sbplayer_, Destroy(sb_player)).WillOnce(Invoke([&](SbPlayer p) {
    player_status_(p, context, kSbPlayerStateDestroyed, 0);
    delete reinterpret_cast<::media::MockSbPlayer*>(p);
  }));
  host_->Destroy(player);
  EXPECT_TRUE(PollEvents().empty());
  task_environment_.RunUntilIdle();
  const std::vector<WasmSbEvent> events = PollEvents();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].kind, kWasmSbEventPlayerDestroyed);
  EXPECT_EQ(events[0].player, player);
  EXPECT_EQ(events[0].a, 0);
}

TEST_F(WasmPlayerHostTest, CopiesSamples) {
  const uint32_t player = CreateAudioPlayer();
  WriteAudioSamples(2);
  EXPECT_CALL(sbplayer_, WriteSamples(sb_player_, kSbMediaTypeAudio, _, 2))
      .Times(2)
      .WillRepeatedly(
          Invoke([&](SbPlayer, SbMediaType, const SbPlayerSampleInfo* s, int) {
            EXPECT_EQ(*static_cast<const uint8_t*>(s[0].buffer), 0xa0);
            EXPECT_NE(s[0].buffer, memory_.data() + kBuffer);
            EXPECT_EQ(s[1].timestamp, 20000);
            EXPECT_EQ(s[1].audio_sample_info.stream_info.codec,
                      kSbMediaAudioCodecOpus);
          }));
  NeedData(kSbMediaTypeAudio);
  EXPECT_TRUE(WriteSamples(player, kSbMediaTypeAudio, 2));
  NeedData(kSbMediaTypeAudio);
  EXPECT_TRUE(WriteSamples(player, kSbMediaTypeAudio, 2));
  NeedData(kSbMediaTypeAudio);
  EXPECT_CALL(sbplayer_, WriteEndOfStream(sb_player_, kSbMediaTypeAudio));
  EXPECT_TRUE(host_->WriteEndOfStream(player, kSbMediaTypeAudio));
  EXPECT_FALSE(host_->WriteEndOfStream(player, 7));
}

TEST_F(WasmPlayerHostTest, WritesOnlyWhatStarboardAskedFor) {
  const uint32_t player = CreateAudioPlayer();
  WriteAudioSamples(kMaxSamplesPerWrite + 1);
  EXPECT_CALL(sbplayer_, WriteSamples).Times(0);
  EXPECT_CALL(sbplayer_, WriteEndOfStream).Times(0);
  EXPECT_FALSE(WriteSamples(player, kSbMediaTypeAudio, 1));
  EXPECT_FALSE(host_->WriteEndOfStream(player, kSbMediaTypeAudio));
  NeedData(kSbMediaTypeVideo);
  EXPECT_FALSE(WriteSamples(player, kSbMediaTypeAudio, 1));
  NeedData(kSbMediaTypeAudio);
  EXPECT_FALSE(
      WriteSamples(player, kSbMediaTypeAudio, kMaxSamplesPerWrite + 1));

  // One write per request; rejected writes don't use it up.
  EXPECT_CALL(sbplayer_, WriteSamples).Times(1);
  EXPECT_TRUE(WriteSamples(player, kSbMediaTypeAudio, kMaxSamplesPerWrite));
  EXPECT_FALSE(WriteSamples(player, kSbMediaTypeAudio, 1));
  EXPECT_FALSE(host_->WriteEndOfStream(player, kSbMediaTypeAudio));
  NeedData(kSbMediaTypeAudio);
  EXPECT_CALL(sbplayer_, WriteEndOfStream).Times(1);
  EXPECT_TRUE(host_->WriteEndOfStream(player, kSbMediaTypeAudio));
}

TEST_F(WasmPlayerHostTest, RejectedWriteKeepsRequest) {
  const uint32_t player = CreateAudioPlayer();
  NeedData(kSbMediaTypeAudio);
  EXPECT_CALL(sbplayer_, WriteSamples).Times(0);
  WriteAudioSamples(kMaxSamplesPerWrite + 1);
  EXPECT_FALSE(
      WriteSamples(player, kSbMediaTypeAudio, kMaxSamplesPerWrite + 1));
  WasmSbPlayerSampleInfo out_of_range = AudioSample(0);
  out_of_range.buffer = kMemorySize;
  Write(kInfos, out_of_range);
  EXPECT_FALSE(WriteSamples(player, kSbMediaTypeAudio, 1));

  WriteAudioSamples(1);
  EXPECT_CALL(sbplayer_, WriteSamples(sb_player_, kSbMediaTypeAudio, _, 1))
      .Times(1);
  EXPECT_TRUE(WriteSamples(player, kSbMediaTypeAudio, 1));
  EXPECT_FALSE(WriteSamples(player, kSbMediaTypeAudio, 1));
  EXPECT_EQ(host_->SampleCopiesForTesting(player), 1u);
}

TEST_F(WasmPlayerHostTest, RejectedEndOfStreamKeepsRequest) {
  const uint32_t player = CreateAudioPlayer();
  NeedData(kSbMediaTypeAudio);
  EXPECT_CALL(sbplayer_, WriteEndOfStream).Times(0);
  EXPECT_FALSE(host_->WriteEndOfStream(player, 7));
  EXPECT_FALSE(host_->WriteEndOfStream(player, kSbMediaTypeVideo));
  EXPECT_FALSE(host_->WriteEndOfStream(player + 1, kSbMediaTypeAudio));

  EXPECT_CALL(sbplayer_, WriteEndOfStream(sb_player_, kSbMediaTypeAudio))
      .Times(1);
  EXPECT_TRUE(host_->WriteEndOfStream(player, kSbMediaTypeAudio));
  EXPECT_FALSE(host_->WriteEndOfStream(player, kSbMediaTypeAudio));
}

TEST_F(WasmPlayerHostTest, RejectsInvalidSamples) {
  const uint32_t player = CreateAudioPlayer();
  EXPECT_CALL(sbplayer_, WriteSamples).Times(0);
  NeedData(kSbMediaTypeAudio);
  NeedData(kSbMediaTypeVideo);

  WriteAudioSamples(1);
  EXPECT_FALSE(WriteSamples(player, kSbMediaTypeAudio, 0));
  EXPECT_FALSE(WriteSamples(player, 7, 1));
  GuestMemory memory = Memory();
  EXPECT_FALSE(
      host_->WriteSamples(memory, player, kSbMediaTypeAudio, kMemorySize, 1));

  using Mutation = std::function<void(WasmSbPlayerSampleInfo&)>;
  const std::vector<Mutation> mutations = {
      [](auto& s) { s.buffer = kMemorySize; },
      [](auto& s) { s.buffer_size = 0; },
      [](auto& s) { s.timestamp = -1; },
      [](auto& s) { s.timestamp = (int64_t{1} << 53) + 1; },
      [](auto& s) {
        s.side_data_count = 1;
        s.side_data = kMemorySize;
      },
      [](auto& s) { s.drm_info = kBuffer; },
      [](auto& s) { s.audio_sample_info.stream_info.number_of_channels = 0; },
      [](auto& s) {
        s.audio_sample_info.stream_info.mime = kMemorySize - 1;
        s.audio_sample_info.stream_info.mime_size = 2;
      },
      [](auto& s) {
        s.audio_sample_info.stream_info.codec = kSbMediaAudioCodecNone;
      },
  };
  for (size_t i = 0; i < mutations.size(); ++i) {
    WasmSbPlayerSampleInfo info = AudioSample(0);
    mutations[i](info);
    Write(kInfos, info);
    EXPECT_FALSE(WriteSamples(player, kSbMediaTypeAudio, 1))
        << "mutation " << i;
  }

  // Video samples carry the same stream info checks as creation.
  const auto video_sample = [](int32_t codec, int32_t width, uint32_t bits) {
    WasmSbPlayerSampleInfo info = AudioSample(0);
    info.type = kSbMediaTypeVideo;
    info.video_sample_info = {};
    info.video_sample_info.stream_info.codec = codec;
    info.video_sample_info.stream_info.frame_width = width;
    info.video_sample_info.stream_info.frame_height = 1080;
    info.video_sample_info.stream_info.color_metadata.bits_per_channel = bits;
    return info;
  };
  for (const WasmSbPlayerSampleInfo& info :
       {video_sample(kSbMediaVideoCodecNone, 1920, 8),
        video_sample(kSbMediaVideoCodecVp9, 0, 8),
        video_sample(kSbMediaVideoCodecVp9, 1920, 9)}) {
    Write(kInfos, info);
    EXPECT_FALSE(WriteSamples(player, kSbMediaTypeVideo, 1));
  }

  // The rejected writes left the request for a valid one.
  WriteAudioSamples(1);
  EXPECT_CALL(sbplayer_, WriteSamples).Times(1);
  EXPECT_TRUE(WriteSamples(player, kSbMediaTypeAudio, 1));
}

TEST_F(WasmPlayerHostTest, PassesMaximumSamplesPerWriteThrough) {
  const uint32_t player = CreateAudioPlayer();
  EXPECT_CALL(sbplayer_, GetMaximumNumberOfSamplesPerWrite(_, _))
      .WillRepeatedly(Return(256));
  EXPECT_EQ(host_->GetMaximumNumberOfSamplesPerWrite(player, kSbMediaTypeAudio),
            256);
  EXPECT_EQ(host_->GetMaximumNumberOfSamplesPerWrite(player, 7), 0);
}

TEST_F(WasmPlayerHostTest, FreesSampleCopiesOnDeallocate) {
  const uint32_t player = CreateAudioPlayer();
  WriteAudioSamples(1);
  const void* buffer = nullptr;
  EXPECT_CALL(sbplayer_, WriteSamples)
      .WillOnce(Invoke([&](SbPlayer, SbMediaType, const SbPlayerSampleInfo* s,
                           int) { buffer = s[0].buffer; }));
  NeedData(kSbMediaTypeAudio);
  ASSERT_TRUE(WriteSamples(player, kSbMediaTypeAudio, 1));
  EXPECT_EQ(host_->SampleCopiesForTesting(player), 1u);

  deallocate_sample_(sb_player_, context_, buffer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(host_->SampleCopiesForTesting(player), 0u);
}

TEST_F(WasmPlayerHostTest, KeepsSampleCopiesUntilDestroyReturns) {
  const uint32_t player = CreateAudioPlayer();
  WriteAudioSamples(1);
  const uint8_t* buffer = nullptr;
  EXPECT_CALL(sbplayer_, WriteSamples)
      .WillOnce(
          Invoke([&](SbPlayer, SbMediaType, const SbPlayerSampleInfo* s, int) {
            buffer = static_cast<const uint8_t*>(s[0].buffer);
          }));
  NeedData(kSbMediaTypeAudio);
  ASSERT_TRUE(WriteSamples(player, kSbMediaTypeAudio, 1));

  memory_[kBuffer] = 0;
  bool destroyed = false;
  EXPECT_CALL(sbplayer_, Destroy(sb_player_)).WillOnce(Invoke([&](SbPlayer p) {
    EXPECT_EQ(*buffer, 0xa0);
    destroyed = true;
    delete reinterpret_cast<::media::MockSbPlayer*>(p);
  }));
  host_->Destroy(player);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(destroyed);
}

TEST_F(WasmPlayerHostTest, PassesPlaybackControlsThrough) {
  const uint32_t player = CreateAudioPlayer();
  EXPECT_CALL(sbplayer_, SetBounds(sb_player_, 1, 2, 3, 4, 5));
  host_->SetBounds(player, 1, 2, 3, 4, 5);

  EXPECT_CALL(sbplayer_, SetPlaybackRate(sb_player_, 0.0))
      .WillOnce(Return(true));
  EXPECT_TRUE(host_->SetPlaybackRate(player, 0));
  EXPECT_FALSE(host_->SetPlaybackRate(player, std::nan("")));
  EXPECT_FALSE(host_->SetPlaybackRate(player, 1e-300));
  EXPECT_FALSE(host_->SetPlaybackRate(player, 1e300));

  EXPECT_CALL(sbplayer_, SetVolume(sb_player_, 0.5));
  host_->SetVolume(player, 0.5);
  host_->SetVolume(player, 2);
}

TEST_F(WasmPlayerHostTest, HolePositionsPlayerUntilSurfaceDestroyed) {
  const uint32_t surface = host_->CreateSurface();
  const uint32_t player = Create(AudioCreationParam(), surface);
  ASSERT_NE(player, 0u);
  EXPECT_CALL(sbplayer_, SetBounds(sb_player_, 1, 2, 3, 4, 5));
  host_->SetBounds(player, 1, 2, 3, 4, 5);

  EXPECT_CALL(sbplayer_, SetBounds(sb_player_, 1, 10, 20, 30, 40));
  MoveHole(surface, gfx::RectF(10, 20, 30, 40));
  // The guest's z-index still applies.
  EXPECT_CALL(sbplayer_, SetBounds(sb_player_, 6, 10, 20, 30, 40));
  host_->SetBounds(player, 6, 7, 8, 9, 10);

  EXPECT_CALL(sbplayer_, SetBounds(sb_player_, 6, 7, 8, 9, 10));
  host_->DestroySurface(surface);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(geometry_.clients.begin()->second.is_connected());
}

TEST_F(WasmPlayerHostTest, PublishesSurfaceVideoSize) {
  const uint32_t surface = host_->CreateSurface();
  const base::UnguessableToken token =
      *base::UnguessableToken::DeserializeFromString(
          host_->SurfaceToken(surface));
  std::vector<gfx::Size> sizes;
  base::ScopedClosureRunner watch = SurfaceVideoSize::Watch(
      token, base::BindLambdaForTesting(
                 [&](const gfx::Size& size) { sizes.push_back(size); }));

  WasmSbPlayerCreationParam param = AudioCreationParam();
  param.video_stream_info.codec = kSbMediaVideoCodecVp9;
  param.video_stream_info.frame_width = 1080;
  param.video_stream_info.frame_height = 1920;
  param.video_stream_info.color_metadata.bits_per_channel = 8;
  const uint32_t player = Create(param, surface);
  ASSERT_NE(player, 0u);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sizes, std::vector<gfx::Size>({{1080, 1920}}));

  WasmSbPlayerSampleInfo info = AudioSample(0);
  info.type = kSbMediaTypeVideo;
  info.video_sample_info = {};
  info.video_sample_info.stream_info = param.video_stream_info;
  info.video_sample_info.stream_info.frame_width = 1920;
  info.video_sample_info.stream_info.frame_height = 1080;
  Write(kInfos, info);
  NeedData(kSbMediaTypeVideo);
  ASSERT_TRUE(WriteSamples(player, kSbMediaTypeVideo, 1));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sizes, std::vector<gfx::Size>({{1080, 1920}, {1920, 1080}}));

  // An audio-only player on the surface leaves the size alone.
  ASSERT_NE(Create(AudioCreationParam(), surface), 0u);
  host_->DestroySurface(surface);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sizes.size(), 2u);
}

TEST_F(WasmPlayerHostTest, NewPlayerStartsAtSurfacesHole) {
  const uint32_t surface = host_->CreateSurface();
  ASSERT_NE(Create(AudioCreationParam(), surface), 0u);
  MoveHole(surface, gfx::RectF(10, 20, 30, 40));

  EXPECT_CALL(sbplayer_, SetBounds(_, 0, 10, 20, 30, 40));
  ASSERT_NE(Create(AudioCreationParam(), surface), 0u);
}

TEST_F(WasmPlayerHostTest, PlayerWithoutSurfaceIgnoresHoles) {
  const uint32_t surface = host_->CreateSurface();
  ASSERT_NE(CreateAudioPlayer(), 0u);
  EXPECT_CALL(sbplayer_, SetBounds).Times(0);
  MoveHole(surface, gfx::RectF(10, 20, 30, 40));
}

TEST_F(WasmPlayerHostTest, DestroyingHostUnsubscribes) {
  const uint32_t surface = host_->CreateSurface();
  const base::UnguessableToken token =
      *base::UnguessableToken::DeserializeFromString(
          host_->SurfaceToken(surface));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(geometry_.clients.size(), 1u);
  SurfaceVideoSize::Set(token, gfx::Size(1080, 1920));
  host_.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(geometry_.clients.begin()->second.is_connected());
  // The host forgot the surface's size: a new watcher gets nothing.
  bool notified = false;
  base::ScopedClosureRunner watch = SurfaceVideoSize::Watch(
      token,
      base::BindLambdaForTesting([&](const gfx::Size&) { notified = true; }));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(notified);
}

TEST_F(WasmPlayerHostTest, WritesPlayerInfo) {
  const uint32_t player = CreateAudioPlayer();
  EXPECT_CALL(sbplayer_, GetInfo(sb_player_, _))
      .WillOnce(Invoke([](SbPlayer, SbPlayerInfo* info) {
        info->current_media_timestamp = 123;
        info->is_paused = true;
        info->playback_rate = 2.0;
      }));
  GuestMemory memory = Memory();
  ASSERT_TRUE(host_->GetInfo(memory, player, kOut));
  const WasmSbPlayerInfo info = ReadBack<WasmSbPlayerInfo>(kOut);
  EXPECT_EQ(info.current_media_timestamp, 123);
  EXPECT_EQ(info.is_paused, 1);
  EXPECT_EQ(info.playback_rate, 2.0);
  EXPECT_FALSE(host_->GetInfo(memory, player, kMemorySize - 8));
}

TEST_F(WasmPlayerHostTest, QueriesSbMedia) {
  MockSbMediaInterface& sbmedia = sbmedia_;
  memcpy(memory_.data() + kMime, kAudioMime, strlen(kAudioMime));
  GuestMemory memory = Memory();

  EXPECT_CALL(sbmedia, CanPlayMimeAndKeySystem(StrEq(kAudioMime), StrEq("")))
      .WillOnce(Return(kSbMediaSupportTypeProbably));
  EXPECT_EQ(
      host_->CanPlayMimeAndKeySystem(memory, kMime, strlen(kAudioMime), 0, 0),
      kSbMediaSupportTypeProbably);
  EXPECT_CALL(sbmedia, CanChangeType(StrEq(kAudioMime), StrEq("audio")))
      .WillOnce(Return(true));
  EXPECT_TRUE(
      host_->CanChangeType(memory, kMime, strlen(kAudioMime), kMime, 5));

  EXPECT_CALL(sbmedia, CanPlayMimeAndKeySystem).Times(0);
  EXPECT_CALL(sbmedia, CanChangeType).Times(0);
  EXPECT_EQ(host_->CanPlayMimeAndKeySystem(memory, kMemorySize, 1, 0, 0),
            kSbMediaSupportTypeNotSupported);
  EXPECT_FALSE(host_->CanChangeType(memory, kMime, 1, kMemorySize, 1));
}

TEST_F(WasmPlayerHostTest, WritesAudioConfiguration) {
  const uint32_t player = CreateAudioPlayer();
  EXPECT_CALL(sbplayer_, GetAudioConfiguration(sb_player_, 0, _))
      .WillOnce(Invoke([](SbPlayer, int, SbMediaAudioConfiguration* config) {
        config->connector = kSbMediaAudioConnectorBluetooth;
        config->latency = 150'000;
        config->coding_type = kSbMediaAudioCodingTypePcm;
        config->number_of_channels = 2;
        return true;
      }));
  EXPECT_CALL(sbplayer_, GetAudioConfiguration(sb_player_, 1, _))
      .WillOnce(Return(false));
  GuestMemory memory = Memory();
  ASSERT_TRUE(host_->GetAudioConfiguration(memory, player, 0, kOut));
  const auto config = ReadBack<WasmSbMediaAudioConfiguration>(kOut);
  EXPECT_EQ(config.connector, kSbMediaAudioConnectorBluetooth);
  EXPECT_EQ(config.latency, 150'000);
  EXPECT_EQ(config.coding_type, kSbMediaAudioCodingTypePcm);
  EXPECT_EQ(config.number_of_channels, 2);

  EXPECT_FALSE(host_->GetAudioConfiguration(memory, player, 1, kOut));
  EXPECT_FALSE(host_->GetAudioConfiguration(memory, player, -1, kOut));
  EXPECT_FALSE(
      host_->GetAudioConfiguration(memory, player, 0, kMemorySize - 8));
  EXPECT_FALSE(host_->GetAudioConfiguration(memory, player + 1, 0, kOut));
}

TEST_F(WasmPlayerHostTest, QueueOutlivesContextDuringDestroy) {
  ASSERT_NE(CreateAudioPlayer(), 0u);
  SbPlayer sb_player = sb_player_;
  void* context = context_;
  EXPECT_CALL(sbplayer_, Destroy(sb_player)).WillOnce(Invoke([&](SbPlayer p) {
    player_status_(p, context, kSbPlayerStateDestroyed, 0);
    delete reinterpret_cast<::media::MockSbPlayer*>(p);
  }));
  // What H5vccWasmImports::ContextDestroyed() does, with the destroy still
  // in flight.
  host_.reset();
  events_->Shutdown();
  events_.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace starboard_wasm
}  // namespace cobalt
