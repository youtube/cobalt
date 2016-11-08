// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "third_party/starboard/android/shared/mediacodec/mediacodec_audio_decoder.h"
#include "starboard/log.h"
#include "starboard/memory.h"


namespace starboard {
namespace shared {
namespace mediacodec {

AudioDecoder::AudioDecoder(SbMediaAudioCodec audio_codec,
                           const SbMediaAudioHeader& audio_header)
    : stream_ended_(false),
      audio_header_(audio_header) {
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAac);
  const char type[20] = "audio/mp4a-latm";
  mediacodec_ = AMediaCodec_createDecoderByType(type);
  if(mediacodec_ == NULL) {
    LOGI("mediacodec: audio: Failed to create for MIME type: %s\n", type);
    return;
  }
  mediaformat_ = AMediaFormat_new();

#if 0
 ////// @todo Extractor Code BEGIN
  AMediaExtractor *ex = AMediaExtractor_new();

  file_ =
      SbFileOpen("/storage/emulated/0/Android/data/com.example.native_activity/files/audio.aac",
              kSbFileOpenOnly | kSbFileRead, NULL, NULL);

  if (file_ == NULL)
      LOGI("mediacodec: audio: Couldn't open audio file %s %d", __FUNCTION__, __LINE__);

  LOGI("mediacodec: audio: File descriptor = %d", *(int *)file_);

  int err = AMediaExtractor_setDataSourceFd(ex, *(int*)file_, 0, 4096);
  if (err != 0)
      LOGI("mediacodec: audio: setDataSource error: %d", err);

  AMediaFormat *format = AMediaExtractor_getTrackFormat(ex, 0);

  const char *s = AMediaFormat_toString(format);
  LOGI("mediacodec: audio: format = %s", s);
#endif

  //TODO: Don't hardcode MIME type
  AMediaFormat_setString(mediaformat_, AMEDIAFORMAT_KEY_MIME, type);
  AMediaFormat_setInt32(mediaformat_, AMEDIAFORMAT_KEY_CHANNEL_COUNT, audio_header.number_of_channels);
  AMediaFormat_setInt32(mediaformat_, AMEDIAFORMAT_KEY_SAMPLE_RATE, audio_header.samples_per_second);
//  AMediaFormat_setInt32(mediaformat_, AMEDIAFORMAT_KEY_BIT_RATE,
//          audio_header.bits_per_sample * audio_header.samples_per_second *kSbMediaTimeSecond);
  AMediaFormat_setInt32(mediaformat_, AMEDIAFORMAT_KEY_IS_ADTS, 1);

  LOGI("mediacodec: audio: configuring: channels: %d, sample rate = %d",
          audio_header.number_of_channels, audio_header.samples_per_second);
#if 0
  LOGI("mediacodec: audio: specific config START");
  for (int i = 0; i < audio_header.audio_specific_config_size; i++) {
      LOGI("@@@  %X  @@@", audio_header.audio_specific_config[i]);
  }
#endif


  uint8_t es[2] = { 0x12, 0x10 };
  AMediaFormat_setBuffer(mediaformat_, "csd-0", es, 2);

  media_status_t status = AMediaCodec_configure(mediacodec_, mediaformat_, NULL, NULL, 0);
//  media_status_t status = AMediaCodec_configure(mediacodec_, format, NULL, NULL, 0);
  if(status != 0) {
    const char *format_string = AMediaFormat_toString(mediaformat_);
    LOGI("mediacodec: audio: Failed to configure for format: %s; status = %d\n", format_string, status);
    return;
  }
  status = AMediaCodec_start(mediacodec_);
  if(status != 0) {
    LOGI("mediacodec: audio: Failed to start; status = %d\n", status);
    return;
  }
  LOGI("mediacodec: audio: Initialized\n");
#if 0
  file_ =
      SbFileOpen("/storage/emulated/0/Android/data/com.example.native_activity/files/audio.aac",
              kSbFileCreateAlways | kSbFileWrite | kSbFileRead, NULL, NULL);

