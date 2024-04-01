// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/check.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decoder_selector.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_video_decoder.h"
#endif  // !defined(OS_ANDROID)

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

namespace {

enum DecryptorCapability {
  kNoDecryptor,
  kDecryptOnly,
  kDecryptAndDecode,
};

enum DecoderCapability {
  kAlwaysFail,
  kClearOnly,
  kEncryptedOnly,
  kAlwaysSucceed,
};

bool DecoderCapabilitySupportsDecryption(DecoderCapability capability) {
  switch (capability) {
    case kAlwaysFail:
      return false;
    case kClearOnly:
      return false;
    case kEncryptedOnly:
      return true;
    case kAlwaysSucceed:
      return true;
  }
}

Status IsConfigSupported(DecoderCapability capability, bool is_encrypted) {
  switch (capability) {
    case kAlwaysFail:
      return StatusCode::kCodeOnlyForTesting;
    case kClearOnly:
      return is_encrypted ? StatusCode::kCodeOnlyForTesting : OkStatus();
    case kEncryptedOnly:
      return is_encrypted ? OkStatus() : StatusCode::kCodeOnlyForTesting;
    case kAlwaysSucceed:
      return OkStatus();
  }
}

const int kDecoder1 = 0xabc;
const int kDecoder2 = 0xdef;
const int kDecoder3 = 0x123;
const int kDecoder4 = 0x456;

// Specializations for the AUDIO version of the test.
class AudioDecoderSelectorTestParam {
 public:
  static constexpr DemuxerStream::Type kStreamType = DemuxerStream::AUDIO;

  using StreamTraits = DecoderStreamTraits<DemuxerStream::AUDIO>;
  using MockDecoder = MockAudioDecoder;
  using Output = AudioBuffer;
  using DecoderType = AudioDecoderType;

#if !defined(OS_ANDROID)
  using DecryptingDecoder = DecryptingAudioDecoder;
#endif  // !defined(OS_ANDROID)

  // StreamTraits() takes different parameters depending on the type.
  static std::unique_ptr<StreamTraits> CreateStreamTraits(MediaLog* media_log) {
    return std::make_unique<StreamTraits>(media_log, CHANNEL_LAYOUT_STEREO);
  }

  static const base::Feature& ForceHardwareDecodersFeature() {
    return kForceHardwareAudioDecoders;
  }

  static media::DecoderPriority MockDecoderPriorityCB(
      const media::AudioDecoderConfig& config,
      const media::AudioDecoder& decoder) {
    const auto above_cutoff =
        config.samples_per_second() > TestAudioConfig::NormalSampleRateValue();
    return above_cutoff == decoder.IsPlatformDecoder()
               ? media::DecoderPriority::kNormal
               : media::DecoderPriority::kDeprioritized;
  }
  static media::DecoderPriority NormalDecoderPriorityCB(
      const media::AudioDecoderConfig& /*config*/,
      const media::AudioDecoder& /*decoder*/) {
    return media::DecoderPriority::kNormal;
  }
  static media::DecoderPriority SkipDecoderPriorityCB(
      const media::AudioDecoderConfig& /*config*/,
      const media::AudioDecoder& /*decoder*/) {
    return media::DecoderPriority::kSkipped;
  }

  static void UseNormalClearDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_audio_decoder_config(TestAudioConfig::Normal());
  }
  static void UseHighQualityClearDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_audio_decoder_config(TestAudioConfig::HighSampleRate());
  }
  static void UseNormalEncryptedDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_audio_decoder_config(TestAudioConfig::NormalEncrypted());
  }
  static void UseHighQualityEncryptedDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_audio_decoder_config(TestAudioConfig::HighSampleRateEncrypted());
  }

  // Decoder::Initialize() takes different parameters depending on the type.
  static void ExpectInitialize(MockDecoder* decoder,
                               DecoderCapability capability) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _))
        .WillRepeatedly([capability](const AudioDecoderConfig& config,
                                     CdmContext*, AudioDecoder::InitCB& init_cb,
                                     const AudioDecoder::OutputCB&,
                                     const WaitingCB&) {
          std::move(init_cb).Run(
              IsConfigSupported(capability, config.is_encrypted()));
        });
  }

  static void ExpectNotInitialize(MockDecoder* decoder) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _)).Times(0);
  }

  static void SetRTCDecoderness(MockDecoder* decoder, bool is_rtc_decoder) {}
};

// Allocate storage for the member variables.
constexpr DemuxerStream::Type AudioDecoderSelectorTestParam::kStreamType;

