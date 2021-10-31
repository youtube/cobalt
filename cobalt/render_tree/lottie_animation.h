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

#include <string>

#include "base/callback.h"
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
  enum class LottieState { kPlaying, kPaused, kStopped, kFrozen };

  enum class LottieMode { kNormal, kBounce };

  // https://lottiefiles.github.io/lottie-player/properties.html
  struct LottieProperties {
    // Since an explicitly specified value of |count| must be greater than 0,
    // a default value of -1 indicates that there is no limit to the number of
    // times the animation should loop.
    static constexpr int kDefaultCount = -1;
    static constexpr int kDefaultDirection = 1;
    static constexpr bool kDefaultLoop = false;
    static constexpr LottieMode kDefaultMode = LottieMode::kNormal;
    static constexpr double kDefaultSpeed = 1;

    LottieProperties() = default;

    // Return true if |state| is updated to a new & valid LottieState.
    bool UpdateState(const LottieState& new_state) {
      // Regardless of whether the state actually changes, per the web spec, we
      // need to dispatch an event signaling that a particular playback state
      // was requested.
      if (new_state == LottieState::kPlaying && !onplay_callback.is_null()) {
        onplay_callback.Run();
      } else if (new_state == LottieState::kPaused &&
                 !onpause_callback.is_null()) {
        onpause_callback.Run();
      } else if (new_state == LottieState::kStopped &&
                 !onstop_callback.is_null()) {
        onstop_callback.Run();
      }

      if (new_state == state) {
        return false;
      }
      state = new_state;
      return true;
    }

    // Return true if we can freeze the animation, i.e. the animation is
    // currently playing.
    bool FreezeAnimationState() {
      if (state == LottieState::kPlaying) {
        state = LottieState::kFrozen;
        return true;
      }
      return false;
    }

    // Return true if we can unfreeze the animation, i.e. the animation is
    // currently frozen.
    bool UnfreezeAnimationState() {
      if (state == LottieState::kFrozen) {
        state = LottieState::kPlaying;
        return true;
      }
      return false;
    }

    // Return true if |count| is updated to a new & valid number.
    bool UpdateCount(int new_count) {
      // |count| must be positive.
      if (new_count <= 0 || new_count == count) {
        return false;
      }
      count = new_count;
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

    // Return the string equivalent of |mode|.
    std::string GetModeAsString() const {
      if (mode == LottieMode::kBounce) {
        return "bounce";
      }
      // Always fallback to the default.
      return "normal";
    }

    // Return true if |mode| is updated to a new & valid mode.
    bool UpdateMode(std::string new_mode) {
      if (new_mode == "normal") {
        return UpdateMode(LottieMode::kNormal);
      } else if (new_mode == "bounce") {
        return UpdateMode(LottieMode::kBounce);
      }
      return false;
    }

    // Return true if |mode| is updated.
    bool UpdateMode(LottieMode new_mode) {
      if (new_mode == mode) {
        return false;
      }
      mode = new_mode;
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

    void ToggleLooping() { loop = !loop; }

    void TogglePlay() {
      if (state == LottieState::kPlaying) {
        UpdateState(LottieState::kPaused);
      } else {
        UpdateState(LottieState::kPlaying);
      }
    }

    // |state| indicates whether the animation is playing (visible and
    // animating), paused (visible but not animating), or stopped (not visible
    // and frame time = 0).
    LottieState state = LottieState::kStopped;

    int count = kDefaultCount;
    int direction = kDefaultDirection;
    bool loop = kDefaultLoop;
    LottieMode mode = kDefaultMode;
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

    base::Closure onplay_callback;
    base::Closure onpause_callback;
    base::Closure onstop_callback;
    base::Closure oncomplete_callback;
    base::Closure onloop_callback;
    base::Callback<void(double, double)> onenterframe_callback;
    base::Closure onfreeze_callback;
    base::Closure onunfreeze_callback;
  };

  void BeginRenderFrame(const LottieProperties& properties) {
    // Trigger callback if playback state changed.
    if (played_this_frame_ && properties.state == LottieState::kFrozen &&
        !properties_.onunfreeze_callback.is_null()) {
      properties_.onunfreeze_callback.Run();
    }
    if (!played_this_frame_ && properties.state == LottieState::kPlaying &&
        !properties_.onfreeze_callback.is_null()) {
      properties_.onfreeze_callback.Run();
    }
    played_this_frame_ = false;
    properties_ = properties;
  }

  void SetAnimationTime(base::TimeDelta animate_function_time) {
    played_this_frame_ = true;
    SetAnimationTimeInternal(animate_function_time);
  }

 protected:
  virtual ~LottieAnimation() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<LottieAnimation>;

  virtual void SetAnimationTimeInternal(
      base::TimeDelta animate_function_time) = 0;

  LottieProperties properties_;

 private:
  // A flag that will be set to true only if the animation is visible onscreen
  // and is rendered.
  bool played_this_frame_ = false;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_LOTTIE_ANIMATION_H_
