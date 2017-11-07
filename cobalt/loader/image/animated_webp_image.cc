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

#include <string>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/loader/image/image_decoder.h"
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
    render_tree::ResourceProvider* resource_provider)
    : size_(size),
      is_opaque_(is_opaque),
      demux_(NULL),
      demux_state_(WEBP_DEMUX_PARSING_HEADER),
      received_first_frame_(false),
      is_playing_(false),
      frame_count_(0),
      loop_count_(kLoopInfinite),
      current_frame_index_(0),
      should_dispose_previous_frame_to_background_(false),
      resource_provider_(resource_provider),
      frame_provider_(new FrameProvider()) {
  TRACE_EVENT0("cobalt::loader::image",
               "AnimatedWebPImage::AnimatedWebPImage()");
}

scoped_refptr<AnimatedImage::FrameProvider>
AnimatedWebPImage::GetFrameProvider() {
  TRACE_EVENT0("cobalt::loader::image",
               "AnimatedWebPImage::GetFrameProvider()");
  return frame_provider_;
}

void AnimatedWebPImage::Play(
    const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::Play()");
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
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::Stop()");
  base::AutoLock lock(lock_);
  if (is_playing_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&AnimatedWebPImage::StopInternal, base::Unretained(this)));
  }
}

void AnimatedWebPImage::AppendChunk(const uint8* data, size_t size) {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::AppendChunk()");
  TRACK_MEMORY_SCOPE("Rendering");
  base::AutoLock lock(lock_);

  data_buffer_.insert(data_buffer_.end(), data, data + size);
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

    // The default background color of the canvas in [Blue, Green, Red, Alpha],
    // from most significant byte to least significant byte.
    uint32_t background_color = WebPDemuxGetI(demux_, WEBP_FF_BACKGROUND_COLOR);
    background_color_ =
        render_tree::ColorRGBA((background_color >> 8 & 0xff) / 255.0f,
                               (background_color >> 16 & 0xff) / 255.0f,
                               (background_color >> 24 & 0xff) / 255.0f,
                               (background_color >> 0 & 0xff) / 255.0f);

    if (is_playing_) {
      PlayInternal();
    }
  }
  frame_count_ = new_frame_count;
}

AnimatedWebPImage::~AnimatedWebPImage() {
  TRACE_EVENT0("cobalt::loader::image",
               "AnimatedWebPImage::~AnimatedWebPImage()");
  Stop();
  bool is_playing = false;
  {
    base::AutoLock lock(lock_);
    is_playing = is_playing_;
  }
  if (is_playing) {
    message_loop_->WaitForFence();
  }
  WebPDemuxDelete(demux_);
}

void AnimatedWebPImage::StopInternal() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::StopInternal()");
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!decode_closure_.callback().is_null()) {
    is_playing_ = false;
    decode_closure_.Cancel();
  }
}

void AnimatedWebPImage::PlayInternal() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::PlayInternal()");
  current_frame_time_ = base::TimeTicks::Now();
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AnimatedWebPImage::DecodeFrames, base::Unretained(this)));
}

void AnimatedWebPImage::DecodeFrames() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::DecodeFrames()");
  TRACK_MEMORY_SCOPE("Rendering");
  DCHECK(is_playing_ && received_first_frame_);
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock lock(lock_);

  if (decode_closure_.callback().is_null()) {
    decode_closure_.Reset(
        base::Bind(&AnimatedWebPImage::DecodeFrames, base::Unretained(this)));
  }

  if (AdvanceFrame()) {
    // Decode the frames from current frame to next frame and blend the results.
    DecodeOneFrame(current_frame_index_);
  }

  // Set up the next time to call the decode callback.
  if (is_playing_) {
    const base::TimeDelta min_delay =
        base::TimeDelta::FromMilliseconds(kMinimumDelayInMilliseconds);
    base::TimeDelta delay;
    if (next_frame_time_) {
      delay = *next_frame_time_ - base::TimeTicks::Now();
      if (delay < min_delay) {
        delay = min_delay;
      }
    } else {
      delay = min_delay;
    }

    message_loop_->PostDelayedTask(FROM_HERE, decode_closure_.callback(),
                                   delay);
  }
}

