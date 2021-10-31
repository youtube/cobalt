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

#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/lottie_frame_custom_event.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/global_environment.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

using render_tree::LottieAnimation;

const char LottiePlayer::kTagName[] = "lottie-player";

LottiePlayer::LottiePlayer(Document* document)
    : HTMLElement(document, base::Token(kTagName)),
      autoplaying_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(event_queue_(this)),
      callback_task_runner_(base::MessageLoop::current()->task_runner()) {
  SetAnimationEventCallbacks();
}

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

std::string LottiePlayer::background() const {
  return GetAttribute("background").value_or("");
}

void LottiePlayer::set_background(std::string background) {
  SetAttribute("background", background);
}

int LottiePlayer::count() const { return properties_.count; }

void LottiePlayer::set_count(int count) {
  SetAttribute("count", base::Int32ToString(count));
}

int LottiePlayer::direction() const { return properties_.direction; }

void LottiePlayer::set_direction(int direction) {
  SetAttribute("direction", base::Int32ToString(direction));
}

bool LottiePlayer::hover() const { return GetBooleanAttribute("hover"); }

void LottiePlayer::set_hover(bool hover) {
  // The value of 'hover' is true when the 'hover' attribute is present.
  // The value of the attribute is irrelevant.
  if (hover) {
    SetBooleanAttribute("hover", true);
  } else {
    SetBooleanAttribute("hover", false);
  }
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

std::string LottiePlayer::mode() const { return properties_.GetModeAsString(); }

void LottiePlayer::set_mode(std::string mode) { SetAttribute("mode", mode); }

double LottiePlayer::speed() const { return properties_.speed; }

void LottiePlayer::set_speed(double speed) {
  SetAttribute("speed", base::NumberToString(speed));
}

std::string LottiePlayer::preserve_aspect_ratio() const {
  // Skottie animations default to "xMidYMid meet", meaning that the animation
  // will be uniformly scaled and centered relative to the element's width and
  // height.
  return "xMidYMid meet";
}

std::string LottiePlayer::renderer() const {
  // Cobalt uses a custom compiled-in renderer.
  return "skottie-m79";
}

void LottiePlayer::Load(std::string src) {
  // https://github.com/LottieFiles/lottie-player#loadsrc-string--object--void
  set_src(src);
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
    if (frame_string.empty()) {
      DLOG(WARNING) << "Percent string cannot be empty.";
      return;
    }
    double frame_percent;
    bool prefix_is_num = base::StringToDouble(
        frame_string.substr(0, frame_string.length() - 1), &frame_percent);
    if (frame_string.back() != '%' || !prefix_is_num) {
      DLOG(WARNING) << "Not a valid percent string: "
                    << frame.AsType<std::string>();
      return;
    }
    properties_.SeekPercent(frame_percent);
  }

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

void LottiePlayer::ToggleLooping() {
  // https://github.com/LottieFiles/lottie-player#togglelooping--void
  properties_.ToggleLooping();
  UpdateLottieObjects();
}

void LottiePlayer::TogglePlay() {
  // https://github.com/LottieFiles/lottie-player#toggleplay--void
  properties_.TogglePlay();

  UpdateLottieObjects();
}

LottieAnimation::LottieProperties LottiePlayer::GetProperties() const {
  return properties_;
}

void LottiePlayer::OnHover() {
  if (hover()) {
    UpdateState(LottieAnimation::LottieState::kPlaying);
  }
}

void LottiePlayer::OnUnHover() {
  if (hover()) {
    UpdateState(LottieAnimation::LottieState::kStopped);
  }
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
  } else if (name == "background") {
    SetStyleAttribute("background:" + value);
  } else if (name == "count") {
    int count;
    base::StringToInt32(value, &count);
    SetCount(count);
  } else if (name == "direction") {
    int direction;
    base::StringToInt32(value, &direction);
    SetDirection(direction);
  } else if (name == "loop") {
    SetLooping(true);
  } else if (name == "mode") {
    SetMode(value);
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
  } else if (name == "background") {
    SetStyleAttribute("background:transparent");
  } else if (name == "count") {
    SetCount(LottieAnimation::LottieProperties::kDefaultCount);
  } else if (name == "direction") {
    SetDirection(LottieAnimation::LottieProperties::kDefaultDirection);
  } else if (name == "loop") {
    SetLooping(LottieAnimation::LottieProperties::kDefaultLoop);
  } else if (name == "mode") {
    SetMode(LottieAnimation::LottieProperties::kDefaultMode);
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
      // The requested resource has already been loaded. Trigger the "load" and
      // "ready" events to indicate that the animation data is already loaded
      // and the DOM element is already ready.
      ScheduleEvent(base::Tokens::load());
      ScheduleEvent(base::Tokens::ready());
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
  // Trigger a load event to indicate that the animation data has loaded, and
  // then a "ready" event to indicate the DOM element is ready.
  ScheduleEvent(base::Tokens::load());
  ScheduleEvent(base::Tokens::ready());
  // If the animation is autoplaying and autoplay is true, then the animation
  // playback state needs to be set to LottieAnimation::LottieState::kPlaying.
  UpdatePlaybackStateIfAutoplaying();
}

void LottiePlayer::OnLoadingError() {
  TRACE_EVENT0("cobalt::dom", "LottiePlayer::OnLoadingError()");
  AllowGarbageCollectionAfterEventIsDispatched(
      base::Tokens::error(), std::move(prevent_gc_until_load_complete_));
  if (node_document()) {
    node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }
  cached_image_loaded_callback_handler_.reset();
  ScheduleEvent(base::Tokens::error());
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
  if (properties_.UpdateState(state)) {
    UpdateLottieObjects();
  }
}

void LottiePlayer::UpdatePlaybackStateIfAutoplaying() {
  if (autoplaying_ && autoplay()) {
    properties_.UpdateState(LottieAnimation::LottieState::kPlaying);
  }
  autoplaying_ = false;
}

void LottiePlayer::SetCount(int count) {
  if (properties_.UpdateCount(count)) {
    UpdateLottieObjects();
  }
}

void LottiePlayer::SetMode(std::string mode) {
  if (properties_.UpdateMode(mode)) {
    UpdateLottieObjects();
  }
}

void LottiePlayer::SetMode(LottieAnimation::LottieMode mode) {
  if (properties_.UpdateMode(mode)) {
    UpdateLottieObjects();
  }
}

void LottiePlayer::UpdateLottieObjects() {
  node_document()->RecordMutation();
  InvalidateLayoutBoxesOfNodeAndAncestors();
}

void LottiePlayer::ScheduleEvent(base::Token event_name) {
  // https://github.com/LottieFiles/lottie-player#events
  scoped_refptr<Event> event = new Event(event_name);
  event->set_target(this);
  event_queue_.Enqueue(event);
}

void LottiePlayer::SetAnimationEventCallbacks() {
  DCHECK(callback_task_runner_);
  properties_.onplay_callback =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 callback_task_runner_, FROM_HERE,
                 base::Bind(&LottiePlayer::OnPlay, base::AsWeakPtr(this)));
  properties_.onpause_callback =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 callback_task_runner_, FROM_HERE,
                 base::Bind(&LottiePlayer::OnPause, base::AsWeakPtr(this)));
  properties_.onstop_callback =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 callback_task_runner_, FROM_HERE,
                 base::Bind(&LottiePlayer::OnStop, base::AsWeakPtr(this)));
  properties_.oncomplete_callback =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 callback_task_runner_, FROM_HERE,
                 base::Bind(&LottiePlayer::OnComplete, base::AsWeakPtr(this)));
  properties_.onloop_callback =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 callback_task_runner_, FROM_HERE,
                 base::Bind(&LottiePlayer::OnLoop, base::AsWeakPtr(this)));
  properties_.onenterframe_callback = base::Bind(
      &LottiePlayer::CallOnEnterFrame, callback_task_runner_,
      base::Bind(&LottiePlayer::OnEnterFrame, base::AsWeakPtr(this)));
  properties_.onfreeze_callback =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 callback_task_runner_, FROM_HERE,
                 base::Bind(&LottiePlayer::OnFreeze, base::AsWeakPtr(this)));
  properties_.onunfreeze_callback =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 callback_task_runner_, FROM_HERE,
                 base::Bind(&LottiePlayer::OnUnfreeze, base::AsWeakPtr(this)));
}