// Specializations for the VIDEO version of the test.
class VideoDecoderSelectorTestParam {
 public:
  static constexpr DemuxerStream::Type kStreamType = DemuxerStream::VIDEO;

  using StreamTraits = DecoderStreamTraits<DemuxerStream::VIDEO>;
  using MockDecoder = MockVideoDecoder;
  using Output = VideoFrame;
  using DecoderType = VideoDecoderType;

#if !defined(OS_ANDROID)
  using DecryptingDecoder = DecryptingVideoDecoder;
#endif  // !defined(OS_ANDROID)

  static const base::Feature& ForceHardwareDecodersFeature() {
    return kForceHardwareVideoDecoders;
  }

  static std::unique_ptr<StreamTraits> CreateStreamTraits(MediaLog* media_log) {
    return std::make_unique<StreamTraits>(media_log);
  }

  static media::DecoderPriority MockDecoderPriorityCB(
      const media::VideoDecoderConfig& config,
      const media::VideoDecoder& decoder) {
    auto const above_cutoff = config.visible_rect().height() >
                              TestVideoConfig::NormalCodedSize().height();
    return decoder.IsPlatformDecoder() == above_cutoff
               ? media::DecoderPriority::kNormal
               : media::DecoderPriority::kDeprioritized;
  }
  static media::DecoderPriority NormalDecoderPriorityCB(
      const media::VideoDecoderConfig& /*config*/,
      const media::VideoDecoder& /*decoder*/) {
    return media::DecoderPriority::kNormal;
  }
  static media::DecoderPriority SkipDecoderPriorityCB(
      const media::VideoDecoderConfig& /*config*/,
      const media::VideoDecoder& /*decoder*/) {
    return media::DecoderPriority::kSkipped;
  }

  static void UseNormalClearDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_video_decoder_config(TestVideoConfig::Normal());
  }
  static void UseHighQualityClearDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_video_decoder_config(TestVideoConfig::Large());
  }
  static void UseNormalEncryptedDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_video_decoder_config(TestVideoConfig::NormalEncrypted());
  }
  static void UseHighQualityEncryptedDecoderConfig(
      StrictMock<MockDemuxerStream>& stream) {
    stream.set_video_decoder_config(TestVideoConfig::LargeEncrypted());
  }

  static void ExpectInitialize(MockDecoder* decoder,
                               DecoderCapability capability) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _, _))
        .WillRepeatedly(
            [capability](const VideoDecoderConfig& config, bool low_delay,
                         CdmContext*, VideoDecoder::InitCB& init_cb,
                         const VideoDecoder::OutputCB&, const WaitingCB&) {
              std::move(init_cb).Run(
                  IsConfigSupported(capability, config.is_encrypted()));
            });
  }

  static void ExpectNotInitialize(MockDecoder* decoder) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _, _)).Times(0);
  }

  static void SetRTCDecoderness(MockDecoder* decoder, bool is_optimized) {
    EXPECT_CALL(*decoder, IsOptimizedForRTC())
        .WillRepeatedly(Return(is_optimized));
  }
};

// Allocate storate for the member variables.
constexpr DemuxerStream::Type VideoDecoderSelectorTestParam::kStreamType;

}  // namespace

// Note: The parameter is called TypeParam in the test cases regardless of what
// we call it here. It's been named the same for convenience.
// Note: The test fixtures inherit from this class. Inside the test cases the
// test fixture class is called TestFixture.
template <typename TypeParam>
class DecoderSelectorTest : public ::testing::Test {
 public:
  // Convenience aliases.
  using Self = DecoderSelectorTest<TypeParam>;
  using StreamTraits = typename TypeParam::StreamTraits;
  using Decoder = typename StreamTraits::DecoderType;
  using MockDecoder = typename TypeParam::MockDecoder;
  using Output = typename TypeParam::Output;
  using DecoderType = typename TypeParam::DecoderType;
  using Selector = DecoderSelector<TypeParam::kStreamType>;

  struct MockDecoderArgs {
    static MockDecoderArgs Create(int decoder_id,
                                  DecoderCapability capability) {
      MockDecoderArgs result;
      result.decoder_id = decoder_id;
      result.capability = capability;
      result.supports_decryption =
          DecoderCapabilitySupportsDecryption(capability);
      result.is_platform_decoder = false;
      result.expect_not_initialized = false;
      return result;
    }

    int decoder_id;
    DecoderCapability capability;
    bool supports_decryption;
    bool is_platform_decoder;
    bool expect_not_initialized;
    bool is_rtc_decoder = false;
  };

