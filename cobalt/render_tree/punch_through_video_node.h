// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_PUNCH_THROUGH_VIDEO_NODE_H_
#define COBALT_RENDER_TREE_PUNCH_THROUGH_VIDEO_NODE_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace render_tree {

// Results in a rectangle being rasterized onto the screen where each pixel of
// the rectangle has 0 alpha (but it is not blended with the destination color,
// or in other words, it punches through to the background). This is useful when
// another subsystem is rendering a video on another layer behind the graphics.
//
// WARNING: It is very difficult for implementations to support this
//          properly in every scenario (e.g. if the renderer happens to be
//          rendering to an offscreen surface when this ImageNode is
//          encountered, the punch through will apply to the offscreen
//          surface, which itself may be blended with the final image.)
//          Use this setting sparingly.  As of now it is used only to
//          support punch out video rendering.
class PunchThroughVideoNode : public Node {
 public:
  typedef base::Callback<bool(const math::Rect&)> SetBoundsCB;

  struct Builder {
    Builder(const math::RectF& rect, const SetBoundsCB& set_bounds_cb)
        : rect(rect), set_bounds_cb(set_bounds_cb) {}

    bool operator==(const Builder& other) const {
      return rect == other.rect && set_bounds_cb.Equals(other.set_bounds_cb);
    }

    // The destination rectangle (size includes border).
    math::RectF rect;
    const SetBoundsCB set_bounds_cb;
  };

  explicit PunchThroughVideoNode(const Builder& builder) : data_(builder) {}

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<PunchThroughVideoNode>();
  }

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_PUNCH_THROUGH_VIDEO_NODE_H_
