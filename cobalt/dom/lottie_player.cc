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

#include "cobalt/dom/lottie_player.h"

#include <memory>
#include <string>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/global_environment.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

using render_tree::LottieAnimation;

const char LottiePlayer::kTagName[] = "lottie-player";

LottiePlayer::LottiePlayer(Document* document)
    : HTMLElement(document, base::Token(kTagName)), autoplaying_(true) {}

std::string LottiePlayer::src() const {
  return GetAttribute("src").value_or("");
}

void LottiePlayer::set_src(const std::string& src) { SetAttribute("src", src); }

bool LottiePlayer::autoplay() const { return GetBooleanAttribute("autoplay"); }

void LottiePlayer::set_autoplay(bool autoplay) {
  // The value of 'autoplay' is true when the 'autoplay' attribute is present.
  // The value of the attribute is irrelevant.
  if (autoplay) {
    SetBooleanAttribute("autoplay", true);
  } else {
    SetBooleanAttribute("autoplay", false);
  }
}

int LottiePlayer::direction() const { return properties_.direction; }

void LottiePlayer::set_direction(int direction) {
  SetAttribute("direction", base::Int32ToString(direction));
}

bool LottiePlayer::loop() const { return properties_.loop; }

void LottiePlayer::set_loop(bool loop) {
  // The value of 'loop' is true when the 'loop' attribute is present.
  // The value of the attribute is irrelevant.
  if (loop) {
    SetBooleanAttribute("loop", true);
  } else {
    SetBooleanAttribute("loop", false);
  }
}

double LottiePlayer::speed() const { return properties_.speed; }

void LottiePlayer::set_speed(double speed) {
  SetAttribute("speed", base::NumberToString(speed));
}

std::string LottiePlayer::renderer() const {
  // Cobalt uses a custom compiled-in renderer.
  return "skottie-m79";
}

void LottiePlayer::Play() {
  // https://lottiefiles.github.io/lottie-player/methods.html#play--void
  UpdateState(LottieAnimation::LottieState::kPlaying);
}

void LottiePlayer::Pause() {
  // https://lottiefiles.github.io/lottie-player/methods.html#pause--void
  UpdateState(LottieAnimation::LottieState::kPaused);
}

void LottiePlayer::Stop() {
  // https://lottiefiles.github.io/lottie-player/methods.html#stop--void
  UpdateState(LottieAnimation::LottieState::kStopped);
}

void LottiePlayer::Seek(FrameType frame) {
  // https://lottiefiles.github.io/lottie-player/methods.html#seekvalue-number--string--void
  if (frame.IsType<double>()) {
    properties_.SeekFrame(frame.AsType<double>());
  } else if (frame.IsType<std::string>()) {
    // Check whether a valid percent string.
    std::string frame_string = frame.AsType<std::string>();
    double frame_percent;
    base::StringToDouble(frame_string.substr(0, frame_string.length() - 1),
                         &frame_percent);
    if (frame_string.back() != '%' || !frame_percent) {
      DLOG(WARNING) << "Not a valid percent string: "
                    << frame.AsType<std::string>();
      return;
    }
    properties_.SeekPercent(frame_percent);
  }

  autoplaying_ = false;
  UpdateLottieObjects();
}

void LottiePlayer::SetDirection(int direction) {
  // https://lottiefiles.github.io/lottie-player/methods.html#setdirectionvalue-number--void
  if (properties_.UpdateDirection(direction)) {
    UpdateLottieObjects();
  }
}

void LottiePlayer::SetLooping(bool loop) {
  // https://lottiefiles.github.io/lottie-player/methods.html#setloopingvalue-boolean--void
  if (properties_.UpdateLoop(loop)) {
    UpdateLottieObjects();
  }
}

void LottiePlayer::SetSpeed(double speed) {
  // https://lottiefiles.github.io/lottie-player/methods.html#setspeedvalue-number--void
  if (properties_.UpdateSpeed(speed)) {
    UpdateLottieObjects();
  }
}

LottieAnimation::LottieProperties LottiePlayer::GetUpdatedProperties() {
  // The state may not have be initialized properly if the animation is
  // autoplaying.
  if (autoplaying_) {
    LottieAnimation::LottieState state =
        autoplay() ? LottieAnimation::LottieState::kPlaying
                   : LottieAnimation::LottieState::kStopped;
    properties_.UpdateState(state);
  }

  return properties_;
}

void LottiePlayer::PurgeCachedBackgroundImagesOfNodeAndDescendants() {
  if (!cached_image_loaded_callback_handler_) {
    return;
  }

  // While we are still loading, treat this as an error.
  OnLoadingError();
}

void LottiePlayer::OnSetAttribute(const std::string& name,
                                  const std::string& value) {
  if (name == "src") {
    UpdateAnimationData();
  } else if (name == "direction") {
    int direction;
    base::StringToInt32(value, &direction);
    SetDirection(direction);
  } else if (name == "loop") {
    SetLooping(true);
  } else if (name == "speed") {
    double speed;
    base::StringToDouble(value, &speed);
    SetSpeed(speed);
  } else {
    HTMLElement::OnSetAttribute(name, value);
  }
}