  DecoderSelectorTest()
      : traits_(TypeParam::CreateStreamTraits(&media_log_)),
        demuxer_stream_(TypeParam::kStreamType) {}

  void OnWaiting(WaitingReason reason) { NOTREACHED(); }
  void OnOutput(scoped_refptr<Output> output) { NOTREACHED(); }

  MOCK_METHOD0_T(NoDecoderSelected, void());
  MOCK_METHOD1_T(OnDecoderSelected, void(int));
  MOCK_METHOD1_T(OnDecoderSelected, void(DecoderType));
  MOCK_METHOD1_T(OnDemuxerStreamSelected,
                 void(std::unique_ptr<DecryptingDemuxerStream>));

  void OnDecoderSelectedThunk(
      std::unique_ptr<Decoder> decoder,
      std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
    // Report only the type or id of the decoder, since that's what the tests
    // care about. The decoder will be destructed immediately.
    if (decoder && decoder->GetDecoderType() == DecoderType::kTesting) {
      OnDecoderSelected(
          static_cast<MockDecoder*>(decoder.get())->GetDecoderId());
    } else if (decoder) {
      OnDecoderSelected(decoder->GetDecoderType());
    } else {
      NoDecoderSelected();
    }

    if (decrypting_demuxer_stream)
      OnDemuxerStreamSelected(std::move(decrypting_demuxer_stream));
  }

  void AddDecryptingDecoder() {
    // Require the DecryptingDecoder to be first, because that's easier to
    // implement.
    DCHECK(mock_decoders_to_create_.empty());
    DCHECK(!use_decrypting_decoder_);
    use_decrypting_decoder_ = true;
  }

  void AddMockDecoder(int decoder_id, DecoderCapability capability) {
    auto args = MockDecoderArgs::Create(decoder_id, capability);
    AddMockDecoder(std::move(args));
  }

  void AddMockPlatformDecoder(int decoder_id, DecoderCapability capability) {
    auto args = MockDecoderArgs::Create(std::move(decoder_id), capability);
    args.is_platform_decoder = true;
    AddMockDecoder(std::move(args));
  }

  void AddMockRTCPlatformDecoder(int decoder_type,
                                 DecoderCapability capability) {
    auto args = MockDecoderArgs::Create(std::move(decoder_type), capability);
    args.is_rtc_decoder = true;
    args.is_platform_decoder = true;
    AddMockDecoder(std::move(args));
  }

  void AddMockDecoder(MockDecoderArgs args) {
    // Actual decoders are created in CreateDecoders(), which may be called
    // multiple times by the DecoderSelector.
    mock_decoders_to_create_.push_back(std::move(args));
  }

  std::vector<std::unique_ptr<Decoder>> CreateDecoders() {
    std::vector<std::unique_ptr<Decoder>> decoders;

#if !defined(OS_ANDROID)
    if (use_decrypting_decoder_) {
      decoders.push_back(
          std::make_unique<typename TypeParam::DecryptingDecoder>(
              task_environment_.GetMainThreadTaskRunner(), &media_log_));
    }
#endif  // !defined(OS_ANDROID)

    for (const auto& args : mock_decoders_to_create_) {
      std::unique_ptr<StrictMock<MockDecoder>> decoder =
          std::make_unique<StrictMock<MockDecoder>>(args.is_platform_decoder,
                                                    args.supports_decryption,
                                                    args.decoder_id);
      if (args.expect_not_initialized) {
        TypeParam::ExpectNotInitialize(decoder.get());
      } else {
        TypeParam::ExpectInitialize(decoder.get(), args.capability);
      }
      TypeParam::SetRTCDecoderness(decoder.get(), args.is_rtc_decoder);
      decoders.push_back(std::move(decoder));
    }

    return decoders;
  }

  void CreateCdmContext(DecryptorCapability capability) {
    DCHECK(!decoder_selector_);

    cdm_context_ = std::make_unique<StrictMock<MockCdmContext>>();

    EXPECT_CALL(*cdm_context_, RegisterEventCB(_)).Times(AnyNumber());

    if (capability == kNoDecryptor) {
      EXPECT_CALL(*cdm_context_, GetDecryptor())
          .WillRepeatedly(Return(nullptr));
      return;
    }

    decryptor_ = std::make_unique<NiceMock<MockDecryptor>>();
    EXPECT_CALL(*cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(decryptor_.get()));
    switch (TypeParam::kStreamType) {
      case DemuxerStream::AUDIO:
        EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
            .WillRepeatedly(
                RunOnceCallback<1>(capability == kDecryptAndDecode));
        break;
      case DemuxerStream::VIDEO:
        EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
            .WillRepeatedly(
                RunOnceCallback<1>(capability == kDecryptAndDecode));
        break;
      default:
        NOTREACHED();
    }
  }

