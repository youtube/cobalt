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

#include "starboard/shared/libfdkaac/fdk_aac_audio_decoder.h"

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/libfdkaac/libfdkaac_library_loader.h"

namespace starboard {
namespace shared {
namespace libfdkaac {

FdkAacAudioDecoder::FdkAacAudioDecoder() {
  static_assert(sizeof(INT_PCM) == sizeof(int16_t),
                "sizeof(INT_PCM) has to be the same as sizeof(int16_t).");
  InitializeCodec();
}

FdkAacAudioDecoder::~FdkAacAudioDecoder() {
  TeardownCodec();
}

void FdkAacAudioDecoder::Initialize(const OutputCB& output_cb,
                                    const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void FdkAacAudioDecoder::Decode(const InputBuffers& input_buffers,
                                const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(output_cb_);
  SB_DCHECK(decoder_ != NULL);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }
  const auto& input_buffer = input_buffers[0];
  if (!WriteToFdkDecoder(input_buffer)) {
    return;
  }

  timestamp_queue_.push(input_buffer->timestamp());
  Schedule(consumed_cb);
  ReadFromFdkDecoder(kDecodeModeDoNotFlush);
}

scoped_refptr<FdkAacAudioDecoder::DecodedAudio> FdkAacAudioDecoder::Read(
    int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  *samples_per_second = samples_per_second_;
  return result;
}

void FdkAacAudioDecoder::Reset() {
  SB_DCHECK(decoder_ != NULL);
  SB_DCHECK(BelongsToCurrentThread());

  TeardownCodec();
  InitializeCodec();

  stream_ended_ = false;
  decoded_audios_ = std::queue<scoped_refptr<DecodedAudio>>();  // clear
  partially_decoded_audio_ = nullptr;
  partially_decoded_audio_data_in_bytes_ = 0;
  timestamp_queue_ = std::queue<SbTime>();  // clear
  // Clean up stream information and deduced results.
  has_stream_info_ = false;
  num_channels_ = 0;
  samples_per_second_ = 0;
  decoded_audio_size_in_bytes_ = 0;
  audio_data_to_discard_in_bytes_ = 0;
  CancelPendingJobs();
}

void FdkAacAudioDecoder::WriteEndOfStream() {
  SB_DCHECK(decoder_ != NULL);
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  while (!timestamp_queue_.empty()) {
    if (!ReadFromFdkDecoder(kDecodeModeFlush)) {
      return;
    }
  }
  stream_ended_ = true;
  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);
  Schedule(output_cb_);
}

void FdkAacAudioDecoder::InitializeCodec() {
  SB_DCHECK(decoder_ == NULL);
  decoder_ = aacDecoder_Open(TT_MP4_ADTS, 1);
  SB_DCHECK(decoder_ != NULL);

  // Set AAC_PCM_MAX_OUTPUT_CHANNELS to 0 to disable downmixing feature.
  // It makes the decoder output contain the same number of channels as the
  // encoded bitstream.
  AAC_DECODER_ERROR error =
      aacDecoder_SetParam(decoder_, AAC_PCM_MAX_OUTPUT_CHANNELS, 0);
  SB_DCHECK(error == AAC_DEC_OK);
}

void FdkAacAudioDecoder::TeardownCodec() {
  if (decoder_) {
    aacDecoder_Close(decoder_);
    decoder_ = nullptr;
  }
}

bool FdkAacAudioDecoder::WriteToFdkDecoder(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

  unsigned char* data = const_cast<unsigned char*>(input_buffer->data());
  const unsigned int data_size_in_bytes = input_buffer->size();
  unsigned int left_to_decode_in_bytes = input_buffer->size();
  AAC_DECODER_ERROR error = aacDecoder_Fill(
      decoder_, &data, &data_size_in_bytes, &left_to_decode_in_bytes);
  if (error != AAC_DEC_OK) {
    SB_LOG(ERROR) << "aacDecoder_Fill() failed with result : " << error;
    error_cb_(kSbPlayerErrorDecode,
              FormatString("aacDecoder_Fill() failed with result %d.", error));
    return false;
  }

  // Returned |left_to_decode_in_bytes| should always be 0 as DecodeFrame() will
  // be called immediately on the same thread.
  SB_DCHECK(left_to_decode_in_bytes == 0);
  return true;
}

bool FdkAacAudioDecoder::ReadFromFdkDecoder(DecodeMode mode) {
  SB_DCHECK(mode == kDecodeModeFlush || mode == kDecodeModeDoNotFlush);
  int flags = mode == kDecodeModeFlush ? AACDEC_FLUSH : 0;

  AAC_DECODER_ERROR error = aacDecoder_DecodeFrame(
      decoder_, reinterpret_cast<INT_PCM*>(output_buffer_),
      kMaxOutputBufferSizeInBytes / sizeof(INT_PCM), flags);
  if (error != AAC_DEC_OK) {
    error_cb_(
        kSbPlayerErrorDecode,
        FormatString("aacDecoder_DecodeFrame() failed with result %d.", error));
    return false;
  }

  TryToUpdateStreamInfo();
  SB_DCHECK(has_stream_info_);
  if (audio_data_to_discard_in_bytes_ >= decoded_audio_size_in_bytes_) {
    // Discard all decoded data in |output_buffer_|.
    audio_data_to_discard_in_bytes_ -= decoded_audio_size_in_bytes_;
    return true;
  }

  // Discard the initial |audio_data_to_discard_in_bytes_| in |output_buffer_|.
  int offset_in_bytes = audio_data_to_discard_in_bytes_;
  audio_data_to_discard_in_bytes_ = 0;
  TryToOutputDecodedAudio(output_buffer_ + offset_in_bytes,
                          decoded_audio_size_in_bytes_ - offset_in_bytes);
  return true;
}

void FdkAacAudioDecoder::TryToUpdateStreamInfo() {
  if (has_stream_info_) {
    return;
  }
  CStreamInfo* stream_info = aacDecoder_GetStreamInfo(decoder_);
  SB_DCHECK(stream_info);

  num_channels_ = stream_info->numChannels;
  samples_per_second_ = stream_info->sampleRate;
  decoded_audio_size_in_bytes_ =
      sizeof(int16_t) * stream_info->frameSize * num_channels_;
  audio_data_to_discard_in_bytes_ =
      sizeof(int16_t) * stream_info->outputDelay * num_channels_;
  has_stream_info_ = true;
}

void FdkAacAudioDecoder::TryToOutputDecodedAudio(const uint8_t* data,
                                                 int size_in_bytes) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(has_stream_info_);

