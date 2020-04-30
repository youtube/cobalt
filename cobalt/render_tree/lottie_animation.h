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

#ifndef COBALT_RENDER_TREE_LOTTIE_ANIMATION_H_
#define COBALT_RENDER_TREE_LOTTIE_ANIMATION_H_

#include "base/time/time.h"
#include "cobalt/render_tree/image.h"

namespace cobalt {
namespace render_tree {

// The LottieAnimation type is an abstract base class that represents a stored
// Lottie animation. When constructing a render tree, external Lottie animations
// can be introduced by adding an LottieNode and associating it with a specific
// LottieAnimation object.
class LottieAnimation : public Image {
 public:
  struct LottieProperties {
    LottieProperties() = default;

    explicit LottieProperties(bool loop) : loop(loop) {}

    bool loop;
  };

  virtual void SetProperties(LottieProperties properties) = 0;

  virtual void SetAnimationTime(base::TimeDelta animation_time) = 0;

 protected:
  virtual ~LottieAnimation() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<LottieAnimation>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_LOTTIE_ANIMATION_H_
