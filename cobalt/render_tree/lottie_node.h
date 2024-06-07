// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_LOTTIE_NODE_H_
#define COBALT_RENDER_TREE_LOTTIE_NODE_H_

#include "base/compiler_specific.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/lottie_animation.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace render_tree {

// A Lottie animation.
class LottieNode : public Node {
 public:
  struct Builder {
    Builder(const Builder&) = default;

    Builder(const scoped_refptr<LottieAnimation>& animation,
            const math::RectF destination_rect);

    bool operator==(const Builder& other) const {
      return animation == other.animation &&
             destination_rect == other.destination_rect &&
             animation_time == other.animation_time;
    }

    // Information about the contents of the original animation file.
    scoped_refptr<LottieAnimation> animation;

    // The destination rectangle into which the animation will be rasterized.
    math::RectF destination_rect;

    // The time that the animation should currently be playing at.
    base::TimeDelta animation_time;
  };

  // Forwarding constructor to the set of Builder constructors.
  template <typename... Args>
  LottieNode(Args&&... args) : data_(std::forward<Args>(args)...) {}

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<LottieNode>();
  }

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_LOTTIE_NODE_H_
