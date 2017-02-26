/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/image/animated_webp_image.h"

#include "cobalt/loader/image/webp_image_decoder.h"
#include "starboard/memory.h"

namespace cobalt {
namespace loader {
namespace image {

const int kPixelSize = 4;
const int kLoopInfinite = 0;

AnimatedWebPImage::AnimatedWebPImage(
    const math::Size& size, bool is_opaque,
    render_tree::ResourceProvider* resource_provider)
    : thread_("AnimatedWebPImage Decoding"),
      size_(size),
      is_opaque_(is_opaque),
      demux_(NULL),
      demux_state_(WEBP_DEMUX_PARSING_HEADER),
      frame_count_(0),
      loop_count_(kLoopInfinite),
      background_color_(0),
      current_frame_index_(0),
      next_frame_index_(0),
      resource_provider_(resource_provider) {}

scoped_refptr<render_tree::Image> AnimatedWebPImage::GetFrame() {
  base::AutoLock lock(lock_);
  return current_frame_image_;
}

void AnimatedWebPImage::AppendChunk(const uint8* data, size_t input_byte) {
  base::AutoLock lock(lock_);

  data_buffer_.insert(data_buffer_.end(), data, data + input_byte);
  WebPData webp_data = {&data_buffer_[0], data_buffer_.size()};
  WebPDemuxDelete(demux_);
  demux_ = WebPDemuxPartial(&webp_data, &demux_state_);
  DCHECK_GT(demux_state_, WEBP_DEMUX_PARSING_HEADER);

  // Update frame count.
  int new_frame_count = WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT);
  if (new_frame_count > 0 && frame_count_ == 0) {
    // We've just received the first frame, start playing animation now.

    loop_count_ = WebPDemuxGetI(demux_, WEBP_FF_LOOP_COUNT);
    background_color_ = WebPDemuxGetI(demux_, WEBP_FF_BACKGROUND_COLOR);
    image_buffer_ = AllocateImageData();
    DCHECK(image_buffer_);
    FillImageBufferWithBackgroundColor();

    current_frame_time_ = base::TimeTicks::Now();
    current_frame_index_ = 0;
    thread_.Start();
    thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AnimatedWebPImage::DecodeFrames, base::Unretained(this)));
  }
  frame_count_ = new_frame_count;
}

void AnimatedWebPImage::DecodeFrames() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::DecodeFrames()");
  base::AutoLock lock(lock_);

  UpdateTimelineInfo();

  // Decode the frames from current frame to next frame and blend the results
  // if necessary.
  for (int frame_index = current_frame_index_ + 1;
       frame_index <= next_frame_index_; ++frame_index) {
    WebPIterator webp_iterator;
    WebPDemuxGetFrame(demux_, frame_index, &webp_iterator);
    if (!webp_iterator.complete) {
      break;
    }

    WEBPImageDecoder webp_image_decoder(resource_provider_);
    webp_image_decoder.DecodeChunk(webp_iterator.fragment.bytes,
                                   webp_iterator.fragment.size);
    WebPDemuxReleaseIterator(&webp_iterator);
    if (!webp_image_decoder.FinishWithSuccess()) {
      LOG(ERROR) << "Failed to decode WebP image frame.";
      break;
    }

    scoped_ptr<render_tree::ImageData> next_frame_data =
        webp_image_decoder.RetrieveImageData();
    DCHECK(next_frame_data);

    uint8_t* image_buffer_memory = image_buffer_->GetMemory();
    uint8_t* next_frame_memory = next_frame_data->GetMemory();
    // Alpha-blending: Given that each of the R, G, B and A channels is 8-bit,
    // and the RGB channels are not premultiplied by alpha, the formula for
    // blending 'dst' onto 'src' is:
    // blend.A = src.A + dst.A * (1 - src.A / 255)
    // if blend.A = 0 then
    //   blend.RGB = 0
    // else
    //   blend.RGB = (src.RGB * src.A +
    //                dst.RGB * dst.A * (1 - src.A / 255)) / blend.A
    //   https://developers.google.com/speed/webp/docs/riff_container#animation
    for (int y = 0; y < webp_iterator.height; ++y) {
      for (int x = 0; x < webp_iterator.width; ++x) {
        uint8_t* src =
            next_frame_memory + (y * webp_iterator.width + x) * kPixelSize;
        uint8_t* dst = image_buffer_memory +
                       ((y + webp_iterator.y_offset) * size_.width() + x +
                        webp_iterator.x_offset) *
                           kPixelSize;
        uint8_t blend[4];
        blend[3] = static_cast<uint8_t>(src[3] + dst[3] * (1 - src[3] / 255.0));
        if (blend[3] == 0) {
          blend[0] = blend[1] = blend[2] = 0;
        } else {
          for (int i = 0; i < 3; ++i) {
            blend[i] = static_cast<uint8_t>(
                (src[i] * src[3] + dst[i] * dst[3] * (1 - src[3] / 255.0)) /
                blend[3]);
          }
        }
        SbMemoryCopy(dst, &blend, kPixelSize);
      }
    }

    if (webp_iterator.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND) {
      FillImageBufferWithBackgroundColor();
    }
  }

  // Generate the next frame render tree image if necessary.
  if (current_frame_index_ != next_frame_index_) {
    current_frame_index_ = next_frame_index_;
    scoped_ptr<render_tree::ImageData> next_frame_data = AllocateImageData();
    DCHECK(next_frame_data);
    uint8_t* image_buffer_memory = image_buffer_->GetMemory();
    uint8_t* next_frame_memory = next_frame_data->GetMemory();
    SbMemoryCopy(next_frame_memory, image_buffer_memory,
                 size_.width() * size_.height() * kPixelSize);
    current_frame_image_ =
        resource_provider_->CreateImage(next_frame_data.Pass());
  }

  // Set up the next time to call the decode callback.
  base::TimeDelta delay = next_frame_time_ - base::TimeTicks::Now();
  if (delay < base::TimeDelta()) {
    delay = base::TimeDelta();
  }
  thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AnimatedWebPImage::DecodeFrames, base::Unretained(this)),
      delay);

  if (!decoded_callback_.is_null()) {
    decoded_callback_.Run();
  }
}

