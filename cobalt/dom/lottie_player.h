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

#ifndef COBALT_DOM_LOTTIE_PLAYER_H_
#define COBALT_DOM_LOTTIE_PLAYER_H_

#include <string>

#include "base/message_loop/message_loop.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/render_tree/lottie_animation.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/union_type.h"

namespace cobalt {
namespace dom {

class Document;

typedef render_tree::LottieAnimation LottieAnimation;

// Custom element that represents a Lottie web player, which embeds and plays
// Lottie animations.
// Although LottiePlayer does not inherit from HTMLImageElement, much of its
// functionality is based off that of HTMLImageElement - in particular, loading
// the animation pointed to by the "src" attribute.
//   https://lottiefiles.github.io/lottie-player/
class LottiePlayer : public HTMLElement {
 public:
  static const char kTagName[];

  typedef script::UnionType2<double, std::string> FrameType;

  explicit LottiePlayer(Document* document);

  // Web API: LottiePlayer
  //
  std::string src() const;
  void set_src(const std::string& src);
  bool autoplay() const;
  void set_autoplay(bool loop);
  std::string background() const;
  void set_background(std::string background);
  int count() const;
  void set_count(int count);
  int direction() const;
  void set_direction(int direction);
  bool hover() const;
  void set_hover(bool hover);
  bool loop() const;
  void set_loop(bool loop);
  std::string mode() const;
  void set_mode(std::string mode);
  double speed() const;
  void set_speed(double speed);
  std::string preserve_aspect_ratio() const;
  std::string renderer() const;
  void Load(std::string src);
  void Play();
  void Pause();
  void Stop();
  void Seek(FrameType frame);
  void SetDirection(int direction);
  void SetLooping(bool loop);
  void SetSpeed(double speed);
  void ToggleLooping();
  void TogglePlay();

  // Custom, not in any spec
  //
  // From HTMLElement
  scoped_refptr<LottiePlayer> AsLottiePlayer() override { return this; }

  const scoped_refptr<loader::image::CachedImage>& cached_image() {
    return cached_image_;
  }

  LottieAnimation::LottieProperties GetProperties() const;

  // These functions will be called when there is a hover change for the
  // element.
  void OnHover();
  void OnUnHover();

  DEFINE_WRAPPABLE_TYPE(LottiePlayer);

 private:
  ~LottiePlayer() override {}

  // From Node.
  void PurgeCachedBackgroundImagesOfNodeAndDescendants() override;

  // From Element.
  void OnSetAttribute(const std::string& name,
                      const std::string& value) override;
  void OnRemoveAttribute(const std::string& name) override;

  void UpdateAnimationData();

  void OnLoadingSuccess();
  void OnLoadingError();

  void PreventGarbageCollectionUntilEventIsDispatched(base::Token event_name);
  void AllowGarbageCollectionAfterEventIsDispatched(
      base::Token event_name,
      std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
          scoped_prevent_gc);
  void DestroyScopedPreventGC(
      std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
          scoped_prevent_gc);

  void UpdateState(LottieAnimation::LottieState state);
  void UpdatePlaybackStateIfAutoplaying();
  void SetCount(int count);
  void SetMode(std::string mode);
  void SetMode(LottieAnimation::LottieMode mode);
  void UpdateLottieObjects();

  void ScheduleEvent(base::Token event_name);
  void SetAnimationEventCallbacks();

  // These are callbacks triggered during animation playback.
  void OnPlay();
  void OnPause();
  void OnStop();
  void OnComplete();
  void OnLoop();
  void OnEnterFrame(double frame, double seeker);
  static void CallOnEnterFrame(
      scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
      base::Callback<void(double, double)> enter_frame_callback, double frame,
      double seeker);
  void OnFreeze();
  void OnUnfreeze();

  scoped_refptr<loader::image::CachedImage> cached_image_;
  std::unique_ptr<loader::image::CachedImage::OnLoadedCallbackHandler>
      cached_image_loaded_callback_handler_;

  std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
      prevent_gc_until_load_complete_;

  // Indicates whether playback is dictated by the "autoplay" attributes, or
  // if other playback methods have been called.
  bool autoplaying_;
  LottieAnimation::LottieProperties properties_;

  EventQueue event_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_LOTTIE_PLAYER_H_
