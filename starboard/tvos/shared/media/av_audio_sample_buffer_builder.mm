// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#import "starboard/tvos/shared/media/av_audio_sample_buffer_builder.h"

#include <vector>

#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "third_party/opus/src/include/opus.h"
#include "third_party/opus/src/include/opus_multistream.h"

namespace starboard {

namespace {

constexpr AudioFormatID kAudioFormatMPEG4QAAC =
    'qaac';  // 0x71, 0x61, 0x61, 0x63

constexpr int kAacFramesPerPacket = 1024;
constexpr int kADTSHeaderSize = 7;

class AacAVSampleBufferBuilder : public AVAudioSampleBufferBuilder {
 public:
  AacAVSampleBufferBuilder(const AudioStreamInfo& audio_stream_info,
                           bool is_encrypted)
      : AVAudioSampleBufferBuilder(audio_stream_info) {
    AudioStreamBasicDescription input_format = {0};
    input_format.mSampleRate = audio_stream_info.samples_per_second;
    input_format.mFormatID =
        is_encrypted ? kAudioFormatMPEG4QAAC : kAudioFormatMPEG4AAC;
    input_format.mFormatFlags = kAudioFormatFlagIsFloat;
    input_format.mBitsPerChannel = 0;
    input_format.mChannelsPerFrame = audio_stream_info.number_of_channels;
    input_format.mBytesPerFrame = 0;
    input_format.mFramesPerPacket = kAacFramesPerPacket;
    input_format.mBytesPerPacket = 0;

    OSStatus status = CMAudioFormatDescriptionCreate(
        kCFAllocatorDefault, &input_format, 0, NULL, 0, NULL, NULL,
        &format_description_);
    if (status != 0) {
      RecordOSError("CreateAudioFormatDescription", status);
    }
  }

  ~AacAVSampleBufferBuilder() override { CFRelease(format_description_); }

  bool BuildSampleBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                         int64_t media_time_offset,
                         CMSampleBufferRef* sample_buffer,
                         size_t* frames_in_buffer) override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK(input_buffer);
    SB_DCHECK(sample_buffer);
    SB_DCHECK(frames_in_buffer);
    SB_DCHECK(!error_occurred_);
    SB_DCHECK(input_buffer->audio_stream_info().codec == kSbMediaAudioCodecAac);

    CMBlockBufferRef block;
    // TODO: Avoid releasing |input_buffer| before |sample_buffer| is released,
    // the internal data may be still in use.
    OSStatus status = CMBlockBufferCreateWithMemoryBlock(
        NULL, const_cast<uint8_t*>(input_buffer->data() + kADTSHeaderSize),
        input_buffer->size() - kADTSHeaderSize, kCFAllocatorNull, NULL, 0,
        input_buffer->size() - kADTSHeaderSize, 0, &block);
    if (status != 0) {
      RecordOSError("CreateBlockBuffer", status);
      return false;
    }

    AudioStreamPacketDescription packet_desc = {0};
    packet_desc.mDataByteSize = input_buffer->size() - kADTSHeaderSize;
    packet_desc.mStartOffset = 0;
    packet_desc.mVariableFramesInPacket = kAacFramesPerPacket;

    status = CMAudioSampleBufferCreateWithPacketDescriptions(
        NULL, block, true, NULL, NULL, format_description_, 1,
        CMTimeMake(input_buffer->timestamp() + media_time_offset, 1000000),
        &packet_desc, sample_buffer);
    CFRelease(block);
    if (status != 0) {
      RecordOSError("CreateSampleBuffer", status);
      return false;
    }
    *frames_in_buffer = kAacFramesPerPacket;
    return true;
  }

 private:
  CMAudioFormatDescriptionRef format_description_ = {0};
};

typedef struct {
  int nb_streams;
  int nb_coupled_streams;
  unsigned char mapping[8];
} VorbisLayout;

/* Index is nb_channel-1 */
static const VorbisLayout vorbis_mappings[8] = {
    {1, 0, {0}},                      /* 1: mono */
    {1, 1, {0, 1}},                   /* 2: stereo */
    {2, 1, {0, 2, 1}},                /* 3: 1-d surround */
    {2, 2, {0, 1, 2, 3}},             /* 4: quadraphonic surround */
    {3, 2, {0, 4, 1, 2, 3}},          /* 5: 5-channel surround */
    {4, 2, {0, 4, 1, 2, 3, 5}},       /* 6: 5.1 surround */
    {4, 3, {0, 4, 1, 2, 3, 5, 6}},    /* 7: 6.1 surround */
    {5, 3, {0, 6, 1, 2, 3, 4, 5, 7}}, /* 8: 7.1 surround */
};