  void CreateDecoderSelector() {
    decoder_selector_ = std::make_unique<Selector>(
        task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(&Self::CreateDecoders, base::Unretained(this)),
        &media_log_);
    decoder_selector_->Initialize(
        traits_.get(), &demuxer_stream_, cdm_context_.get(),
        base::BindRepeating(&Self::OnWaiting, base::Unretained(this)));
  }

  void UseClearDecoderConfig() {
    TypeParam::UseNormalClearDecoderConfig(demuxer_stream_);
  }
  void UseHighQualityClearDecoderConfig() {
    TypeParam::UseHighQualityClearDecoderConfig(demuxer_stream_);
  }
  void UseEncryptedDecoderConfig() {
    TypeParam::UseNormalEncryptedDecoderConfig(demuxer_stream_);
  }
  void UseHighQualityEncryptedDecoderConfig() {
    TypeParam::UseHighQualityEncryptedDecoderConfig(demuxer_stream_);
  }

  void SelectDecoder() {
    decoder_selector_->SelectDecoder(
        base::BindOnce(&Self::OnDecoderSelectedThunk, base::Unretained(this)),
        base::BindRepeating(&Self::OnOutput, base::Unretained(this)));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  NullMediaLog media_log_;

  std::unique_ptr<StreamTraits> traits_;
  StrictMock<MockDemuxerStream> demuxer_stream_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;
  std::unique_ptr<NiceMock<MockDecryptor>> decryptor_;

  std::unique_ptr<Selector> decoder_selector_;

  bool use_decrypting_decoder_ = false;
  std::vector<MockDecoderArgs> mock_decoders_to_create_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecoderSelectorTest);
};

using VideoDecoderSelectorTest =
    DecoderSelectorTest<VideoDecoderSelectorTestParam>;

using DecoderSelectorTestParams =
    ::testing::Types<AudioDecoderSelectorTestParam,
                     VideoDecoderSelectorTestParam>;
TYPED_TEST_SUITE(DecoderSelectorTest, DecoderSelectorTestParams);

// Tests for clear streams. CDM will not be used for clear streams so
// DecryptorCapability doesn't really matter.

TYPED_TEST(DecoderSelectorTest, ClearStream_NoDecoders) {
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_NoClearDecoder) {
  this->AddDecryptingDecoder();
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kAlwaysFail);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_ExternalFallback) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();

  this->decoder_selector_->FinalizeDecoderSelection();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

// Tests that platform decoders are prioritized for
// high-quality configs, retaining their relative order.
TYPED_TEST(DecoderSelectorTest, ClearStream_PrioritizePlatformDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder2, kAlwaysSucceed);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseHighQualityClearDecoderConfig();
  this->CreateDecoderSelector();
  this->decoder_selector_->OverrideDecoderPriorityCBForTesting(
      base::BindRepeating(TypeParam::MockDecoderPriorityCB));

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests that non-platform decoders are prioritized for
// normal-quality configs, retaining their relative order.
TYPED_TEST(DecoderSelectorTest, ClearStream_DeprioritizePlatformDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder2, kAlwaysSucceed);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();
  this->decoder_selector_->OverrideDecoderPriorityCBForTesting(
      base::BindRepeating(TypeParam::MockDecoderPriorityCB));

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests that platform and non-platform decoders remain in the order they are
// given for a priority callback returning 'kNormal'.
TYPED_TEST(DecoderSelectorTest,
           ClearStream_NormalPriorityCallbackRetainsGivenOrder) {
  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder2, kAlwaysSucceed);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();
  this->decoder_selector_->OverrideDecoderPriorityCBForTesting(
      base::BindRepeating(TypeParam::NormalDecoderPriorityCB));

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_SkipAllDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder2, kAlwaysSucceed);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();
  this->decoder_selector_->OverrideDecoderPriorityCBForTesting(
      base::BindRepeating(TypeParam::SkipDecoderPriorityCB));

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearStream_ForceHardwareDecoders) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(TypeParam::ForceHardwareDecodersFeature());

  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests the production predicate for `DecoderSelector<DemuxerStream::VIDEO>`
