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
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/rect_node.h"
#include "nb/memory_scope.h"
#include "starboard/memory.h"

namespace cobalt {
namespace loader {
namespace image {
namespace {

const int kLoopInfinite = 0;
const int kMinimumDelayInMilliseconds = 10;

}  // namespace

AnimatedWebPImage::AnimatedWebPImage(
    const math::Size& size, bool is_opaque,
    render_tree::PixelFormat pixel_format,
    render_tree::ResourceProvider* resource_provider)
    : size_(size),
      is_opaque_(is_opaque),
      pixel_format_(pixel_format),
      demux_(NULL),
      demux_state_(WEBP_DEMUX_PARSING_HEADER),
      received_first_frame_(false),
      is_playing_(false),
      frame_count_(0),
      loop_count_(kLoopInfinite),
      current_frame_index_(0),
      next_frame_index_(0),
      should_dispose_previous_frame_to_background_(false),
      resource_provider_(resource_provider) {}

scoped_refptr<render_tree::Image> AnimatedWebPImage::GetFrame() {
  base::AutoLock lock(lock_);
  return current_canvas_;
}

void AnimatedWebPImage::Play(
    const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  base::AutoLock lock(lock_);

  if (is_playing_) {
    return;
  }
  is_playing_ = true;

  message_loop_ = message_loop;
  if (received_first_frame_) {
    PlayInternal();
  }
}

void AnimatedWebPImage::Stop() {
  if (!decode_closure_.callback().is_null()) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&AnimatedWebPImage::StopInternal, base::Unretained(this)));
  }
}

void AnimatedWebPImage::AppendChunk(const uint8* data, size_t input_byte) {
  TRACK_MEMORY_SCOPE("Rendering");
  base::AutoLock lock(lock_);

  data_buffer_.insert(data_buffer_.end(), data, data + input_byte);
  WebPData webp_data = {&data_buffer_[0], data_buffer_.size()};
  WebPDemuxDelete(demux_);
  demux_ = WebPDemuxPartial(&webp_data, &demux_state_);
  DCHECK_GT(demux_state_, WEBP_DEMUX_PARSING_HEADER);

  // Update frame count.
  int new_frame_count = WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT);
  if (new_frame_count > 0 && frame_count_ == 0) {
    // We've just received the first frame.

    received_first_frame_ = true;
    loop_count_ = WebPDemuxGetI(demux_, WEBP_FF_LOOP_COUNT);

    // The default background color of the canvas in [Blue, Green, Red, Alpha]
    // byte order. It is read in little endian order as an 32bit int.
    uint32_t background_color = WebPDemuxGetI(demux_, WEBP_FF_BACKGROUND_COLOR);
    background_color_ =
        render_tree::ColorRGBA((background_color >> 16 & 0xff) / 255.0f,
                               (background_color >> 8 & 0xff) / 255.0f,
                               (background_color & 0xff) / 255.0f,
                               (background_color >> 24 & 0xff) / 255.0f);

    if (is_playing_) {
      PlayInternal();
    }
  }
  frame_count_ = new_frame_count;
}

AnimatedWebPImage::~AnimatedWebPImage() {
  Stop();
  base::AutoLock lock(lock_);
  WebPDemuxDelete(demux_);
}

void AnimatedWebPImage::StopInternal() {
  is_playing_ = false;
  decode_closure_.Cancel();
}

void AnimatedWebPImage::PlayInternal() {
  current_frame_time_ = base::TimeTicks::Now();
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AnimatedWebPImage::DecodeFrames, base::Unretained(this)));
}

void AnimatedWebPImage::DecodeFrames() {
  TRACK_MEMORY_SCOPE("Rendering");
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::DecodeFrames()");
  DCHECK(is_playing_ && received_first_frame_);
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (decode_closure_.callback().is_null()) {
    decode_closure_.Reset(
        base::Bind(&AnimatedWebPImage::DecodeFrames, base::Unretained(this)));
  }

  UpdateTimelineInfo();

  // Decode the frames from current frame to next frame and blend the results.
  for (int frame_index = current_frame_index_ + 1;
       frame_index <= next_frame_index_; ++frame_index) {
    if (!DecodeOneFrame(frame_index)) {
      break;
    }
  }
  current_frame_index_ = next_frame_index_;

  // Set up the next time to call the decode callback.
  base::AutoLock lock(lock_);
  if (is_playing_) {
    base::TimeDelta delay = next_frame_time_ - base::TimeTicks::Now();
    const base::TimeDelta min_delay =
        base::TimeDelta::FromMilliseconds(kMinimumDelayInMilliseconds);
    if (delay < min_delay) {
      delay = min_delay;
    }
    message_loop_->PostDelayedTask(FROM_HERE, decode_closure_.callback(),
                                   delay);
  }
}