class OpusAVSampleBufferBuilder : public AVAudioSampleBufferBuilder {
 public:
  explicit OpusAVSampleBufferBuilder(const AudioStreamInfo& audio_stream_info)
      : AVAudioSampleBufferBuilder(audio_stream_info) {
    AudioStreamBasicDescription input_format = {0};
    input_format.mSampleRate = audio_stream_info_.samples_per_second;
    input_format.mFormatID = kAudioFormatLinearPCM;
    input_format.mFormatFlags = kAudioFormatFlagIsFloat;
    input_format.mBitsPerChannel =
        8 * GetBytesPerSample(kSbMediaAudioSampleTypeFloat32);
    input_format.mChannelsPerFrame = audio_stream_info_.number_of_channels;
    input_format.mBytesPerFrame =
        audio_stream_info_.number_of_channels *
        GetBytesPerSample(kSbMediaAudioSampleTypeFloat32);
    input_format.mFramesPerPacket = 1;
    input_format.mBytesPerPacket =
        input_format.mBytesPerFrame * input_format.mFramesPerPacket;

    AudioChannelLayout channel_layout = {0};
    channel_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;

    OSStatus status = CMAudioFormatDescriptionCreate(
        NULL, &input_format, sizeof(channel_layout), &channel_layout, 0, NULL,
        NULL, &format_description_);
    if (status != 0) {
      RecordOSError("CreateAudioFormatDescription", status);
    }

    const VorbisLayout& layout =
        vorbis_mappings[audio_stream_info_.number_of_channels - 1];
    int error;
    opus_decoder_ = opus_multistream_decoder_create(
        audio_stream_info_.samples_per_second,
        audio_stream_info_.number_of_channels, layout.nb_streams,
        layout.nb_coupled_streams, layout.mapping, &error);
    if (error != OPUS_OK) {
      RecordError("Failed to create decoder");
    }
  }

  ~OpusAVSampleBufferBuilder() override {
    if (opus_decoder_) {
      opus_multistream_decoder_destroy(opus_decoder_);
    }
    CFRelease(format_description_);
  }

  bool BuildSampleBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                         int64_t media_time_offset,
                         CMSampleBufferRef* sample_buffer,
                         size_t* frames_in_buffer) override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK(input_buffer);
    SB_DCHECK(sample_buffer);
    SB_DCHECK(frames_in_buffer);
    SB_DCHECK(!error_occurred_);
    SB_DCHECK(input_buffer->audio_stream_info().codec ==
              kSbMediaAudioCodecOpus);

    int frames_in_packet =
        opus_packet_get_nb_frames(input_buffer->data(), input_buffer->size());
    int samples_per_frame = opus_packet_get_samples_per_frame(
        input_buffer->data(), audio_stream_info_.samples_per_second);
    int frams_size = frames_in_packet * samples_per_frame;
    SB_DCHECK(frams_size > 0);

    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        audio_stream_info_.number_of_channels, kSbMediaAudioSampleTypeFloat32,
        kSbMediaAudioFrameStorageTypeInterleaved,
        input_buffer->timestamp() + media_time_offset,
        audio_stream_info_.number_of_channels * frams_size *
            GetBytesPerSample(kSbMediaAudioSampleTypeFloat32));
    int decoded_frames = opus_multistream_decode_float(
        opus_decoder_, static_cast<const unsigned char*>(input_buffer->data()),
        input_buffer->size(), reinterpret_cast<float*>(decoded_audio->data()),
        frams_size, 0);

    SB_DCHECK(decoded_frames == frams_size);

    CMBlockBufferRef block;
    OSStatus status = CMBlockBufferCreateWithMemoryBlock(
        NULL, NULL, decoded_audio->size_in_bytes(), NULL, NULL, 0,
        decoded_audio->size_in_bytes(), 0, &block);
    if (status != 0) {
      RecordOSError("BlockBufferCreate", status);
      return false;
    }

    status = CMBlockBufferReplaceDataBytes(decoded_audio->data(), block, 0,
                                           decoded_audio->size_in_bytes());
    if (status != 0) {
      RecordOSError("BlockCopyData", status);
      CFRelease(block);
      return false;
    }

    status = CMAudioSampleBufferCreateWithPacketDescriptions(
        NULL, block, true, NULL, NULL, format_description_, decoded_frames,
        CMTimeMake(input_buffer->timestamp() + media_time_offset, 1000000),
        NULL, sample_buffer);
    CFRelease(block);
    if (status != 0) {
      RecordOSError("CreateSampleBuffer", status);
      return false;
    }
    *frames_in_buffer = decoded_frames;
    return true;
  }

 private:
  CMAudioFormatDescriptionRef format_description_ = {0};
  OpusMSDecoder* opus_decoder_ = nullptr;
};