  if (file_ == NULL)
      LOGI("mediacodec: audio: Couldn't open audio file %s %d", __FUNCTION__, __LINE__);
#endif
}

AudioDecoder::~AudioDecoder() {
//  SbFileClose(file_);
  TeardownCodec();
}

void AudioDecoder::Decode(const InputBuffer& input_buffer,
                          std::vector<uint8_t>* output) {
  SB_CHECK(output != NULL);

  output->clear();

  if (stream_ended_) {
    LOGI("mediacodec: audio: Decode() is called after WriteEndOfStream() is called.");
    return;
  }

  const uint8_t* data = input_buffer.data();
  int input_size = input_buffer.size();

  if(data == NULL) {
    LOGI("mediacodec: audio: bad data from input buffer\n");
    output->clear();
    return;
  }

  /* Dequeue and fill input buffer */
  ssize_t index = AMediaCodec_dequeueInputBuffer(mediacodec_, kSbMediaTimeSecond);
  if(index < 0) {
    LOGI("mediacodec: audio: input buffer index: %d\n", index);
    output->clear();
    return;
  }

  size_t size;
  uint8_t* mc_input_buffer = AMediaCodec_getInputBuffer(mediacodec_, index, &size);
  if(mc_input_buffer == NULL || size <= 0) {
    LOGI("mediacodec: audio: bad input buffer: NULL or size 0\n");
    return;
  }
  size_t write_size = 0;
  if(size > (size_t)input_size) {
    write_size = (size_t)input_size;
  } else {
    write_size = size;
  }

//  LOGI("mediacodec: audio: input size = %ld, input buffer size = %ld, write_size = %ld\n", input_size, size, write_size);

  // TODO: Need to convert to interleaved float?  Or change paramater to <int>?
  SbMemoryCopy(mc_input_buffer, data, write_size);

#if 0
  // mediacodec: audio: Write to a local file
  int bytes_remaining = write_size;
  int bytes_written = 0;
  uint8_t* pData = const_cast<uint8_t*>(data);
  while (bytes_remaining > 0) {
    bytes_written = SbFileWrite(file_, (char*)pData, write_size);
    if (bytes_written < 0)
        SB_NOTREACHED();
    bytes_remaining -= bytes_written;
    pData += bytes_written;
  }
#endif
#if 0
  uint8_t* aac = mc_input_buffer;
  for (int32_t i = 0; i < write_size; i++) {
      LOGI(" aac data = %X", *aac++);
  }
#endif

  /* Queue input buffer */
  int64_t pts_us = (input_buffer.pts() * 1000000) / kSbMediaTimeSecond;
  media_status_t status = AMediaCodec_queueInputBuffer(mediacodec_, index, 0, write_size, pts_us, 0);
  if(status != 0) {
    LOGI("mediacodec: audio: error queueing input buffer; status = %d\n", status);
    return;
  }

  /* Dequeue and release output buffer */
  AMediaCodecBufferInfo info;
  ssize_t out_index = AMediaCodec_dequeueOutputBuffer(mediacodec_, &info, 200000/*10 * kSbTimeSecond*/);
  if(out_index < 0) {
    LOGI("mediacodec: audio: bad output buffer index: %d, pts = %d\n", out_index, input_buffer.pts());
    LOGI("mediacodec: audio: info.offset = %d, info.size = %d, info.pts = %ld, info.flags = %d, pts_us = %ld\n", info.offset, info.size, info.presentationTimeUs, info.flags, pts_us);
    return;
  }
  size = 0;
  uint8_t *output_buffer = AMediaCodec_getOutputBuffer(mediacodec_, out_index, &size);

  if(output_buffer != NULL && size > 0) {
    output->insert(output->end(), output_buffer + info.offset, &output_buffer[info.offset + info.size]);
//    output->resize(info.size);
//    SbMemoryCopy(output, output_buffer + info.offset, info.size);

//      LOGI("mediacodec: audio: Copied %d bytes to decoded stream", info.size);
  }

  AMediaFormat *format = AMediaCodec_getOutputFormat(mediacodec_);
  const char *format_string = AMediaFormat_toString(mediaformat_);
//  LOGI("mediacodec: audio: output format: %s\n", format_string);

  status = AMediaCodec_releaseOutputBuffer(mediacodec_, out_index, false);
  if(status != 0) {
    LOGI("mediacodec: audio: error releasing output buffer; status = %d\n", status);
    return;
  }
}

void AudioDecoder::WriteEndOfStream() {
  // AAC has no dependent frames so we needn't flush the decoder.  Set the flag
  // to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
}

void AudioDecoder::Reset() {
  stream_ended_ = false;
}

int AudioDecoder::GetSamplesPerSecond() {
  return audio_header_.samples_per_second;
}

void AudioDecoder::TeardownCodec() {
    LOGI("mediacodec: audio: Teardown\n");
    if(mediacodec_ != NULL) {
        AMediaCodec_delete(mediacodec_);
    }
    if(mediaformat_ != NULL) {
        AMediaFormat_delete(mediaformat_);
    }
}

}  // namespace mediacodec

namespace starboard {
namespace player {
namespace filter {

// static
AudioDecoder* AudioDecoder::Create(SbMediaAudioCodec audio_codec,
                                   const SbMediaAudioHeader& audio_header) {
  mediacodec::AudioDecoder* decoder =
      new mediacodec::AudioDecoder(audio_codec, audio_header);
  if (decoder->is_valid()) {
    return decoder;
  }
  delete decoder;
  return NULL;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard

}  // namespace shared
}  // namespace starboard
