/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include "cobalt/render_tree/clear_rect_node.h"
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
    render_tree::ResourceProvider* resource_provider,
    const base::DebuggerHooks& debugger_hooks)
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
      debugger_hooks_(debugger_hooks),
      frame_provider_(new FrameProvider()) {
  TRACE_EVENT0("cobalt::loader::image",
               "AnimatedWebPImage::AnimatedWebPImage()");
  DETACH_FROM_THREAD(task_runner_thread_checker_);
}

scoped_refptr<AnimatedImage::FrameProvider>
AnimatedWebPImage::GetFrameProvider() {
  TRACE_EVENT0("cobalt::loader::image",
               "AnimatedWebPImage::GetFrameProvider()");
  return frame_provider_;
}

void AnimatedWebPImage::Play(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::Play()");
  // This function may be called from a thread that is not the task runner
  // thread, but it must be consistent (and consistent with Stop() also).
  // This ensures that it's safe to set |task_runner_| without holding a lock.
  DCHECK_CALLED_ON_VALID_THREAD(task_runner_thread_checker_);
  if (!task_runner_) {
    task_runner_ = task_runner;
  } else {
    DCHECK_EQ(task_runner_, task_runner);
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(&AnimatedWebPImage::PlayInternal,
                                               base::Unretained(this)));
}

void AnimatedWebPImage::Stop() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::Stop()");
  // This function may be called from a thread that is not the task runner
  // thread, but it must be consistent (and consistent with Play() also).
  DCHECK_CALLED_ON_VALID_THREAD(task_runner_thread_checker_);
  if (!task_runner_) {
    // The image has not started playing yet.
    return;
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(&AnimatedWebPImage::StopInternal,
                                               base::Unretained(this)));
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

    // The background color for webp is treated as transparent white,
    // thus pulling in whatever color the document uses.
    background_color_ = render_tree::ColorRGBA(0, 0, 0, 0);

    if (is_playing_) {
      StartDecoding();
    }
  }
  frame_count_ = new_frame_count;
}

AnimatedWebPImage::~AnimatedWebPImage() {
  TRACE_EVENT0("cobalt::loader::image",
               "AnimatedWebPImage::~AnimatedWebPImage()");
  if (task_runner_) {
    Stop();
    task_runner_->WaitForFence();
  }

  WebPDemuxDelete(demux_);
}

void AnimatedWebPImage::PlayInternal() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::PlayInternal()");
  base::AutoLock lock(lock_);

  if (is_playing_) {
    return;
  }
  is_playing_ = true;

  if (received_first_frame_) {
    StartDecoding();
  }
}

void AnimatedWebPImage::StopInternal() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::StopInternal()");
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!is_playing_) {
    return;
  }

  if (!decode_closure_.callback().is_null()) {
    is_playing_ = false;
    decode_closure_.Cancel();
  }
}

void AnimatedWebPImage::StartDecoding() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::StartDecoding()");
  lock_.AssertAcquired();
  current_frame_time_ = base::TimeTicks::Now();
  if (task_runner_->BelongsToCurrentThread()) {
    DecodeFrames();
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&AnimatedWebPImage::LockAndDecodeFrames,
                                      base::Unretained(this)));
  }
}

void AnimatedWebPImage::LockAndDecodeFrames() {
  TRACE_EVENT0("cobalt::loader::image",
               "AnimatedWebPImage::LockAndDecodeFrames()");
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(lock_);
  DecodeFrames();
}

void AnimatedWebPImage::DecodeFrames() {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::DecodeFrames()");
  TRACK_MEMORY_SCOPE("Rendering");
  lock_.AssertAcquired();
  DCHECK(is_playing_ && received_first_frame_);
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (decode_closure_.callback().is_null()) {
    decode_closure_.Reset(base::Bind(&AnimatedWebPImage::LockAndDecodeFrames,
                                     base::Unretained(this)));
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

    task_runner_->PostDelayedTask(FROM_HERE, decode_closure_.callback(), delay);
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

void DecodeError(const base::Optional<std::string>& error) {
  if (error) LOG(ERROR) << *error;
}
}  // namespace

bool AnimatedWebPImage::DecodeOneFrame(int frame_index) {
  TRACE_EVENT0("cobalt::loader::image", "AnimatedWebPImage::DecodeOneFrame()");
  TRACK_MEMORY_SCOPE("Rendering");
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

    ImageDecoder image_decoder(resource_provider_, debugger_hooks_,
                               base::Bind(&RecordImage, &next_frame_image),
                               ImageDecoder::kImageTypeWebP,
                               base::Bind(&DecodeError));
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
      builder.AddChild(new render_tree::ClearRectNode(math::RectF(size_),
                                                      background_color_));
    }
    // Dispose previous frame by adding a solid rectangle.
    if (should_dispose_previous_frame_to_background_) {
      builder.AddChild(new render_tree::ClearRectNode(previous_frame_rect_,
                                                      background_color_));
    }

    // Add the current frame.
    if (webp_iterator.blend_method == WEBP_MUX_NO_BLEND) {
      // If blending is disabled, first clear the image region to transparent
      // before rendering.
      builder.AddChild(new render_tree::ClearRectNode(
          math::RectF(
              math::PointF(webp_iterator.x_offset, webp_iterator.y_offset),
              next_frame_image->GetSize()),
          render_tree::ColorRGBA(0, 0, 0, 0)));
    }

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
  DCHECK(task_runner_->BelongsToCurrentThread());
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

scoped_refptr<render_tree::Image> AnimatedWebPImage::GetFrameForDebugging(
    int target_frame) {
  TRACE_EVENT0("cobalt::loader::image",
               "AnimatedWebPImage::GetFrameForDebugging()");

  base::AutoLock lock(lock_);
  DCHECK(!should_dispose_previous_frame_to_background_ && !current_canvas_);
  DCHECK(current_frame_index_ == 0 && !is_playing_);

  if (target_frame <= 0 || target_frame > frame_count_) {
    LOG(WARNING) << "Invalid frame index: " << target_frame;
    return nullptr;
  }

  for (int frame = 1; frame <= target_frame; ++frame) {
    DecodeOneFrame(frame);
  }

  // Reset states when GetFrameForDebugging finishes
  should_dispose_previous_frame_to_background_ = false;
  scoped_refptr<render_tree::Image> target_canvas = current_canvas_;
  current_canvas_ = nullptr;

  return target_canvas;
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
