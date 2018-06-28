// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_BLUR_FILTER_H_
#define COBALT_RENDER_TREE_BLUR_FILTER_H_

#include "base/logging.h"

namespace cobalt {
namespace render_tree {

// An BlurFilter can be used to express the desire to modify a given source
// image to appear blurred.  In particular, it will be convolved with a
// Gaussian kernel that has the specified standard deviation (|blur_sigma|).
// Note that |blur_sigma| must be non-negative.
class BlurFilter {
 public:
  // |blur_sigma| must be non-negative.
  explicit BlurFilter(float blur_sigma) { set_blur_sigma(blur_sigma); }

  bool operator==(const BlurFilter& other) const {
    return blur_sigma_ == other.blur_sigma_;
  }

  void set_blur_sigma(float blur_sigma) {
    DCHECK_LE(0.0f, blur_sigma);
    blur_sigma_ = blur_sigma;
  }
  float blur_sigma() const { return blur_sigma_; }

 private:
  float blur_sigma_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_BLUR_FILTER_H_
