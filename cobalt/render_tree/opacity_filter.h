// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_OPACITY_FILTER_H_
#define COBALT_RENDER_TREE_OPACITY_FILTER_H_

#include "base/logging.h"

namespace cobalt {
namespace render_tree {

// An OpacityFilter can be used to express the desire to modify a given source
// image to become transparent.  Opacity should be a value between [0, 1] where
// 0 is completely invisible and 1 is fully opaque.
class OpacityFilter {
 public:
  explicit OpacityFilter(float opacity) { set_opacity(opacity); }

  bool operator==(const OpacityFilter& other) const {
    return opacity_ == other.opacity_;
  }

  void set_opacity(float opacity) {
    DCHECK_LE(0.0f, opacity);
    DCHECK_GE(1.0f, opacity);
    opacity_ = opacity;
  }
  float opacity() const { return opacity_; }

 private:
  float opacity_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_OPACITY_FILTER_H_