const int kAc3FramesPerPacket = 1536;

// TODO: Abstract AacAVSampleBufferBuilder and Ac3AVSampleBufferBuilder.
class Ac3AVSampleBufferBuilder : public AVAudioSampleBufferBuilder {
 public:
  explicit Ac3AVSampleBufferBuilder(const AudioStreamInfo& audio_stream_info)
      : AVAudioSampleBufferBuilder(audio_stream_info) {
    AudioStreamBasicDescription input_format = {0};
    if (audio_stream_info.codec == kSbMediaAudioCodecAc3) {
      input_format.mFormatID = kAudioFormatAC3;
    } else {
      SB_DCHECK(audio_stream_info.codec == kSbMediaAudioCodecEac3);
      input_format.mFormatID = kAudioFormatEnhancedAC3;
    }
    input_format.mSampleRate = audio_stream_info_.samples_per_second;
    input_format.mFormatFlags = kAudioFormatFlagIsFloat;
    input_format.mBitsPerChannel = 0;
    input_format.mChannelsPerFrame = audio_stream_info_.number_of_channels;
    input_format.mBytesPerFrame = 0;
    input_format.mFramesPerPacket = kAc3FramesPerPacket;
    input_format.mBytesPerPacket = 0;

    OSStatus status;

    if (audio_stream_info_.number_of_channels == 6) {
      AudioChannelLayout channel_layout = {0};
      channel_layout.mChannelLayoutTag = kAudioChannelLayoutTag_EAC_6_0_A;

      status = CMAudioFormatDescriptionCreate(
          kCFAllocatorDefault, &input_format, sizeof(channel_layout),
          &channel_layout, 0, NULL, NULL, &format_description_);
    } else {
      status = CMAudioFormatDescriptionCreate(kCFAllocatorDefault,
                                              &input_format, 0, NULL, 0, NULL,
                                              NULL, &format_description_);
    }
    if (status != 0) {
      RecordOSError("CreateAudioFormatDescription", status);
    }
  }

  ~Ac3AVSampleBufferBuilder() override { CFRelease(format_description_); }

  bool BuildSampleBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                         int64_t media_time_offset,
                         CMSampleBufferRef* sample_buffer,
                         size_t* frames_in_buffer) override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK(input_buffer);
    SB_DCHECK(sample_buffer);
    SB_DCHECK(frames_in_buffer);
    SB_DCHECK(!error_occurred_);
    SB_DCHECK(input_buffer->audio_stream_info().codec ==
              audio_stream_info_.codec);

    CMBlockBufferRef block;
    // TODO: Avoid releasing |input_buffer| before |sample_buffer| is released,
    // the internal data may be still in use.
    OSStatus status = CMBlockBufferCreateWithMemoryBlock(
        NULL, const_cast<uint8_t*>(input_buffer->data()), input_buffer->size(),
        kCFAllocatorNull, NULL, 0, input_buffer->size(), 0, &block);
    if (status != 0) {
      RecordOSError("CreateBlockBuffer", status);
      return false;
    }

    AudioStreamPacketDescription packet_desc = {0};
    packet_desc.mDataByteSize = input_buffer->size();
    packet_desc.mStartOffset = 0;
    packet_desc.mVariableFramesInPacket = kAc3FramesPerPacket;

    status = CMAudioSampleBufferCreateWithPacketDescriptions(
        NULL, block, true, NULL, NULL, format_description_, 1,
        CMTimeMake(input_buffer->timestamp() + media_time_offset, 1000000),
        &packet_desc, sample_buffer);
    CFRelease(block);
    if (status != 0) {
      RecordOSError("CreateSampleBuffer", status);
      return false;
    }
    *frames_in_buffer = kAc3FramesPerPacket;
    return true;
  }

 private:
  CMAudioFormatDescriptionRef format_description_ = {0};
};

}  // namespace

