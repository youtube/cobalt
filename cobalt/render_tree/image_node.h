/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef RENDER_TREE_IMAGE_NODE_H_
#define RENDER_TREE_IMAGE_NODE_H_

#include "base/compiler_specific.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// An image that supports scaling and tiling.
class ImageNode : public Node {
 public:
  // If no width/height are specified, the native width and height of the
  // image will be selected and used as the image node's width and height.
  explicit ImageNode(const scoped_refptr<Image>& image);

  // The specified image will render with the given width and height, which
  // may result in scaling.
  ImageNode(const scoped_refptr<Image>& image,
            const math::SizeF& destination_size);

  // A type-safe branching.
  void Accept(NodeVisitor* visitor) OVERRIDE;

  // A source of pixels. May be smaller or larger than layed out image.
  // The class does not own the image, it merely refers it from a resource pool.
  scoped_refptr<Image> source() const { return image_; }

  // A part of the source image that has to be scaled or tiled.
  const math::RectF& source_rect() const;

  // Returns the width and height that the image will be rasterized as.
  const math::SizeF& destination_size() const { return destination_size_; }

  // A horizontal and vertical tile factor.
  // Tile factor is a number between 0 and 1 which defines how much smaller
  // the tile is than the destination image. If the tile factor is 1, no tiling
  // is requested in a given dimension. If the tile factor is 0.5, the source
  // image has to be repeated twice in a given dimension, and so on.
  float tile_factor_x() const;
  float tile_factor_y() const;

 private:
  scoped_refptr<Image> image_;

  math::SizeF destination_size_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_IMAGE_NODE_H_
