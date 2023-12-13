// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_MOCK_FILTERS_H_
#define COBALT_MEDIA_BASE_MOCK_FILTERS_H_

#include "cobalt/media/base/sbplayer_pipeline.h"
#include "string"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/chromium/media/base/pipeline.h"
#include "vector"

namespace cobalt {
namespace media {

using ::media::AudioDecoderConfig;
using ::media::AudioPipelineInfo;
using ::media::Demuxer;
using ::media::DemuxerHost;
using ::media::DemuxerStream;
using ::media::MediaTrack;
using ::media::PipelineMetadata;
using ::media::PipelineStatusCallback;
using ::media::VideoDecoderConfig;
using ::media::VideoPipelineInfo;

class MockPipelineClient : public ::media::Pipeline::Client {
 public:
  MockPipelineClient();
  ~MockPipelineClient();

  MOCK_METHOD1(OnError, void(PipelineStatus));
  MOCK_METHOD1(OnFallback, void(PipelineStatus));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD1(OnMetadata, void(const PipelineMetadata&));
  MOCK_METHOD2(OnBufferingStateChange,
               void(::media::BufferingState,
                    ::media::BufferingStateChangeReason));
  MOCK_METHOD0(OnDurationChange, void());
  MOCK_METHOD1(OnWaiting, void(::media::WaitingReason));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size&));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool));
  MOCK_METHOD1(OnVideoFrameRateChange, void(absl::optional<int>));
  MOCK_METHOD0(OnVideoAverageKeyframeDistanceUpdate, void());
  MOCK_METHOD1(OnAudioPipelineInfoChange, void(const AudioPipelineInfo&));
  MOCK_METHOD1(OnVideoPipelineInfoChange, void(const VideoPipelineInfo&));
  MOCK_METHOD1(OnRemotePlayStateChange,
               void(::media::MediaStatus::State state));
  MOCK_METHOD(void, OnAddTextTrack,
              (const ::media::TextTrackConfig& config,
               ::media::AddTextTrackDoneCB done_cb),
              (override));
};

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer();

  MockDemuxer(const MockDemuxer&) = delete;
  MockDemuxer& operator=(const MockDemuxer&) = delete;

  ~MockDemuxer() override;

  // Demuxer implementation.
  std::string GetDisplayName() const override;

  // DemuxerType GetDemuxerType() const override;

  void Initialize(DemuxerHost* host, PipelineStatusCallback cb) override {
    OnInitialize(host, cb);
  }
  MOCK_METHOD(void, OnInitialize,
              (DemuxerHost * host, PipelineStatusCallback& cb), ());
  MOCK_METHOD(void, StartWaitingForSeek, (base::TimeDelta), (override));
  MOCK_METHOD(void, CancelPendingSeek, (base::TimeDelta), (override));
  void Seek(base::TimeDelta time, PipelineStatusCallback cb) override {
    OnSeek(time, cb);
  }
  MOCK_METHOD(bool, IsSeekable, (), (const override));
  MOCK_METHOD(void, OnSeek, (base::TimeDelta time, PipelineStatusCallback& cb),
              ());
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, AbortPendingReads, (), (override));
  MOCK_METHOD(std::vector<DemuxerStream*>, GetAllStreams, (), (override));

  MOCK_METHOD(base::TimeDelta, GetStartTime, (), (const, override));
  MOCK_METHOD(base::Time, GetTimelineOffset, (), (const, override));
  MOCK_METHOD(int64_t, GetMemoryUsage, (), (const, override));
  MOCK_METHOD(absl::optional<::media::container_names::MediaContainerName>,
              GetContainerForMetrics, (), (const, override));
  MOCK_METHOD(void, OnEnabledAudioTracksChanged,
              (const std::vector<MediaTrack::Id>&, base::TimeDelta,
               TrackChangeCB),
              (override));
  MOCK_METHOD(void, OnSelectedVideoTrackChanged,
              (const std::vector<MediaTrack::Id>&, base::TimeDelta,
               TrackChangeCB),
              (override));

  // MOCK_METHOD(void, SetPlaybackRate, (double), (override));
};


class MockDemuxerStream : public DemuxerStream {
 public:
  explicit MockDemuxerStream(DemuxerStream::Type type);

  MockDemuxerStream(const MockDemuxerStream&) = delete;
  MockDemuxerStream& operator=(const MockDemuxerStream&) = delete;

  ~MockDemuxerStream() override;

  // DemuxerStream implementation.
  Type type() const override;
  // StreamLiveness liveness() const override;
#if defined(STARBOARD)
  // typedef base::OnceCallback<void(Status, const
  // std::vector<scoped_refptr<DecoderBuffer>>&)> ReadCB;
  void Read(int max_number_of_buffers_to_read, ReadCB read_cb) override {
    OnRead(read_cb);
  }
#else   // defined (STARBOARD)
  // typedef base::OnceCallback<void(Status, scoped_refptr<DecoderBuffer>)>
  // ReadCB;
  void Read(uint32_t count, ReadCB read_cb) override { OnRead(read_cb); }
#endif  // defined (STARBOARD)

