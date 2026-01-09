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

#include "starboard/tvos/shared/media/audio_decoder.h"

#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {

const int kAacFramesPerPacket = 1024;
const int kAc3InitialFramesPerPacket = 1248;
const int kAc3FivePointOneChannelMap[6] = {0, 2, 5, 1, 3, 4};
const int kAc3FramesPerPacket = 1536;
const int kADTSHeaderSize = 7;

TvosAudioDecoder::TvosAudioDecoder(const AudioStreamInfo& audio_stream_info)
    : audio_stream_info_(audio_stream_info),
      bytes_per_frame_(GetBytesPerSample(kSbMediaAudioSampleTypeFloat32) *
                       audio_stream_info_.number_of_channels),
      audio_buffer_list_{0},
      audio_input_data_block_{0} {
  if (audio_stream_info.codec == kSbMediaAudioCodecAac) {
    input_format_id_ = kAudioFormatMPEG4AAC;
    input_header_size_ = kADTSHeaderSize;
    output_frames_per_packet_ = kAacFramesPerPacket;
  } else if (audio_stream_info.codec == kSbMediaAudioCodecAc3) {
    input_format_id_ = kAudioFormatAC3;
    output_frames_per_packet_ = kAc3FramesPerPacket;
  } else if (audio_stream_info.codec == kSbMediaAudioCodecEac3) {
    input_format_id_ = kAudioFormatEnhancedAC3;
    output_frames_per_packet_ = kAc3FramesPerPacket;
  } else {
    SB_NOTREACHED();
  }

  audio_buffer_list_.mNumberBuffers = 1;
  audio_buffer_list_.mBuffers[0].mNumberChannels =
      audio_stream_info_.number_of_channels;
}

TvosAudioDecoder::~TvosAudioDecoder() {
  DestroyAudioConverter();
}

void TvosAudioDecoder::Initialize(OutputCB output_cb, ErrorCB error_cb) {
  SB_DCHECK(!output_cb_);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(!audio_converter_);

  output_cb_ = std::move(output_cb);
  error_cb_ = std::move(error_cb);
  CreateAudioConverter();
}

void TvosAudioDecoder::Decode(const InputBuffers& input_buffers,
                              const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);

  SB_DCHECK(consumed_cb);
  SB_DCHECK(audio_converter_);

  auto expected_output_frames_in_packet = output_frames_per_packet_;
  const auto& input_buffer = input_buffers[0];

  audio_frame_discarder_.OnInputBuffers(input_buffers);

  for (int i = 0; i < 2; ++i) {
    audio_input_data_block_.data =
        const_cast<uint8_t*>(input_buffer->data()) + input_header_size_;
    audio_input_data_block_.length = input_buffer->size() - input_header_size_;
    audio_input_data_block_.packet_description.mDataByteSize =
        audio_input_data_block_.length;
    audio_input_data_block_.packet_description.mStartOffset = 0;
    audio_input_data_block_.packet_description.mVariableFramesInPacket = 0;
    audio_input_data_block_.is_used = false;

    auto output_byte_size = expected_output_frames_in_packet * bytes_per_frame_;

    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        audio_stream_info_.number_of_channels, kSbMediaAudioSampleTypeFloat32,
        kSbMediaAudioFrameStorageTypeInterleaved, input_buffer->timestamp(),
        output_byte_size);
    audio_buffer_list_.mBuffers[0].mData = decoded_audio->data();
    audio_buffer_list_.mBuffers[0].mDataByteSize = output_byte_size;

    UInt32 actual_output_frames_in_packets = expected_output_frames_in_packet;
    OSStatus status = AudioConverterFillComplexBuffer(
        audio_converter_, AudioConverterComplexInputDataCallback,
        &audio_input_data_block_, &actual_output_frames_in_packets,
        &audio_buffer_list_, NULL);
    if (status != 0) {
      SB_LOG(ERROR) << "Error: cannot decode data (" << status << ").";
      error_cb_(kSbPlayerErrorDecode, "Error: cannot decode data.");
      return;
    }
    if (actual_output_frames_in_packets != expected_output_frames_in_packet) {
      // Theoretically this part of code should work with all codecs, but we
      // limit it to ac3 and ec3 for now.
      bool is_ac3_ec3 = audio_stream_info_.codec == kSbMediaAudioCodecAc3 ||
                        audio_stream_info_.codec == kSbMediaAudioCodecEac3;
      if (is_ac3_ec3 &&
          expected_output_frames_in_packet == kAc3FramesPerPacket &&
          actual_output_frames_in_packets == kAc3InitialFramesPerPacket) {
        SB_LOG(WARNING) << "Expect " << expected_output_frames_in_packet
                        << " frames, but only get "
                        << actual_output_frames_in_packets << " frames, retry.";
        ResetAudioConverter();
        expected_output_frames_in_packet = actual_output_frames_in_packets;
        continue;
      } else {
        SB_LOG(WARNING) << "Error: wrong output frames count, expected "
                        << expected_output_frames_in_packet << ", actual "
                        << actual_output_frames_in_packets << ".";
        error_cb_(kSbPlayerErrorDecode, "Error: wrong output frames count.");
        return;
      }
    }

    Schedule(consumed_cb);

    audio_frame_discarder_.AdjustForDiscardedDurations(
        audio_stream_info_.samples_per_second, &decoded_audio);

    decoded_audios_.push(decoded_audio);
    Schedule(output_cb_);
    return;
  }

  SB_NOTREACHED();
}

void TvosAudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  stream_ended_ = true;
  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);

  audio_frame_discarder_.OnDecodedAudioEndOfStream();

  Schedule(output_cb_);
}

scoped_refptr<DecodedAudio> TvosAudioDecoder::Read(int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  *samples_per_second = audio_stream_info_.samples_per_second;
  return result;
}

void TvosAudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(error_cb_);
  SB_DCHECK(audio_converter_);

  audio_frame_discarder_.Reset();
  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
  CancelPendingJobs();
  ResetAudioConverter();
}

// static
OSStatus TvosAudioDecoder::AudioConverterComplexInputDataCallback(
    AudioConverterRef in_audio_converter,
    UInt32* io_number_data_packets,
    AudioBufferList* io_data,
    AudioStreamPacketDescription** out_data_packet_description,
    void* in_user_data) {
  SB_DCHECK(in_user_data);

  AudioInputDataBlock* input_data =
      static_cast<AudioInputDataBlock*>(in_user_data);
  if (input_data->is_used) {
    *io_number_data_packets = 0;
    return 0;
  }

  *io_number_data_packets = 1;
  io_data->mNumberBuffers = 1;
  io_data->mBuffers[0].mData = input_data->data;
  io_data->mBuffers[0].mDataByteSize = input_data->length;
  if (out_data_packet_description) {
    *out_data_packet_description = &input_data->packet_description;
  }
  input_data->is_used = true;
  return 0;
}

void TvosAudioDecoder::CreateAudioConverter() {
  SB_DCHECK(audio_converter_ == nullptr);

  AudioStreamBasicDescription input_format = {0};
  input_format.mSampleRate = audio_stream_info_.samples_per_second;
  input_format.mFormatID = input_format_id_;
  input_format.mFormatFlags = kAudioFormatFlagIsFloat;
  input_format.mBitsPerChannel = 0;
  input_format.mChannelsPerFrame = audio_stream_info_.number_of_channels;
  input_format.mBytesPerFrame = 0;
  input_format.mFramesPerPacket = output_frames_per_packet_;
  input_format.mBytesPerPacket = 0;

  AudioStreamBasicDescription output_format = {0};
  output_format.mSampleRate = audio_stream_info_.samples_per_second;
  output_format.mFormatID = kAudioFormatLinearPCM;
  output_format.mFormatFlags = kAudioFormatFlagIsFloat;
  output_format.mBitsPerChannel =
      8 * GetBytesPerSample(kSbMediaAudioSampleTypeFloat32);
  output_format.mChannelsPerFrame = audio_stream_info_.number_of_channels;
  output_format.mBytesPerFrame = bytes_per_frame_;
  output_format.mFramesPerPacket = 1;
  output_format.mBytesPerPacket =
      output_format.mBytesPerFrame * output_format.mFramesPerPacket;

  OSStatus status =
      AudioConverterNew(&input_format, &output_format, &audio_converter_);
  if (status != 0) {
    error_cb_(kSbPlayerErrorDecode, "Error: cannot create audio converter.");
    SB_LOG(ERROR) << "Error: cannot create audio converter (" << status << ").";
    audio_converter_ = nullptr;
    return;
  }

  // Ac3/eac3 5.1 audio always have the same channel layout.
  if ((audio_stream_info_.codec == kSbMediaAudioCodecAc3 ||
       audio_stream_info_.codec == kSbMediaAudioCodecEac3) &&
      audio_stream_info_.number_of_channels == 6) {
    AudioConverterSetProperty(audio_converter_, kAudioConverterChannelMap,
                              sizeof(kAc3FivePointOneChannelMap),
                              kAc3FivePointOneChannelMap);
  }
}

void TvosAudioDecoder::ResetAudioConverter() {
  OSStatus status = AudioConverterReset(audio_converter_);
  if (status == 0) {
    return;
  }
  SB_LOG(ERROR) << "Error: cannot reset audio converter (" << status << ").";
  // Reset failed, try to recreate the audio converter.
  DestroyAudioConverter();
  CreateAudioConverter();
}

void TvosAudioDecoder::DestroyAudioConverter() {
  if (!audio_converter_) {
    return;
  }

  OSStatus status = AudioConverterDispose(audio_converter_);
  SB_LOG_IF(ERROR, status != 0)
      << "Error: cannot dispose audio converter (" << status << ").";
  audio_converter_ = nullptr;
}

}  // namespace starboard
