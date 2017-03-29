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

#include "starboard/shared/starboard/player/filter/player_components.h"

#include <queue>

#include "starboard/shared/starboard/player/filter/audio_renderer_impl_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_impl_internal.h"

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
  SB_DCHECK(
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeInt16));
  return kSbMediaAudioSampleTypeInt16;
}

}  // namespace

class StubAudioDecoder : public AudioDecoder {
 public:
  explicit StubAudioDecoder(const SbMediaAudioHeader& audio_header)
      : sample_type_(GetSupportedSampleType()), audio_header_(audio_header) {}
  void Decode(const InputBuffer& input_buffer) SB_OVERRIDE {
    // Values to represent what kind of dummy audio to fill the decoded audio
    // we produce with.
    enum FillType {
      kSilence,
      kWave,
    };
    // Can be set locally to fill with different types.
    const FillType fill_type = kSilence;

    if (last_input_buffer_.is_valid()) {
      SbMediaTime diff = input_buffer.pts() - last_input_buffer_.pts();
      size_t sample_size =
          GetSampleType() == kSbMediaAudioSampleTypeInt16 ? 2 : 4;
      size_t size = diff * GetSamplesPerSecond() * sample_size *
                    audio_header_.number_of_channels / kSbMediaTimeSecond;
      decoded_audios_.push(new DecodedAudio(input_buffer.pts(), size));

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
    }
    last_input_buffer_ = input_buffer;
  }
  void WriteEndOfStream() SB_OVERRIDE {
    if (last_input_buffer_.is_valid()) {
      // There won't be a next pts, so just guess that the decoded size is
      // 4 times the encoded size.
      decoded_audios_.push(new DecodedAudio(last_input_buffer_.pts(),
                                            4 * last_input_buffer_.size()));
    }
    decoded_audios_.push(new DecodedAudio());
  }
  scoped_refptr<DecodedAudio> Read() SB_OVERRIDE {
    scoped_refptr<DecodedAudio> result;
    if (!decoded_audios_.empty()) {
      result = decoded_audios_.front();
      decoded_audios_.pop();
    }
    return result;
  }
  void Reset() SB_OVERRIDE {
    while (!decoded_audios_.empty()) {
      decoded_audios_.pop();
    }
    last_input_buffer_ = InputBuffer();
  }
  SbMediaAudioSampleType GetSampleType() const SB_OVERRIDE {
    return sample_type_;
  }
  int GetSamplesPerSecond() const SB_OVERRIDE {
    return audio_header_.samples_per_second;
  }

 private:
  SbMediaAudioSampleType sample_type_;
  SbMediaAudioHeader audio_header_;
  std::queue<scoped_refptr<DecodedAudio> > decoded_audios_;
  InputBuffer last_input_buffer_;
};

class StubVideoDecoder : public HostedVideoDecoder {
 public:
  StubVideoDecoder() : host_(NULL) {}
  void WriteInputBuffer(const InputBuffer& input_buffer) SB_OVERRIDE {
    SB_DCHECK(host_ != NULL);
    host_->OnDecoderStatusUpdate(
        kNeedMoreInput, VideoFrame::CreateEmptyFrame(input_buffer.pts()));
  }
  void WriteEndOfStream() SB_OVERRIDE {
    SB_DCHECK(host_ != NULL);
    host_->OnDecoderStatusUpdate(kBufferFull, VideoFrame::CreateEOSFrame());
  }
  void Reset() SB_OVERRIDE {}
  void SetHost(Host* host) {
    SB_DCHECK(host != NULL);
    SB_DCHECK(host_ == NULL);
    host_ = host;
  }

 private:
  Host* host_;
};

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  return output_mode == kSbPlayerOutputModePunchOut;
}
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

// static
scoped_ptr<PlayerComponents> PlayerComponents::Create(
    const AudioParameters& audio_parameters,
    const VideoParameters& video_parameters) {
  StubAudioDecoder* audio_decoder =
      new StubAudioDecoder(audio_parameters.audio_header);
  StubVideoDecoder* video_decoder = new StubVideoDecoder();
  AudioRendererImpl* audio_renderer =
      new AudioRendererImpl(audio_parameters.job_queue,
                            scoped_ptr<AudioDecoder>(audio_decoder).Pass(),
                            audio_parameters.audio_header);
  VideoRendererImpl* video_renderer = new VideoRendererImpl(
      scoped_ptr<HostedVideoDecoder>(video_decoder).Pass());

  return scoped_ptr<PlayerComponents>(
      new PlayerComponents(audio_renderer, video_renderer));
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