  MOCK_METHOD1(OnRead, void(ReadCB& read_cb));
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  // MOCK_METHOD0(EnableBitstreamConverter, void());
  MOCK_METHOD0(SupportsConfigChanges, bool());

  // void set_audio_decoder_config(const AudioDecoderConfig& config);
  // void set_video_decoder_config(const VideoDecoderConfig& config);
  // void set_liveness(StreamLiveness liveness);

 private:
  Type type_ = DemuxerStream::Type::UNKNOWN;
  // StreamLiveness liveness_ = StreamLiveness::kUnknown;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;
};

class MockSbPlayerInterface : public SbPlayerInterface {
 public:
  MockSbPlayerInterface() = default;
  MockSbPlayerInterface(const MockSbPlayerInterface&) = delete;
  MockSbPlayerInterface& operator=(const MockSbPlayerInterface&) = delete;

  MOCK_METHOD(SbPlayer, Create,
              (SbWindow window, const SbPlayerCreationParam* creation_param,
               SbPlayerDeallocateSampleFunc sample_deallocate_func,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               SbPlayerErrorFunc player_error_func, void* context,
               SbDecodeTargetGraphicsContextProvider* context_provider),
              (override));

  MOCK_METHOD(SbPlayerOutputMode, GetPreferredOutputMode,
              (const SbPlayerCreationParam* creation_param), (override));

  MOCK_METHOD(void, Destroy, (SbPlayer player), (override));
  MOCK_METHOD(void, Seek,
              (SbPlayer player, SbTime seek_to_timestamp, int ticket),
              (override));

  MOCK_METHOD(bool, IsEnhancedAudioExtensionEnabled, (), (const override));
  MOCK_METHOD(void, WriteSamples,
              (SbPlayer player, SbMediaType sample_type,
               const SbPlayerSampleInfo* sample_infos,
               int number_of_sample_infos),
              (override));
  MOCK_METHOD(void, WriteSamples,
              (SbPlayer player, SbMediaType sample_type,
               const CobaltExtensionEnhancedAudioPlayerSampleInfo* sample_infos,
               int number_of_sample_infos),
              (override));
  MOCK_METHOD(int, GetMaximumNumberOfSamplesPerWrite,
              (SbPlayer player, SbMediaType sample_type), (override));
  MOCK_METHOD(void, WriteEndOfStream,
              (SbPlayer player, SbMediaType stream_type), (override));
  MOCK_METHOD(void, SetBounds,
              (SbPlayer player, int z_index, int x, int y, int width,
               int height),
              (override));
  MOCK_METHOD(bool, SetPlaybackRate, (SbPlayer player, double playback_rate),
              (override));
  MOCK_METHOD(void, SetVolume, (SbPlayer player, double volume), (override));

#if SB_API_VERSION >= 15
  MOCK_METHOD(void, GetInfo, (SbPlayer player, SbPlayerInfo* out_player_info),
              (override));
#else   // SB_API_VERSION >= 15
  MOCK_METHOD(void, GetInfo, (SbPlayer player, SbPlayerInfo2* out_player_info2),
              (override));
#endif  // SB_API_VERSION >= 15
  MOCK_METHOD(SbDecodeTarget, GetCurrentFrame, (SbPlayer player), (override));
//
// #if SB_HAS(PLAYER_WITH_URL)
//  SbPlayer CreateUrlPlayer(const char* url, SbWindow window,
//                           SbPlayerStatusFunc player_status_func,
//                           SbPlayerEncryptedMediaInitDataEncounteredCB
//                               encrypted_media_init_data_encountered_cb,
//                           SbPlayerErrorFunc player_error_func,
//                           void* context) override;
//  void SetUrlPlayerDrmSystem(SbPlayer player, SbDrmSystem drm_system)
//  override; bool GetUrlPlayerOutputModeSupported(SbPlayerOutputMode
//  output_mode) override; void GetUrlPlayerExtraInfo(
//      SbPlayer player, SbUrlPlayerExtraInfo* out_url_player_info) override;
// #endif  // SB_HAS(PLAYER_WITH_URL)
//
#if SB_API_VERSION >= 15
  MOCK_METHOD(bool, GetAudioConfiguration,
              (SbPlayer player, int index,
               SbMediaAudioConfiguration* out_audio_configuration),
              (override));
#endif  // SB_API_VERSION >= 15
  //
  // private:
  //  void (*enhanced_audio_player_write_samples_)(
  //      SbPlayer player, SbMediaType sample_type,
  //      const CobaltExtensionEnhancedAudioPlayerSampleInfo* sample_infos,
  //      int number_of_sample_infos) = nullptr;
};

class MockDecodeTargetProvider : public DecodeTargetProvider {
 public:
  MockDecodeTargetProvider() = default;
  MockDecodeTargetProvider(const MockDecodeTargetProvider&) = delete;
  MockDecodeTargetProvider& operator=(const MockDecodeTargetProvider&) = delete;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_MOCK_FILTERS_H_