namespace {

void RecordImage(scoped_refptr<render_tree::Image>* image_pointer,
                 const scoped_refptr<loader::image::Image>& image) {
  image::StaticImage* static_image =
      base::polymorphic_downcast<loader::image::StaticImage*>(image.get());
  DCHECK(static_image);
  *image_pointer = static_image->image();
}

void DecodeError(const std::string& error) {
  LOG(ERROR) << error;
}

}  // namespace

bool AnimatedWebPImage::DecodeOneFrame(int frame_index) {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::DecodeOneFrame()");
  TRACK_MEMORY_SCOPE("Rendering");
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  WebPIterator webp_iterator;
  scoped_refptr<render_tree::Image> next_frame_image;

  // Decode the current frame.
  {
    TRACE_EVENT0("cobalt::loader::image", "Decoding");

    WebPDemuxGetFrame(demux_, frame_index, &webp_iterator);
    if (!webp_iterator.complete) {
      return false;
    }

    ImageDecoder image_decoder(
        resource_provider_, base::Bind(&RecordImage, &next_frame_image),
        base::Bind(&DecodeError), ImageDecoder::kImageTypeWebP);
    image_decoder.DecodeChunk(
        reinterpret_cast<const char*>(webp_iterator.fragment.bytes),
        webp_iterator.fragment.size);
    image_decoder.Finish();
    if (!next_frame_image) {
      LOG(ERROR) << "Failed to decode WebP image frame.";
      return false;
    }
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
    frame_provider_->SetFrame(current_canvas_);
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

bool AnimatedWebPImage::AdvanceFrame() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::AdvanceFrame()");
  TRACK_MEMORY_SCOPE("Rendering");
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  base::TimeTicks current_time = base::TimeTicks::Now();

  // If the WebP image hasn't been fully fetched, then stop on the current
  // frame.
  if (demux_state_ == WEBP_DEMUX_PARSED_HEADER) {
    return false;
  }

  // If we're done playing the animation, do nothing.
  if (LoopingFinished()) {
    return false;
  }

  // If it's still not time to advance to the next frame, do nothing.
  if (next_frame_time_ && current_time < *next_frame_time_) {
    return false;
  }

  // Always wait for a consumer to consume the previous frame before moving
  // forward with decoding the next frame.
  if (!frame_provider_->FrameConsumed()) {
    return false;
  }

  if (next_frame_time_) {
    current_frame_time_ = *next_frame_time_;
  } else {
    current_frame_time_ = current_time;
  }

  ++current_frame_index_;
  if (current_frame_index_ == frame_count_) {
    // Check if we have finished looping, and if so return indicating that there
    // is no additional frame available.
    if (LoopingFinished()) {
      next_frame_time_ = base::nullopt;
      return false;
    }

    // Loop around to the beginning
    current_frame_index_ = 0;
    if (loop_count_ != kLoopInfinite) {
      loop_count_--;
    }
  }

  // Update the time in the future at which point we should switch to the
  // frame after the new current frame.
  next_frame_time_ =
      current_frame_time_ + GetFrameDuration(current_frame_index_);
  if (next_frame_time_ < current_time) {
    // Don't let the animation fall back for more than a frame.
    next_frame_time_ = current_time;
  }

  return true;
}

base::TimeDelta AnimatedWebPImage::GetFrameDuration(int frame_index) {
  lock_.AssertAcquired();
  WebPIterator webp_iterator;
  WebPDemuxGetFrame(demux_, frame_index, &webp_iterator);
  base::TimeDelta frame_duration =
      base::TimeDelta::FromMilliseconds(webp_iterator.duration);
  WebPDemuxReleaseIterator(&webp_iterator);
  return frame_duration;
}

bool AnimatedWebPImage::LoopingFinished() const {
  return loop_count_ == 1 && current_frame_index_ == frame_count_;
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