// static
AVAudioSampleBufferBuilder* AVAudioSampleBufferBuilder::CreateBuilder(
    const AudioStreamInfo& audio_stream_info,
    bool is_encrypted) {
  if (audio_stream_info.codec == kSbMediaAudioCodecAac) {
    return new AacAVSampleBufferBuilder(audio_stream_info, is_encrypted);
  }
  if (audio_stream_info.codec == kSbMediaAudioCodecOpus) {
    return new OpusAVSampleBufferBuilder(audio_stream_info);
  }
  if (audio_stream_info.codec == kSbMediaAudioCodecAc3 ||
      audio_stream_info.codec == kSbMediaAudioCodecEac3) {
    return new Ac3AVSampleBufferBuilder(audio_stream_info);
  }
  SB_NOTREACHED();
  return nullptr;
}

void AVAudioSampleBufferBuilder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  error_occurred_ = false;
}

bool AVAudioSampleBufferBuilder::BuildSilenceSampleBuffer(
    int64_t timestamp,
    CMSampleBufferRef* sample_buffer,
    size_t* frames_in_buffer) {
  size_t bytes_per_sample = GetBytesPerSample(kSbMediaAudioSampleTypeFloat32);
  size_t bytes_per_frame =
      bytes_per_sample * audio_stream_info_.number_of_channels;
  CMAudioFormatDescriptionRef format_description = {0};
  AudioStreamBasicDescription input_format = {0};
  input_format.mSampleRate = audio_stream_info_.samples_per_second;
  input_format.mFormatID = kAudioFormatLinearPCM;
  input_format.mFormatFlags = kAudioFormatFlagIsFloat;
  input_format.mBitsPerChannel = 8 * bytes_per_sample;
  input_format.mChannelsPerFrame = audio_stream_info_.number_of_channels;
  input_format.mBytesPerFrame = bytes_per_frame;
  input_format.mFramesPerPacket = 1;
  input_format.mBytesPerPacket =
      input_format.mBytesPerFrame * input_format.mFramesPerPacket;

  AudioChannelLayout channel_layout = {0};
  if (audio_stream_info_.number_of_channels == 1) {
    channel_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
  } else if (audio_stream_info_.number_of_channels == 2) {
    channel_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
  } else if (audio_stream_info_.number_of_channels == 6) {
    channel_layout.mChannelLayoutTag = kAudioChannelLayoutTag_EAC_6_0_A;
  }

  OSStatus status = CMAudioFormatDescriptionCreate(
      NULL, &input_format, sizeof(channel_layout), &channel_layout, 0, NULL,
      NULL, &format_description);
  if (status != 0) {
    RecordOSError("CreateAudioFormatDescription", status);
    return false;
  }

  const int kSilenceBufferSize = 24 * 1024;
  static std::vector<uint8_t> silence_buffer(kSilenceBufferSize, 0);

  size_t silence_frames = kSilenceBufferSize / bytes_per_frame;
  CMBlockBufferRef block;
  status = CMBlockBufferCreateWithMemoryBlock(
      NULL, silence_buffer.data(), silence_frames * bytes_per_frame,
      kCFAllocatorNull, NULL, 0, silence_frames * bytes_per_frame, 0, &block);
  if (status != 0) {
    RecordOSError("BlockBufferCreate", status);
    CFRelease(format_description);
    return false;
  }

  status = CMAudioSampleBufferCreateWithPacketDescriptions(
      NULL, block, true, NULL, NULL, format_description, silence_frames,
      CMTimeMake(timestamp, 1000000), NULL, sample_buffer);
  CFRelease(format_description);
  CFRelease(block);
  if (status != 0) {
    RecordOSError("CreateSampleBuffer", status);
    return false;
  }
  *frames_in_buffer = silence_frames;
  return true;
}

void AVAudioSampleBufferBuilder::RecordOSError(const char* action,
                                               OSStatus status) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(status != 0);

  std::stringstream ss;
  ss << action << " failed with error " << status;
  RecordError(ss.str());
}

void AVAudioSampleBufferBuilder::RecordError(const std::string& message) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  error_message_ = message;
  error_occurred_ = true;
}

}  // namespace starboard