  while (size_in_bytes > 0 && !timestamp_queue_.empty()) {
    if (!partially_decoded_audio_) {
      SB_DCHECK(partially_decoded_audio_data_in_bytes_ == 0);
      partially_decoded_audio_ = new DecodedAudio(
          num_channels_, kSbMediaAudioSampleTypeInt16Deprecated,
          kSbMediaAudioFrameStorageTypeInterleaved, timestamp_queue_.front(),
          decoded_audio_size_in_bytes_);
    }
    int freespace = static_cast<int>(partially_decoded_audio_->size()) -
                    partially_decoded_audio_data_in_bytes_;
    if (size_in_bytes >= freespace) {
      memcpy(partially_decoded_audio_->buffer() +
                 partially_decoded_audio_data_in_bytes_,
             data, freespace);
      data += freespace;
      size_in_bytes -= freespace;
      SB_DCHECK(timestamp_queue_.front() ==
                partially_decoded_audio_->timestamp());
      timestamp_queue_.pop();
      decoded_audios_.push(partially_decoded_audio_);
      Schedule(output_cb_);
      partially_decoded_audio_ = nullptr;
      partially_decoded_audio_data_in_bytes_ = 0;
      continue;
    }
    memcpy(partially_decoded_audio_->buffer() +
               partially_decoded_audio_data_in_bytes_,
           data, size_in_bytes);
    partially_decoded_audio_data_in_bytes_ += size_in_bytes;
    return;
  }
}

}  // namespace libfdkaac
}  // namespace shared
}  // namespace starboard
