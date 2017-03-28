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

  const math::Size& GetSize() const OVERRIDE { return image_->GetSize(); }

  uint32 GetEstimatedSizeInBytes() const OVERRIDE {
    return image_->GetEstimatedSizeInBytes();
  }

  bool IsAnimated() const OVERRIDE { return false; }

  bool IsOpaque() const OVERRIDE { return image_->IsOpaque(); }

  scoped_refptr<render_tree::Image> image() { return image_; }

 private:
  scoped_refptr<render_tree::Image> image_;
};

// Concrete implementations of AnimatedImage should include mechanisms that
// balance the tradeoff between space usage and efficiency of decoding frames
// when playing animation.
class AnimatedImage : public Image {
 public:
  bool IsAnimated() const OVERRIDE { return true; }

  bool IsOpaque() const OVERRIDE { return false; }

  // Get the current frame. Implementation should be thread safe.
  virtual scoped_refptr<render_tree::Image> GetFrame() = 0;

  // Start playing the animation. Implementation should be thread safe.
  virtual void Play() = 0;

  // This callback is intended to be used in a render_tree::AnimateNode.
  void AnimateCallback(const math::RectF& destination_rect,
                       const math::Matrix3F& local_transform,
                       render_tree::ImageNode::Builder* image_node_builder,
                       base::TimeDelta time) {
    UNREFERENCED_PARAMETER(time);

    image_node_builder->source = GetFrame();
    image_node_builder->destination_rect = destination_rect;
    image_node_builder->local_transform = local_transform;
  }
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_IMAGE_H_
