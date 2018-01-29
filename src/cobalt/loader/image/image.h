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

#ifndef COBALT_LOADER_IMAGE_IMAGE_H_
#define COBALT_LOADER_IMAGE_IMAGE_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/image_node.h"

namespace cobalt {
namespace loader {
namespace image {

// This class represents a general image that is stored in memory. It can be
// either a static image or an animated image. It is intended to be cached in
// an ImageCache.
class Image : public base::RefCountedThreadSafe<Image> {
 public:
  virtual const math::Size& GetSize() const = 0;

  virtual uint32 GetEstimatedSizeInBytes() const = 0;

  virtual bool IsOpaque() const = 0;

  virtual bool IsAnimated() const = 0;

 protected:
  virtual ~Image() {}
  friend class base::RefCountedThreadSafe<Image>;
};

// StaticImage is a wrapper around render_tree::Image. The images will be
// decoded immediately after fetching and stored in memory.
class StaticImage : public Image {
 public:
  explicit StaticImage(scoped_refptr<render_tree::Image> image)
      : image_(image) {
    DCHECK(image);
  }

  const math::Size& GetSize() const override { return image_->GetSize(); }

  uint32 GetEstimatedSizeInBytes() const override {
    return image_->GetEstimatedSizeInBytes();
  }

  bool IsAnimated() const override { return false; }

  bool IsOpaque() const override { return image_->IsOpaque(); }

  scoped_refptr<render_tree::Image> image() { return image_; }

 private:
  scoped_refptr<render_tree::Image> image_;
};

// Concrete implementations of AnimatedImage should include mechanisms that
// balance the tradeoff between space usage and efficiency of decoding frames
// when playing animation.
class AnimatedImage : public Image {
 public:
  // FrameProvider nested classes are used as "frame containers".  Typically
  // they will be created by an AnimatedImage and have frames pushed into them
  // by the creating AnimatedImage object.  They can be handed to consumers to
  // have frames pulled out of them.  Its purpose is to allow AnimatedImage
  // objects to be destroyed without affecting consumers of the FrameProvider.
  // If the AnimatedImage is destroyed before the consumer is done pulling
  // frames out of FrameProvider, they will simply be left with the last frame
  // that was in there.
  class FrameProvider : public base::RefCountedThreadSafe<FrameProvider> {
   public:
    FrameProvider() : frame_consumed_(true) {}

    void SetFrame(const scoped_refptr<render_tree::Image>& frame) {
      base::AutoLock lock(mutex_);
      frame_ = frame;
      frame_consumed_ = false;
    }

    bool FrameConsumed() const {
      base::AutoLock lock(mutex_);
      return frame_consumed_;
    }

    scoped_refptr<render_tree::Image> GetFrame() {
      base::AutoLock lock(mutex_);
      frame_consumed_ = true;
      return frame_;
    }

   private:
    virtual ~FrameProvider() {}
    friend class base::RefCountedThreadSafe<FrameProvider>;

    mutable base::Lock mutex_;
    // True if a call to FrameConsumed() has been made after the last call to
    // SetFrame().
    bool frame_consumed_;
    scoped_refptr<render_tree::Image> frame_;
  };

  bool IsAnimated() const override { return true; }

  bool IsOpaque() const override { return false; }

  // Start playing the animation, decoding on the given message loop.
  // Implementation should be thread safe.
  virtual void Play(
      const scoped_refptr<base::MessageLoopProxy>& message_loop) = 0;

  // Stop playing the animation.
  virtual void Stop() = 0;

  // Returns a FrameProvider object from which frames can be pulled out of.
  // The AnimatedImage object is expected to push frames into the FrameProvider
  // as it generates them.
  virtual scoped_refptr<FrameProvider> GetFrameProvider() = 0;

  // This callback is intended to be used in a render_tree::AnimateNode.
  static void AnimateCallback(
      scoped_refptr<FrameProvider> frame_provider,
      const math::RectF& destination_rect,
      const math::Matrix3F& local_transform,
      render_tree::ImageNode::Builder* image_node_builder) {
    image_node_builder->source = frame_provider->GetFrame();
    image_node_builder->destination_rect = destination_rect;
    image_node_builder->local_transform = local_transform;
  }
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_IMAGE_H_
