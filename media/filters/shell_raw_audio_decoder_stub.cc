/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_raw_audio_decoder_stub.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer_pool.h"
#include "media/mp4/aac.h"

namespace media {

namespace {

const size_t kSampleSizeInBytes = sizeof(float);

class ShellRawAudioDecoderStub : public ShellRawAudioDecoder {
 public:
  ShellRawAudioDecoderStub();

  int GetBytesPerSample() const override { return kSampleSizeInBytes; }
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decoder_cb) override;
  bool Flush() override;
  bool UpdateConfig(const AudioDecoderConfig& config) override;

 private:
  DecoderBufferPool decoder_buffer_pool_;
  size_t decoded_buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(ShellRawAudioDecoderStub);
};

ShellRawAudioDecoderStub::ShellRawAudioDecoderStub()
    : decoder_buffer_pool_(GetBytesPerSample()), decoded_buffer_size_(0) {}

void ShellRawAudioDecoderStub::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    const DecodeCB& decoder_cb) {
  DCHECK_NE(decoded_buffer_size_, 0);
  DCHECK(buffer);
  if (buffer->IsEndOfStream()) {
    decoder_cb.Run(BUFFER_DECODED, buffer);
    return;
  }
  scoped_refptr<DecoderBuffer> decoded_buffer =
      decoder_buffer_pool_.Allocate(decoded_buffer_size_);
  decoded_buffer->SetTimestamp(buffer->GetTimestamp());
  decoder_cb.Run(BUFFER_DECODED, decoded_buffer);
}

bool ShellRawAudioDecoderStub::Flush() {
  return true;
}

bool ShellRawAudioDecoderStub::UpdateConfig(const AudioDecoderConfig& config) {
  DCHECK(config.IsValidConfig() && config.codec() == kCodecAAC);
  ChannelLayout channel_layout = config.channel_layout();
  int channel_count = ChannelLayoutToChannelCount(channel_layout);
  if (channel_count != 1 && channel_count != 2 && channel_count != 6 &&
      channel_count != 8) {
    return false;
  }

  decoded_buffer_size_ =
      kSampleSizeInBytes * channel_count * mp4::AAC::kFramesPerAccessUnit;
  return true;
}

}  // namespace

scoped_ptr<ShellRawAudioDecoder> CreateShellRawAudioDecoderStub(
    const AudioDecoderConfig& config) {
  scoped_ptr<ShellRawAudioDecoder> decoder(new ShellRawAudioDecoderStub);
  if (!decoder->UpdateConfig(config)) {
    decoder.reset();
  }
  return decoder.Pass();
}

}  // namespace media
