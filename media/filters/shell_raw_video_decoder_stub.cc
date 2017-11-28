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

#include "media/filters/shell_raw_video_decoder_stub.h"

#include "base/logging.h"

namespace media {

namespace {

typedef ShellVideoDataAllocator::FrameBuffer FrameBuffer;
typedef ShellVideoDataAllocator::YV12Param YV12Param;

class ShellRawVideoDecoderStub : public ShellRawVideoDecoder {
 public:
  explicit ShellRawVideoDecoderStub(ShellVideoDataAllocator* allocator)
      : allocator_(allocator) {}
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  bool Flush() override;
  bool UpdateConfig(const VideoDecoderConfig& config) override;

 private:
  ShellVideoDataAllocator* allocator_;
  gfx::Size natural_size_;

  DISALLOW_COPY_AND_ASSIGN(ShellRawVideoDecoderStub);
};

void ShellRawVideoDecoderStub::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    const DecodeCB& decode_cb) {
  if (buffer->IsEndOfStream()) {
    decode_cb.Run(FRAME_DECODED, VideoFrame::CreateEmptyFrame());
    return;
  }

  size_t yuv_size = natural_size_.width() * natural_size_.height() * 3 / 2;
  scoped_refptr<FrameBuffer> frame_buffer =
      allocator_->AllocateFrameBuffer(yuv_size, 1);
  YV12Param param(natural_size_.width(), natural_size_.height(),
                  gfx::Rect(natural_size_), frame_buffer->data());
  scoped_refptr<VideoFrame> frame =
      allocator_->CreateYV12Frame(frame_buffer, param, buffer->GetTimestamp());
  decode_cb.Run(FRAME_DECODED, frame);
}

bool ShellRawVideoDecoderStub::Flush() {
  return true;
}

bool ShellRawVideoDecoderStub::UpdateConfig(const VideoDecoderConfig& config) {
  natural_size_ = config.natural_size();
  return true;
}

}  // namespace

scoped_ptr<ShellRawVideoDecoder> CreateShellRawVideoDecoderStub(
    ShellVideoDataAllocator* allocator,
    const VideoDecoderConfig& config,
    Decryptor* decryptor,
    bool was_encrypted) {
  DCHECK(allocator);
  scoped_ptr<ShellRawVideoDecoder> decoder(
      new ShellRawVideoDecoderStub(allocator));
  if (decoder->UpdateConfig(config))
    return decoder.Pass();
  return scoped_ptr<ShellRawVideoDecoder>();
}

}  // namespace media
