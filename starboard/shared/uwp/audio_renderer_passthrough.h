// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_UWP_AUDIO_RENDERER_PASSTHROUGH_H_
#define STARBOARD_SHARED_UWP_AUDIO_RENDERER_PASSTHROUGH_H_

#include <functional>
#include <memory>
#include <queue>

#include "starboard/atomic.h"
#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/uwp/wasapi_audio_sink.h"
#include "starboard/time.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace uwp {

using ::starboard::shared::starboard::player::DecodedAudio;
using ::starboard::shared::starboard::player::JobQueue;
using ::starboard::shared::starboard::player::filter::AudioDecoder;
using ::starboard::shared::starboard::player::filter::AudioRenderer;
using ::starboard::shared::starboard::player::filter::MediaTimeProvider;

class AudioRendererPassthrough : public AudioRenderer,
                                 public MediaTimeProvider,
                                 private JobQueue::JobOwner {
 public:
  typedef starboard::media::AudioStreamInfo AudioStreamInfo;

  AudioRendererPassthrough(std::unique_ptr<AudioDecoder> audio_decoder,
                           const AudioStreamInfo& audio_stream_info);
  ~AudioRendererPassthrough() override;

  void Initialize(const ErrorCB& error_cb,
                  const PrerolledCB& prerolled_cb,
                  const EndedCB& ended_cb) override;
  void WriteSamples(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;

  void SetVolume(double volume) override;

  bool IsEndOfStreamWritten() const override;
  bool IsEndOfStreamPlayed() const override;
  bool CanAcceptMoreData() const override;

  // MediaTimeProvider methods
  void Play() override;
  void Pause() override;
  void SetPlaybackRate(double playback_rate) override;
  void Seek(SbTime seek_to_time) override;
  SbTime GetCurrentMediaTime(bool* is_playing,
                             bool* is_eos_played,
                             bool* is_underflow,
                             double* playback_rate) override;

 private:
  void OnDecoderConsumed();
  void OnDecoderOutput();

  void ProcessAudioBuffers();
  bool TryToWriteAudioBufferToSink(scoped_refptr<DecodedAudio> decoded_audio);

  // After end of stream is written, stop the audio sink after it finishes audio
  // output.
  void TryToEndStream();

  // Calculates the playback time elapsed since the last time the timestamp was
  // queried using WASAPIAudioSink::GetCurrentPlaybackTime().
  SbTime CalculateElapsedPlaybackTime(uint64_t update_time);
  // Calculates the final output timestamp of a DecodedAudio.
  SbTime CalculateLastOutputTime(scoped_refptr<DecodedAudio>& decoded_audio);

  const int kMaxDecodedAudios = 16;
  // About 250 ms of (E)AC3 audio.
  const int kNumPrerollDecodedAudios = 8;

  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  Mutex mutex_;
  bool paused_ = true;
  bool seeking_ = false;
  double playback_rate_ = 1.0;
  SbTime seeking_to_time_ = 0;

  atomic_bool end_of_stream_written_{false};
  atomic_bool end_of_stream_played_{false};
  // Use DecodedAudio to store decrypted and formatted encoded audio data.
  std::queue<scoped_refptr<DecodedAudio>> pending_inputs_;

  atomic_bool can_accept_more_data_;

  JobQueue::JobToken process_audio_buffers_job_token_;
  std::function<void()> process_audio_buffers_job_;

  int64_t total_frames_sent_to_sink_ = 0;
  int total_buffers_sent_to_sink_ = 0;

  const int channels_;
  const SbMediaAudioCodec codec_ = kSbMediaAudioCodecNone;
  std::unique_ptr<AudioDecoder> decoder_;
  std::unique_ptr<WASAPIAudioSink> sink_;

  // |iec_sample_rate_| is the sample rate of the stream when stored in an IEC
  // 61937 format. This may be different from the decoded sample rate.
  const int iec_sample_rate_;

  // Used to calculate the media time in GetCurrentMediaTime().
  LARGE_INTEGER performance_frequency_;
};

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_AUDIO_RENDERER_PASSTHROUGH_H_
