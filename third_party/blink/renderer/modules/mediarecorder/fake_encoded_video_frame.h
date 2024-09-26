// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_FAKE_ENCODED_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_FAKE_ENCODED_VIDEO_FRAME_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "third_party/blink/public/web/modules/mediastream/encoded_video_frame.h"

namespace blink {

class FakeEncodedVideoFrame : public EncodedVideoFrame {
 public:
  class Builder {
   public:
    Builder& WithKeyFrame(bool is_key_frame) {
      is_key_frame_ = is_key_frame;
      return *this;
    }
    Builder& WithData(std::string data) {
      data_ = std::move(data);
      return *this;
    }
    Builder& WithCodec(media::VideoCodec codec) {
      codec_ = codec;
      return *this;
    }
    Builder& WithColorSpace(media::VideoColorSpace color_space) {
      color_space_ = color_space;
      return *this;
    }
    Builder& WithResolution(gfx::Size resolution) {
      resolution_ = resolution;
      return *this;
    }
    scoped_refptr<FakeEncodedVideoFrame> BuildRefPtr() {
      return base::MakeRefCounted<FakeEncodedVideoFrame>(
          is_key_frame_, std::move(data_), codec_, std::move(color_space_),
          resolution_);
    }

   private:
    bool is_key_frame_ = false;
    std::string data_;
    media::VideoCodec codec_ = media::VideoCodec::kVP8;
    absl::optional<media::VideoColorSpace> color_space_;
    gfx::Size resolution_{0, 0};
  };

  FakeEncodedVideoFrame(bool is_key_frame,
                        std::string data,
                        media::VideoCodec codec,
                        absl::optional<media::VideoColorSpace> color_space,
                        gfx::Size resolution)
      : is_key_frame_(is_key_frame),
        data_(std::move(data)),
        codec_(codec),
        color_space_(std::move(color_space)),
        resolution_(resolution) {}

  base::span<const uint8_t> Data() const override {
    return base::make_span(reinterpret_cast<const uint8_t*>(data_.data()),
                           data_.size());
  }
  media::VideoCodec Codec() const override { return codec_; }
  bool IsKeyFrame() const override { return is_key_frame_; }
  absl::optional<media::VideoColorSpace> ColorSpace() const override {
    return color_space_;
  }
  gfx::Size Resolution() const override { return resolution_; }

 private:
  bool is_key_frame_;
  std::string data_;
  media::VideoCodec codec_;
  absl::optional<media::VideoColorSpace> color_space_;
  gfx::Size resolution_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_FAKE_ENCODED_VIDEO_FRAME_H_