void AnimatedWebPImage::UpdateTimelineInfo() {
  base::TimeTicks current_time = base::TimeTicks::Now();
  next_frame_index_ = current_frame_index_ ? current_frame_index_ : 1;
  while (true) {
    // Decode frames, until a frame such that the duration covers the current
    // time, i.e. the next frame should be displayed in the future.
    WebPIterator webp_iterator;
    WebPDemuxGetFrame(demux_, next_frame_index_, &webp_iterator);
    next_frame_time_ = current_frame_time_ + base::TimeDelta::FromMilliseconds(
                                                 webp_iterator.duration);
    WebPDemuxReleaseIterator(&webp_iterator);
    if (current_time < next_frame_time_) {
      break;
    }

    current_frame_time_ = next_frame_time_;
    if (next_frame_index_ < frame_count_) {
      next_frame_index_++;
    } else {
      DCHECK_EQ(next_frame_index_, frame_count_);
      // If the WebP image hasn't been fully fetched, or we've reached the end
      // of the last loop, then stop on the current frame.
      if (demux_state_ == WEBP_DEMUX_PARSED_HEADER || loop_count_ == 1) {
        break;
      }
      next_frame_index_ = 1;
      current_frame_index_ = 0;
      if (loop_count_ != kLoopInfinite) {
        loop_count_--;
      }
    }
  }
}

void AnimatedWebPImage::FillImageBufferWithBackgroundColor() {
  uint8_t* image_buffer_memory = image_buffer_->GetMemory();
  for (int y = 0; y < size_.height(); ++y) {
    for (int x = 0; x < size_.width(); ++x) {
      uint32_t* src = reinterpret_cast<uint32_t*>(
          image_buffer_memory + (y * size_.width() + x) * kPixelSize);
      *src = background_color_;
    }
  }
}

AnimatedWebPImage::~AnimatedWebPImage() {
  WebPDemuxDelete(demux_);
  thread_.Stop();
}

scoped_ptr<render_tree::ImageData> AnimatedWebPImage::AllocateImageData() {
  render_tree::PixelFormat pixel_format = render_tree::kPixelFormatRGBA8;
  if (!resource_provider_->PixelFormatSupported(pixel_format)) {
    pixel_format = render_tree::kPixelFormatBGRA8;
  }
  scoped_ptr<render_tree::ImageData> image_data =
      resource_provider_->AllocateImageData(
          size_, pixel_format, render_tree::kAlphaFormatPremultiplied);
  DCHECK(image_data) << "Failed to allocate image.";
  return image_data.Pass();
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