TEST_F(VideoDecoderSelectorTest, ClearStream_PrioritizeSoftwareDecoders) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kResolutionBasedDecoderPriority);

  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create a clear config that will cause software decoders to be
  // prioritized on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::Custom(gfx::Size(64, 64)));
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests the production predicate for `DecoderSelector<DemuxerStream::VIDEO>`
TEST_F(VideoDecoderSelectorTest, ClearStream_PrioritizePlatformDecoders) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kResolutionBasedDecoderPriority);

  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create a clear config that will cause hardware decoders to be prioritized
  // on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::Custom(gfx::Size(4096, 4096)));
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();
  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests for encrypted streams.

// Tests that non-decrypting decoders are filtered out by DecoderSelector
// before being initialized.
TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_NoDecryptor_DecodersNotInitialized) {
  using MockDecoderArgs =
      typename DecoderSelectorTest<TypeParam>::MockDecoderArgs;

  auto args = MockDecoderArgs::Create(kDecoder1, kClearOnly);
  args.expect_not_initialized = true;
  this->AddMockDecoder(std::move(args));

  args = MockDecoderArgs::Create(kDecoder2, kClearOnly);
  args.expect_not_initialized = true;
  this->AddMockDecoder(std::move(args));

  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests that for an encrypted stream, platform decoders are prioritized for
// high-quality configs, retaining their relative order.
TYPED_TEST(DecoderSelectorTest, EncryptedStream_PrioritizePlatformDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder2, kAlwaysSucceed);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseHighQualityEncryptedDecoderConfig();
  this->CreateDecoderSelector();
  this->decoder_selector_->OverrideDecoderPriorityCBForTesting(
      base::BindRepeating(TypeParam::MockDecoderPriorityCB));

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests that for an encrypted stream, non-platform decoders are prioritized for
// normal-quality configs, retaining their relative order.
TYPED_TEST(DecoderSelectorTest, EncryptedStream_DeprioritizePlatformDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder2, kAlwaysSucceed);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();
  this->decoder_selector_->OverrideDecoderPriorityCBForTesting(
      base::BindRepeating(TypeParam::MockDecoderPriorityCB));

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests that platform and non-platform decoders remain in the order they are
// given for a priority callback returning 'kNormal'.
TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_NormalPriorityCallbackRetainsGivenOrder) {
  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder2, kAlwaysSucceed);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();
  this->decoder_selector_->OverrideDecoderPriorityCBForTesting(
      base::BindRepeating(TypeParam::NormalDecoderPriorityCB));

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_SkipAllDecoders) {
  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder2, kAlwaysSucceed);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();
  this->decoder_selector_->OverrideDecoderPriorityCBForTesting(
      base::BindRepeating(TypeParam::SkipDecoderPriorityCB));

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_ForceHardwareDecoders) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(TypeParam::ForceHardwareDecodersFeature());

  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_NoDecryptor_ExternalFallback) {
  this->AddMockDecoder(kDecoder1, kEncryptedOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_NoDecryptor_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kEncryptedOnly);
  this->AddMockDecoder(kDecoder2, kEncryptedOnly);
  this->CreateCdmContext(kNoDecryptor);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();

  this->decoder_selector_->FinalizeDecoderSelection();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_NoDecoder) {
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_OneClearDecoder) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptOnly_InternalFallback) {
  this->AddMockDecoder(kDecoder1, kAlwaysFail);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()));

  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_DecryptOnly_FinalizeDecoderSelection) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

  std::unique_ptr<DecryptingDemuxerStream> saved_dds;
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()))
      .WillOnce([&](std::unique_ptr<DecryptingDemuxerStream> dds) {
        saved_dds = std::move(dds);
      });

  this->SelectDecoder();

  this->decoder_selector_->FinalizeDecoderSelection();

  // DDS is reused.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, EncryptedStream_DecryptAndDecode) {
  this->AddDecryptingDecoder();
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptAndDecode);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

#if !defined(OS_ANDROID)
  // A DecryptingVideoDecoder will be created and selected. The clear decoder
  // should not be touched at all. No DecryptingDemuxerStream should be
  // created.
  EXPECT_CALL(*this, OnDecoderSelected(TestFixture::DecoderType::kDecrypting));
#else
  // A DecryptingDemuxerStream will be created. The clear decoder will be
  // initialized and returned.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()));
