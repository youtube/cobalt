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

#ifndef STARBOARD_SHARED_LIBFDKAAC_FDK_AAC_AUDIO_DECODER_H_
#define STARBOARD_SHARED_LIBFDKAAC_FDK_AAC_AUDIO_DECODER_H_

#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "third_party/libfdkaac/include/aacdecoder_lib.h"

namespace starboard {
namespace shared {
namespace libfdkaac {

class FdkAacAudioDecoder : public starboard::player::filter::AudioDecoder,
                           private starboard::player::JobQueue::JobOwner {
 public:
  // The max supportable channels to be decoded for fdk aac is 8.
  static constexpr int kMaxChannels = 8;

  FdkAacAudioDecoder();
  ~FdkAacAudioDecoder() override;

  // Overriding functions from starboard::player::filter::AudioDecoder.
  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const InputBuffers& input_buffers,
              const ConsumedCB& consumed_cb) override;
  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;
  void WriteEndOfStream() override;

 private:
  // An AAC access unit can contain at most 2048 PCM samples (when it's HE-AAC).
  static constexpr int kMaxSamplesPerAccessUnit = 2048;
  // The max bytes required to store a decoded access unit.
  static constexpr int kMaxOutputBufferSizeInBytes =
      sizeof(int16_t) * kMaxSamplesPerAccessUnit * kMaxChannels;

  enum DecodeMode {
    kDecodeModeFlush,
    kDecodeModeDoNotFlush,
  };

  void InitializeCodec();
  void TeardownCodec();
  bool WriteToFdkDecoder(const scoped_refptr<InputBuffer>& input_buffer);
  bool ReadFromFdkDecoder(DecodeMode mode);
  void TryToUpdateStreamInfo();
  void TryToOutputDecodedAudio(const uint8_t* audio_data, int size_in_bytes);

  OutputCB output_cb_;
  ErrorCB error_cb_;

  bool stream_ended_ = false;
  uint8_t output_buffer_[kMaxOutputBufferSizeInBytes];

  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;
  // The DecodedAudio being filled up, will be appended to |decoded_audios_|
  // once it's fully filled (and |output_cb_| will be called).
  scoped_refptr<DecodedAudio> partially_decoded_audio_;
  int partially_decoded_audio_data_in_bytes_ = 0;
  // Keep timestamps for inputs, which will be used to create DecodedAudio.
  std::queue<SbTime> timestamp_queue_;
  // libfdkaac related parameters are listed below.
  HANDLE_AACDECODER decoder_ = nullptr;
  // There are two quirks of the fdk aac decoder:
  // 1. Its output parameters (contained in CStreamInfo) are only filled after
  //    the first aacDecoder_DecodeFrame() call.
  // 2. When filled with N aac access units (i.e. InputBuffer), it will produce
  //    `stream_info->outputDelay + stream_info->frameSize * N` output samples.
  //    The first `outputDelay` samples should be discarded and the remaining
  //    samples contain valid output.
  bool has_stream_info_ = false;
  int num_channels_ = 0;
  int samples_per_second_ = 0;
  size_t decoded_audio_size_in_bytes_ = 0;
  // How many bytes of audio output left to be discarded.
  size_t audio_data_to_discard_in_bytes_ = 0;
};

}  // namespace libfdkaac
}  // namespace shared
}  // namespace starboard
#endif  // STARBOARD_SHARED_LIBFDKAAC_FDK_AAC_AUDIO_DECODER_H_
