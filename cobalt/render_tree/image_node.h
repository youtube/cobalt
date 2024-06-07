// Copyright 2014 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_RENDER_TREE_IMAGE_NODE_H_
#define COBALT_RENDER_TREE_IMAGE_NODE_H_

#include "base/compiler_specific.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace render_tree {

// An image that supports scaling and tiling.
class ImageNode : public Node {
 public:
  struct Builder {
    Builder(const Builder&) = default;

    // If no width/height are specified, the native width and height of the
    // image will be selected and used as the image node's width and height.
    explicit Builder(const scoped_refptr<Image>& source);
    // The specified image will render with the given width and height, which
    // may result in scaling.
    Builder(const scoped_refptr<Image>& source,
            const math::RectF& destination_rect);
    // Positions the image using the unscaled source image dimensions, along
    // with a translation offset.
    Builder(const scoped_refptr<Image>& source, const math::Vector2dF& offset);
    // Allows users to additionally supply a local matrix to be applied to the
    // normalized image coordinates.
    Builder(const scoped_refptr<Image>& source,
            const math::RectF& destination_rect,
            const math::Matrix3F& local_transform);

    bool operator==(const Builder& other) const {
      return source == other.source &&
             destination_rect == other.destination_rect &&
             local_transform == other.local_transform;
    }

    // A source of pixels. May be smaller or larger than layed out image.
    // The class does not own the image, it merely refers it from a resource
    // pool.
    scoped_refptr<Image> source;

    // The destination rectangle into which the image will be rasterized.
    math::RectF destination_rect;

    // A matrix expressing how the each point within the image box (defined
    // by destination_rect) should be mapped to image data.  The identity
    // matrix would map the entire source image rectangle into the entire
    // destination rectangle.  As an example, if you were to pass in a scale
    // matrix that scales the image coordinates by 0.5 in all directions, the
    // image will appear zoomed out.
    math::Matrix3F local_transform = math::Matrix3F::Identity();
  };

  // Forwarding constructor to the set of Builder constructors.
  template <typename... Args>
  ImageNode(Args&&... args) : data_(std::forward<Args>(args)...) {}

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<ImageNode>();
  }

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_IMAGE_NODE_H_
