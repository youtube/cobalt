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

#ifndef COBALT_LOADER_IMAGE_ANIMATED_WEBP_IMAGE_H_
#define COBALT_LOADER_IMAGE_ANIMATED_WEBP_IMAGE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/resource_provider.h"
#include "third_party/libwebp/webp/demux.h"

namespace cobalt {
namespace loader {
namespace image {

class AnimatedWebPImage : public AnimatedImage {
 public:
  AnimatedWebPImage(const math::Size& size, bool is_opaque,
                    render_tree::ResourceProvider* resource_provider);

  const math::Size& GetSize() const OVERRIDE { return size_; }

  uint32 GetEstimatedSizeInBytes() const OVERRIDE {
    // Return the size of 2 frames of images, since we can have two frames in
    // memory at a time (the previous decode image passed to the frame provider
    // and the next frame that is composed from the previous frame).
    return size_.GetArea() * 4 * 2 + static_cast<uint32>(data_buffer_.size());
  }

  bool IsOpaque() const OVERRIDE { return is_opaque_; }

  scoped_refptr<FrameProvider> GetFrameProvider() OVERRIDE;

  void Play(const scoped_refptr<base::MessageLoopProxy>& message_loop) OVERRIDE;

  void Stop() OVERRIDE;

  void AppendChunk(const uint8* data, size_t input_byte);

 private:
  ~AnimatedWebPImage() OVERRIDE;

  // To be called the decoding thread, to cancel future decodings.
  void StopInternal();

  void PlayInternal();

  // Decodes all frames until current time.
  void DecodeFrames();

  // Decodes the frame with the given index, returns if it succeeded.
  bool DecodeOneFrame(int frame_index);

  // If the time is right, updates the index and time info of the current frame.
  bool AdvanceFrame();

  // Returns the duration of the given frame index.
  base::TimeDelta GetFrameDuration(int frame_index);

  // Returns true if the animation loop is finished.
  bool LoopingFinished() const;

  const math::Size size_;
  const bool is_opaque_;
  WebPDemuxer* demux_;
  WebPDemuxState demux_state_;
  bool received_first_frame_;
  bool is_playing_;
  int frame_count_;
  // The remaining number of times to loop the animation. kLoopInfinite means
  // looping infinitely.
  int loop_count_;
  int current_frame_index_;
  bool should_dispose_previous_frame_to_background_;
  render_tree::ResourceProvider* resource_provider_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  render_tree::ColorRGBA background_color_;
  math::RectF previous_frame_rect_;
  base::CancelableClosure decode_closure_;
  base::TimeTicks current_frame_time_;
  base::optional<base::TimeTicks> next_frame_time_;
  // The original encoded data.
  std::vector<uint8> data_buffer_;
  scoped_refptr<render_tree::Image> current_canvas_;
  scoped_refptr<FrameProvider> frame_provider_;
  base::Lock lock_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_ANIMATED_WEBP_IMAGE_H_
