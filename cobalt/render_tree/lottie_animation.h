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
  enum class LottieState { kPlaying, kPaused, kStopped };

  // https://lottiefiles.github.io/lottie-player/properties.html
  struct LottieProperties {
    static constexpr int kDefaultDirection = 1;
    static constexpr bool kDefaultLoop = false;
    static constexpr double kDefaultSpeed = 1;

    LottieProperties() = default;

    // Return true if |state| is updated to a new & valid LottieState.
    bool UpdateState(LottieState new_state) {
      // It is not possible to pause a stopped animation.
      if (new_state == LottieState::kPaused && state == LottieState::kStopped) {
        return false;
      }
      if (new_state == state) {
        return false;
      }
      state = new_state;
      return true;
    }

    // Return true if |direction| is updated to a new & valid direction.
    bool UpdateDirection(int new_direction) {
      // |direction| can either be 1 or -1.
      if ((new_direction != 1 && new_direction != -1) ||
          (new_direction == direction)) {
        return false;
      }
      direction = new_direction;
      return true;
    }

    // Return true if |loop| is updated.
    bool UpdateLoop(bool new_loop) {
      if (new_loop == loop) {
        return false;
      }
      loop = new_loop;
      return true;
    }

    // Return true is |speed| is updated to a new & valid speed.
    bool UpdateSpeed(double new_speed) {
      // |speed| must be a nonnegative value.
      if (new_speed < 0 || new_speed == speed) {
        return false;
      }
      speed = new_speed;
      return true;
    }

    void SeekFrame(double frame_number) {
      seek_value = frame_number;
      seek_value_is_frame = true;
      UpdateStateAndSeekCounter();
    }

    void SeekPercent(double percent) {
      // Store as normalized [0..1] frame selector.
      seek_value = percent / 100;
      seek_value_is_frame = false;
      UpdateStateAndSeekCounter();
    }

    void UpdateStateAndSeekCounter() {
      // A stopped animation will become paused (i.e. will not be hidden)
      // if seek() has been called.
      if (state == LottieState::kStopped) {
        state = LottieState::kPaused;
      }
      // Update |seek_counter| to indicate that a seek needs to occur.
      ++seek_counter;
    }

    // |state| indicates whether the animation is playing (visible and
    // animating), paused (visible but not animating), or stopped (not visible
    // and frame time = 0).
    LottieState state = LottieState::kStopped;

    int direction = kDefaultDirection;
    bool loop = kDefaultLoop;
    double speed = kDefaultSpeed;

    // |seek_value| indicates either the frame or the normalized [0..1] frame
    // selector that the animation should seek to. The internal implementation
    // that we use for the Lottie animations will handle out-of-bounds values,
    // so there is no need to check here.
    double seek_value;
    // |seek_value_is_frame| indicates whether |seek_value| is a frame (true)
    // or a normalized [0..1] frame selector (false).
    bool seek_value_is_frame;
    // |seek_counter| is incremented every time "seek()" is called on a Lottie
    // animation.
    size_t seek_counter = 0;
  };

  virtual void SetProperties(LottieProperties properties) = 0;

  virtual void SetAnimationTime(base::TimeDelta animate_function_time) = 0;

 protected:
  virtual ~LottieAnimation() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<LottieAnimation>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_LOTTIE_ANIMATION_H_
