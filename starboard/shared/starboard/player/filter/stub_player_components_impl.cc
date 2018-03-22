// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

SbMediaAudioSampleType GetSupportedSampleType() {
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    return kSbMediaAudioSampleTypeFloat32;
  }
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(
      kSbMediaAudioSampleTypeInt16Deprecated));
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

}  // namespace

class StubAudioDecoder : public AudioDecoder, private JobQueue::JobOwner {
 public:
  explicit StubAudioDecoder(const SbMediaAudioHeader& audio_header)
      : sample_type_(GetSupportedSampleType()),
        audio_header_(audio_header),
        stream_ended_(false) {}

  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override {
    SB_UNREFERENCED_PARAMETER(error_cb);
    output_cb_ = output_cb;
  }

  void Decode(const scoped_refptr<InputBuffer>& input_buffer,
              const ConsumedCB& consumed_cb) override {
    SB_DCHECK(input_buffer);

    // Values to represent what kind of dummy audio to fill the decoded audio
    // we produce with.
    enum FillType {
      kSilence,
      kWave,
    };
    // Can be set locally to fill with different types.
    const FillType fill_type = kSilence;

    if (last_input_buffer_) {
      SbMediaTime diff = input_buffer->pts() - last_input_buffer_->pts();
      SB_DCHECK(diff >= 0);
      size_t sample_size =
          GetSampleType() == kSbMediaAudioSampleTypeInt16Deprecated ? 2 : 4;
      size_t size = diff * GetSamplesPerSecond() * sample_size *
                    audio_header_.number_of_channels / kSbMediaTimeSecond;
      size += size % (sample_size * audio_header_.number_of_channels);

      decoded_audios_.push(new DecodedAudio(audio_header_.number_of_channels,
                                            GetSampleType(), GetStorageType(),
                                            input_buffer->pts(), size));

      if (fill_type == kSilence) {
        SbMemorySet(decoded_audios_.back()->buffer(), 0, size);
      } else {
        SB_DCHECK(fill_type == kWave);
        for (int i = 0; i < size / sample_size; ++i) {
          if (sample_size == 2) {
            *(reinterpret_cast<int16_t*>(decoded_audios_.back()->buffer()) +
              i) = i;
          } else {
            SB_DCHECK(sample_size == 4);
            *(reinterpret_cast<float*>(decoded_audios_.back()->buffer()) + i) =
                ((i % 1024) - 512) / 512.0f;
          }
        }
      }
      Schedule(output_cb_);
    }
    Schedule(consumed_cb);
    last_input_buffer_ = input_buffer;
  }

  void WriteEndOfStream() override {
    if (last_input_buffer_) {
      // There won't be a next pts, so just guess that the decoded size is
      // 4 times the encoded size.
      size_t fake_size = 4 * last_input_buffer_->size();
      size_t sample_size =
          GetSampleType() == kSbMediaAudioSampleTypeInt16Deprecated ? 2 : 4;
      fake_size += fake_size % (sample_size * audio_header_.number_of_channels);

      decoded_audios_.push(new DecodedAudio(
          audio_header_.number_of_channels, GetSampleType(), GetStorageType(),
          last_input_buffer_->pts(), fake_size));
      Schedule(output_cb_);
    }
    decoded_audios_.push(new DecodedAudio());
    stream_ended_ = true;
    Schedule(output_cb_);
  }

  scoped_refptr<DecodedAudio> Read() override {
    scoped_refptr<DecodedAudio> result;
    if (!decoded_audios_.empty()) {
      result = decoded_audios_.front();
      decoded_audios_.pop();
    }
    return result;
  }

  void Reset() override {
    while (!decoded_audios_.empty()) {
      decoded_audios_.pop();
    }
    stream_ended_ = false;
    last_input_buffer_ = NULL;

    CancelPendingJobs();
  }
  SbMediaAudioSampleType GetSampleType() const override {
    return sample_type_;
  }
  SbMediaAudioFrameStorageType GetStorageType() const override {
    return kSbMediaAudioFrameStorageTypeInterleaved;
  }
  int GetSamplesPerSecond() const override {
    return audio_header_.samples_per_second;
  }

 private:
  OutputCB output_cb_;
  SbMediaAudioSampleType sample_type_;
  SbMediaAudioHeader audio_header_;
  bool stream_ended_;
  std::queue<scoped_refptr<DecodedAudio> > decoded_audios_;
  scoped_refptr<InputBuffer> last_input_buffer_;
};

class StubVideoDecoder : public VideoDecoder {
 public:
  StubVideoDecoder() {}
  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) override {
    SB_UNREFERENCED_PARAMETER(error_cb);
    SB_DCHECK(decoder_status_cb_);
    decoder_status_cb_ = decoder_status_cb;
  }
  size_t GetPrerollFrameCount() const override { return 1; }
  SbTime GetPrerollTimeout() const override { return kSbTimeMax; }
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer)
      override {
    SB_DCHECK(input_buffer);
    decoder_status_cb_(kNeedMoreInput, new VideoFrame(input_buffer->pts()));
  }
  void WriteEndOfStream() override {
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
  }
  void Reset() override {}
  SbDecodeTarget GetCurrentDecodeTarget() override {
    return kSbDecodeTargetInvalid;
  }

 private:
  DecoderStatusCB decoder_status_cb_;
};

// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  return output_mode == kSbPlayerOutputModePunchOut;
}

class PlayerComponentsImpl : public PlayerComponents {
  void CreateAudioComponents(
      const AudioParameters& audio_parameters,
      scoped_ptr<AudioDecoder>* audio_decoder,
      scoped_ptr<AudioRendererSink>* audio_renderer_sink) override {
    SB_DCHECK(audio_decoder);
    SB_DCHECK(audio_renderer_sink);

    audio_decoder->reset(new StubAudioDecoder(audio_parameters.audio_header));
    audio_renderer_sink->reset(new AudioRendererSinkImpl);
  }

  void CreateVideoComponents(
      const VideoParameters& video_parameters,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink) override {
    const SbTime kVideoSinkRenderInterval = 10 * kSbTimeMillisecond;

    SB_DCHECK(video_decoder);
    SB_DCHECK(video_render_algorithm);
    SB_DCHECK(video_renderer_sink);

    video_decoder->reset(new StubVideoDecoder);
    video_render_algorithm->reset(new VideoRenderAlgorithmImpl);
    *video_renderer_sink = new PunchoutVideoRendererSink(
        video_parameters.player, kVideoSinkRenderInterval);
  }

  void GetAudioRendererParams(int* max_cached_frames,
                              int* max_frames_per_append) const override {
    SB_DCHECK(max_cached_frames);
    SB_DCHECK(max_frames_per_append);

    *max_cached_frames = 256 * 1024;
    *max_frames_per_append = 16384;
  }
};

// static
scoped_ptr<PlayerComponents> PlayerComponents::Create() {
  return make_scoped_ptr<PlayerComponents>(new PlayerComponentsImpl);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