void LottiePlayer::OnRemoveAttribute(const std::string& name) {
  if (name == "src") {
    UpdateAnimationData();
  } else if (name == "direction") {
    SetDirection(LottieAnimation::LottieProperties::kDefaultDirection);
  } else if (name == "loop") {
    SetLooping(LottieAnimation::LottieProperties::kDefaultLoop);
  } else if (name == "speed") {
    SetSpeed(LottieAnimation::LottieProperties::kDefaultSpeed);
  } else {
    HTMLElement::OnRemoveAttribute(name);
  }
}

void LottiePlayer::UpdateAnimationData() {
  DCHECK(base::MessageLoop::current());
  DCHECK(node_document());
  TRACE_EVENT0("cobalt::dom", "LottiePlayer::UpdateAnimationData()");

  if (cached_image_loaded_callback_handler_) {
    cached_image_loaded_callback_handler_.reset();
    prevent_gc_until_load_complete_.reset();
    node_document()->DecreaseLoadingCounter();
  }

  const std::string src = GetAttribute("src").value_or("");

  if (!src.empty()) {
    const GURL& base_url = node_document()->url_as_gurl();
    const GURL selected_source = base_url.Resolve(src);
    if (!selected_source.is_valid()) {
      LOG(WARNING) << src << " cannot be resolved based on " << base_url << ".";
      return;
    }

    auto image_cache = node_document()->html_element_context()->image_cache();
    cached_image_ = image_cache->GetOrCreateCachedResource(selected_source,
                                                           loader::Origin());

    if (cached_image_->TryGetResource()) {
      PreventGarbageCollectionUntilEventIsDispatched(base::Tokens::load());
      return;
    }
  } else {
    PreventGarbageCollectionUntilEventIsDispatched(base::Tokens::error());
    return;
  }

  DCHECK(!prevent_gc_until_load_complete_);
  prevent_gc_until_load_complete_.reset(
      new script::GlobalEnvironment::ScopedPreventGarbageCollection(
          html_element_context()->script_runner()->GetGlobalEnvironment(),
          this));
  node_document()->IncreaseLoadingCounter();
  cached_image_loaded_callback_handler_.reset(
      new loader::image::CachedImage::OnLoadedCallbackHandler(
          cached_image_,
          base::Bind(&LottiePlayer::OnLoadingSuccess, base::Unretained(this)),
          base::Bind(&LottiePlayer::OnLoadingError, base::Unretained(this))));
}

void LottiePlayer::OnLoadingSuccess() {
  TRACE_EVENT0("cobalt::dom", "LottiePlayer::OnLoadingSuccess()");
  AllowGarbageCollectionAfterEventIsDispatched(
      base::Tokens::load(), std::move(prevent_gc_until_load_complete_));
  if (node_document()) {
    node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }
  cached_image_loaded_callback_handler_.reset();

  // Set up the Lottie objects in the box and render trees once the file has
  // successfully loaded.
  UpdateLottieObjects();
}

void LottiePlayer::OnLoadingError() {
  TRACE_EVENT0("cobalt::dom", "LottiePlayer::OnLoadingError()");
  AllowGarbageCollectionAfterEventIsDispatched(
      base::Tokens::error(), std::move(prevent_gc_until_load_complete_));
  if (node_document()) {
    node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }
  cached_image_loaded_callback_handler_.reset();
}

void LottiePlayer::PreventGarbageCollectionUntilEventIsDispatched(
    base::Token event_name) {
  std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
      prevent_gc_until_event_dispatch(
          new script::GlobalEnvironment::ScopedPreventGarbageCollection(
              html_element_context()->script_runner()->GetGlobalEnvironment(),
              this));
  AllowGarbageCollectionAfterEventIsDispatched(
      event_name, std::move(prevent_gc_until_event_dispatch));
}

void LottiePlayer::AllowGarbageCollectionAfterEventIsDispatched(
    base::Token event_name,
    std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
        scoped_prevent_gc) {
  PostToDispatchEventNameAndRunCallback(
      FROM_HERE, event_name,
      base::Bind(&LottiePlayer::DestroyScopedPreventGC,
                 base::AsWeakPtr<LottiePlayer>(this),
                 base::Passed(&scoped_prevent_gc)));
}

void LottiePlayer::DestroyScopedPreventGC(
    std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
        scoped_prevent_gc) {
  scoped_prevent_gc.reset();
}

void LottiePlayer::UpdateState(LottieAnimation::LottieState state) {
  autoplaying_ = false;
  if (properties_.UpdateState(state)) {
    UpdateLottieObjects();
  }
}

void LottiePlayer::UpdateLottieObjects() {
  node_document()->RecordMutation();
  InvalidateLayoutBoxesOfNodeAndAncestors();
}

}  // namespace dom
}  // namespace cobalt