bool AnimatedWebPImage::DecodeOneFrame(int frame_index) {
  TRACK_MEMORY_SCOPE("Rendering");
  base::AutoLock lock(lock_);
  WebPIterator webp_iterator;
  WEBPImageDecoder webp_image_decoder(resource_provider_);
  scoped_refptr<render_tree::Image> next_frame_image;

  // Decode the current frame.
  {
    TRACE_EVENT0("cobalt::loader::image", "Decoding");

    WebPDemuxGetFrame(demux_, frame_index, &webp_iterator);
    if (!webp_iterator.complete) {
      return false;
    }
    webp_image_decoder.DecodeChunk(webp_iterator.fragment.bytes,
                                   webp_iterator.fragment.size);
    if (!webp_image_decoder.FinishWithSuccess()) {
      LOG(ERROR) << "Failed to decode WebP image frame.";
      return false;
    }

    scoped_ptr<render_tree::ImageData> next_frame_data = AllocateImageData(
        math::Size(webp_iterator.width, webp_iterator.height));
    DCHECK(next_frame_data);
    {
      TRACE_EVENT0("cobalt::loader::image", "SbMemoryCopy");
      SbMemoryCopy(next_frame_data->GetMemory(),
                   webp_image_decoder.GetOriginalMemory(),
                   webp_iterator.width * webp_iterator.height *
                       render_tree::BytesPerPixel(pixel_format_));
    }
    next_frame_image = resource_provider_->CreateImage(next_frame_data.Pass());
  }

  // Alpha blend the current frame on top of the buffer.
  {
    TRACE_EVENT0("cobalt::loader::image", "Blending");

    render_tree::CompositionNode::Builder builder;
    // Add the current canvas or, if there is not one, a background color
    // rectangle;
    if (current_canvas_) {
      builder.AddChild(new render_tree::ImageNode(current_canvas_));
    } else {
      scoped_ptr<render_tree::Brush> brush(
          new render_tree::SolidColorBrush(background_color_));
      builder.AddChild(
          new render_tree::RectNode(math::RectF(size_), brush.Pass()));
    }
    // Dispose previous frame by adding a solid rectangle.
    if (should_dispose_previous_frame_to_background_) {
      scoped_ptr<render_tree::Brush> brush(
          new render_tree::SolidColorBrush(background_color_));
      builder.AddChild(
          new render_tree::RectNode(previous_frame_rect_, brush.Pass()));
    }
    // Add the current frame.
    builder.AddChild(new render_tree::ImageNode(
        next_frame_image,
        math::Vector2dF(webp_iterator.x_offset, webp_iterator.y_offset)));

    scoped_refptr<render_tree::Node> root =
        new render_tree::CompositionNode(builder);

    current_canvas_ = resource_provider_->DrawOffscreenImage(root);
  }

  if (webp_iterator.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND) {
    should_dispose_previous_frame_to_background_ = true;
    previous_frame_rect_ =
        math::RectF(webp_iterator.x_offset, webp_iterator.y_offset,
                    webp_iterator.width, webp_iterator.height);
  } else if (webp_iterator.dispose_method == WEBP_MUX_DISPOSE_NONE) {
    should_dispose_previous_frame_to_background_ = false;
  } else {
    NOTREACHED();
  }

  WebPDemuxReleaseIterator(&webp_iterator);
  return true;
}

void AnimatedWebPImage::UpdateTimelineInfo() {
  TRACK_MEMORY_SCOPE("Rendering");
  base::AutoLock lock(lock_);
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

scoped_ptr<render_tree::ImageData> AnimatedWebPImage::AllocateImageData(
    const math::Size& size) {
  TRACK_MEMORY_SCOPE("Rendering");
  scoped_ptr<render_tree::ImageData> image_data =
      resource_provider_->AllocateImageData(
          size, pixel_format_, render_tree::kAlphaFormatPremultiplied);
  DCHECK(image_data) << "Failed to allocate image.";
  return image_data.Pass();
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