#endif  // !defined(OS_ANDROID)

  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest,
           EncryptedStream_DecryptAndDecode_ExternalFallback) {
  this->AddDecryptingDecoder();
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->CreateCdmContext(kDecryptAndDecode);
  this->UseEncryptedDecoderConfig();
  this->CreateDecoderSelector();

#if !defined(OS_ANDROID)
  // DecryptingDecoder is selected immediately.
  EXPECT_CALL(*this, OnDecoderSelected(TestFixture::DecoderType::kDecrypting));
  this->SelectDecoder();
#endif  // !defined(OS_ANDROID)

  // On fallback, a DecryptingDemuxerStream will be created.
  std::unique_ptr<DecryptingDemuxerStream> saved_dds;
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()))
      .WillOnce([&](std::unique_ptr<DecryptingDemuxerStream> dds) {
        saved_dds = std::move(dds);
      });
  this->SelectDecoder();

  // The DecryptingDemuxerStream should be reused.
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

TYPED_TEST(DecoderSelectorTest, ClearToEncryptedStream_DecryptOnly) {
  this->AddMockDecoder(kDecoder1, kClearOnly);
  this->CreateCdmContext(kDecryptOnly);
  this->UseClearDecoderConfig();
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  this->SelectDecoder();

  this->decoder_selector_->FinalizeDecoderSelection();
  this->UseEncryptedDecoderConfig();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder1));
  EXPECT_CALL(*this, OnDemuxerStreamSelected(NotNull()));
  this->SelectDecoder();
}

// Tests the production predicate for `DecoderSelector<DemuxerStream::VIDEO>`
TEST_F(VideoDecoderSelectorTest, EncryptedStream_PrioritizeSoftwareDecoders) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kResolutionBasedDecoderPriority);

  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create an encrypted config that will cause software decoders to be
  // prioritized on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::CustomEncrypted(gfx::Size(64, 64)));
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests the production predicate for `DecoderSelector<DemuxerStream::VIDEO>`
TEST_F(VideoDecoderSelectorTest, EncryptedStream_PrioritizePlatformDecoders) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kResolutionBasedDecoderPriority);

  this->AddMockPlatformDecoder(kDecoder1, kClearOnly);
  this->AddMockDecoder(kDecoder2, kClearOnly);
  this->AddMockPlatformDecoder(kDecoder3, kAlwaysSucceed);
  this->AddMockDecoder(kDecoder4, kAlwaysSucceed);

  // Create an encrypted config that will cause hardware decoders to be
  // prioritized on any platform.
  this->demuxer_stream_.set_video_decoder_config(
      TestVideoConfig::CustomEncrypted(gfx::Size(4096, 4096)));
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder3));
  this->SelectDecoder();
  EXPECT_CALL(*this, OnDecoderSelected(kDecoder4));
  this->SelectDecoder();
  EXPECT_CALL(*this, NoDecoderSelected());
  this->SelectDecoder();
}

// Tests that the normal decoder selector rule skips non-RTC decoders for RTC.
TEST_F(VideoDecoderSelectorTest, RTC_NormalPriority) {
  base::test::ScopedFeatureList features;

  this->AddMockDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockRTCPlatformDecoder(kDecoder2, kAlwaysSucceed);

  auto config = TestVideoConfig::Custom(gfx::Size(4096, 4096));
  config.set_is_rtc(true);
  this->demuxer_stream_.set_video_decoder_config(config);
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

// Tests that the resolution-based rule skips non-RTC decoders for RTC.
TEST_F(VideoDecoderSelectorTest, RTC_DecoderBasedPriority) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kResolutionBasedDecoderPriority);

  this->AddMockDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockRTCPlatformDecoder(kDecoder2, kAlwaysSucceed);

  auto config = TestVideoConfig::Custom(gfx::Size(4096, 4096));
  config.set_is_rtc(true);
  this->demuxer_stream_.set_video_decoder_config(config);
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

// Tests that the hardware-based rule skips non-RTC decoders for RTC.
TEST_F(VideoDecoderSelectorTest, RTC_ForceHardwareDecoders) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kForceHardwareVideoDecoders);

  this->AddMockPlatformDecoder(kDecoder1, kAlwaysSucceed);
  this->AddMockRTCPlatformDecoder(kDecoder2, kAlwaysSucceed);

  auto config = TestVideoConfig::Custom(gfx::Size(4096, 4096));
  config.set_is_rtc(true);
  this->demuxer_stream_.set_video_decoder_config(config);
  this->CreateDecoderSelector();

  EXPECT_CALL(*this, OnDecoderSelected(kDecoder2));
  this->SelectDecoder();
}

}  // namespace media