void LottiePlayer::OnPlay() { ScheduleEvent(base::Tokens::play()); }

void LottiePlayer::OnPause() { ScheduleEvent(base::Tokens::pause()); }

void LottiePlayer::OnStop() { ScheduleEvent(base::Tokens::stop()); }

void LottiePlayer::OnComplete() { ScheduleEvent(base::Tokens::complete()); }

void LottiePlayer::OnLoop() { ScheduleEvent(base::Tokens::loop()); }

void LottiePlayer::OnEnterFrame(double frame, double seeker) {
  LottieFrameCustomEventDetail detail;
  detail.set_frame(frame);
  detail.set_seeker(seeker);

  scoped_refptr<LottieFrameCustomEvent> lottie_frame_custom_event =
      new LottieFrameCustomEvent("frame");
  lottie_frame_custom_event->set_detail(detail);
  lottie_frame_custom_event->set_target(this);
  event_queue_.Enqueue(lottie_frame_custom_event);
}

void LottiePlayer::CallOnEnterFrame(
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
    base::Callback<void(double, double)> enter_frame_callback, double frame,
    double seeker) {
  callback_task_runner->PostTask(
      FROM_HERE, base::Bind(enter_frame_callback, frame, seeker));
}

void LottiePlayer::OnFreeze() {
  if (properties_.FreezeAnimationState()) {
    ScheduleEvent(base::Tokens::freeze());
    UpdateLottieObjects();
  }
}

void LottiePlayer::OnUnfreeze() {
  if (properties_.UnfreezeAnimationState()) {
    ScheduleEvent(base::Tokens::play());
    UpdateLottieObjects();
  }
}

}  // namespace dom
}  // namespace cobalt
