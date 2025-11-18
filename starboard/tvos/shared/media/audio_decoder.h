// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_AUDIO_DECODER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_AUDIO_DECODER_H_

#import <AVFoundation/AVFoundation.h>

#include <queue>

#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_frame_discarder.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {

class TvosAudioDecoder : public AudioDecoder, private JobQueue::JobOwner {
 public:
  explicit TvosAudioDecoder(const AudioStreamInfo& audio_stream_info);
  ~TvosAudioDecoder() override;

  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const InputBuffers& input_buffers,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;

 private:
  struct AudioInputDataBlock {
    void* data;
    UInt32 length;
    AudioStreamPacketDescription packet_description;
    bool is_used;
  };

  TvosAudioDecoder(const TvosAudioDecoder&) = delete;
  TvosAudioDecoder& operator=(const TvosAudioDecoder&) = delete;

  static OSStatus AudioConverterComplexInputDataCallback(
      AudioConverterRef inAudioConverter,
      UInt32* ioDataPacketCount,
      AudioBufferList* ioData,
      AudioStreamPacketDescription** outDataPacketDescription,
      void* inUserData);

  void CreateAudioConverter();
  void ResetAudioConverter();
  void DestroyAudioConverter();

  const AudioStreamInfo audio_stream_info_;
  OutputCB output_cb_;
  ErrorCB error_cb_;
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;
  int bytes_per_frame_;
  UInt32 input_format_id_;
  UInt32 input_header_size_ = 0;
  UInt32 output_frames_per_packet_;

  AudioBufferList audio_buffer_list_;
  AudioConverterRef audio_converter_ = nullptr;
  AudioInputDataBlock audio_input_data_block_;

  AudioFrameDiscarder audio_frame_discarder_;
  bool stream_ended_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_AUDIO_DECODER_H_
