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

#include "cobalt/dom/html_media_element.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/instance_counter.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/map_to_mesh_function.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/eme/media_encrypted_event.h"
#include "cobalt/dom/eme/media_encrypted_event_init.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_video_element.h"
#include "cobalt/dom/media_settings.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/media_source_ready_state.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/url_fetcher_data_source.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/context.h"
#include "cobalt/web/csp_delegate.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/event.h"
#include "cobalt/web/web_settings.h"

namespace cobalt {
namespace dom {

using media::DataSource;
using media::WebMediaPlayer;

namespace {

#define LOG_MEDIA_ELEMENT_ACTIVITIES 0

#if LOG_MEDIA_ELEMENT_ACTIVITIES

#define MLOG() LOG(INFO) << __FUNCTION__ << ": "

#else  // LOG_MEDIA_ELEMENT_ACTIVITIES

#define MLOG() EAT_STREAM_PARAMETERS

#endif  // LOG_MEDIA_ELEMENT_ACTIVITIES

constexpr char kMediaSourceUrlProtocol[] = "blob";
constexpr int kTimeupdateEventIntervalInMilliseconds = 200;

DECLARE_INSTANCE_COUNTER(HTMLMediaElement);

loader::RequestMode GetRequestMode(
    const base::Optional<std::string>& cross_origin_attribute) {
  // https://html.spec.whatwg.org/#cors-settings-attribute
  if (cross_origin_attribute) {
    if (*cross_origin_attribute == "use-credentials") {
      return loader::kCORSModeIncludeCredentials;
    } else {
      // The invalid value default of crossorigin is Anonymous state, leading to
      // "same-origin" credentials mode.
      return loader::kCORSModeSameOriginCredentials;
    }
  }
  // crossorigin attribute's missing value default is No CORS state, leading to
  // "no-cors" request mode.
  return loader::kNoCORSMode;
}

#if SB_HAS(PLAYER_WITH_URL)
bool ResourceNeedsUrlPlayer(const GURL& resource_url) {
  if (resource_url.SchemeIs("data")) {
    return true;
  }
  // Check if resource_url is an hls url. Hls url must contain "hls_variant".
  return resource_url.spec().find("hls_variant") != std::string::npos;
}
#endif  // SB_HAS(PLAYER_WITH_URL)

bool OriginIsSafe(loader::RequestMode request_mode, const GURL& resource_url,
                  const loader::Origin& origin) {
#if SB_HAS(PLAYER_WITH_URL)
  if (ResourceNeedsUrlPlayer(resource_url)) {
    return true;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  if (resource_url.SchemeIs("blob")) {
    // Blob resources come from application and is same-origin.
    return true;
  }
  if (request_mode != loader::kNoCORSMode) {
    return true;
  }
  if (origin == loader::Origin(resource_url)) {
    return true;
  }
  if (resource_url.SchemeIs("data")) {
    return true;
  }
  return false;
}

}  // namespace

HTMLMediaElement::HTMLMediaElement(Document* document, base::Token tag_name)
    : HTMLElement(document, tag_name),
      load_state_(kWaitingForSource),
      ALLOW_THIS_IN_INITIALIZER_LIST(event_queue_(this)),
      playback_rate_(1.f),
      default_playback_rate_(1.0f),
      network_state_(kNetworkEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      ready_state_maximum_(WebMediaPlayer::kReadyStateHaveNothing),
      volume_(1.0f),
      last_seek_time_(0.0),
      previous_progress_time_(std::numeric_limits<double>::max()),
      duration_(std::numeric_limits<double>::quiet_NaN()),
      playing_(false),
      have_fired_loaded_data_(false),
      autoplaying_(true),
      muted_(false),
      paused_(true),
      resume_frozen_flag_(false),
      seeking_(false),
      controls_(false),
      last_time_update_event_movie_time_(std::numeric_limits<double>::max()),
      processing_media_player_callback_(0),
      media_source_url_(std::string(kMediaSourceUrlProtocol) + ':' +
                        base::GenerateGUID()),
      pending_load_(false),
      sent_stalled_event_(false),
      sent_end_event_(false),
      request_mode_(loader::kNoCORSMode) {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::HTMLMediaElement()");
  LOG(INFO) << "Create HTMLMediaElement with volume " << volume_
            << ", playback rate " << playback_rate_ << ".";
  ON_INSTANCE_CREATED(HTMLMediaElement);
}

HTMLMediaElement::~HTMLMediaElement() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::~HTMLMediaElement()");
  LOG(INFO) << "Destroy HTMLMediaElement.";
  ClearMediaSource();
  ON_INSTANCE_RELEASED(HTMLMediaElement);
}

scoped_refptr<MediaError> HTMLMediaElement::error() const {
  MLOG() << (error_ ? error_->code() : 0);
  return error_;
}

std::string HTMLMediaElement::src() const {
  MLOG() << GetAttribute("src").value_or("");
  return GetAttribute("src").value_or("");
}

void HTMLMediaElement::set_src(const std::string& src) {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::set_src()");
  LOG(INFO) << "Set src to \"" << src << "\".";
  SetAttribute("src", src);
  ClearMediaPlayer();
  ScheduleLoad();
}

base::Optional<std::string> HTMLMediaElement::cross_origin() const {
  base::Optional<std::string> cross_origin_attribute =
      GetAttribute("crossOrigin");
  if (cross_origin_attribute &&
      (*cross_origin_attribute != "anonymous" &&
       *cross_origin_attribute != "use-credentials")) {
    return std::string();
  }
  return cross_origin_attribute;
}

void HTMLMediaElement::set_cross_origin(
    const base::Optional<std::string>& value) {
  if (value) {
    SetAttribute("crossOrigin", *value);
  } else {
    RemoveAttribute("crossOrigin");
  }
}

uint16_t HTMLMediaElement::network_state() const {
  MLOG() << network_state_;
  return static_cast<uint16_t>(network_state_);
}

scoped_refptr<TimeRanges> HTMLMediaElement::buffered() const {
  scoped_refptr<TimeRanges> buffered = new TimeRanges;

  if (!player_) {
    LOG(INFO) << "(empty)";
    return buffered;
  }

  player_->UpdateBufferedTimeRanges(
      [&](double start, double end) { buffered->Add(start, end); });
  return buffered;
}

void HTMLMediaElement::Load() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::Load()");
  // LoadInternal may result in a 'beforeload' event, which can make arbitrary
  // DOM mutations.
  scoped_refptr<HTMLMediaElement> protect(this);

  PrepareForLoad();
  LoadInternal();
}

std::string HTMLMediaElement::CanPlayType(const std::string& mime_type) {
  return CanPlayType(mime_type, "");
}

std::string HTMLMediaElement::CanPlayType(const std::string& mime_type,
                                          const std::string& key_system) {
  TRACE_EVENT2("cobalt::dom", "HTMLMediaElement::CanPlayType()", "mime_type",
               mime_type, "key_system", key_system);
  DCHECK(html_element_context()->can_play_type_handler());

  DLOG_IF(ERROR, !key_system.empty())
      << "CanPlayType() only accepts one parameter but (" << key_system
      << ") is passed as a second parameter.";
  auto support_type =
      html_element_context()->can_play_type_handler()->CanPlayProgressive(
          mime_type);
  std::string result = "";
  switch (support_type) {
    case kSbMediaSupportTypeNotSupported:
      result = "";
      break;
    case kSbMediaSupportTypeMaybe:
      result = "maybe";
      break;
    case kSbMediaSupportTypeProbably:
      result = "probably";
      break;
    default:
      NOTREACHED();
  }
  MLOG() << "(" << mime_type << ", " << key_system << ") => " << result;
  return result;
}

const web::EventTarget::EventListenerScriptValue*
HTMLMediaElement::onencrypted() const {
  return GetAttributeEventListener(base::Tokens::encrypted());
}

void HTMLMediaElement::set_onencrypted(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::encrypted(), event_listener);
}

// See https://www.w3.org/TR/encrypted-media/#dom-htmlmediaelement-setmediakeys.
script::Handle<script::Promise<void>> HTMLMediaElement::SetMediaKeys(
    const scoped_refptr<eme::MediaKeys>& media_keys) {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::SetMediaKeys()");

  script::Handle<script::Promise<void>> promise =
      node_document()
          ->html_element_context()
          ->script_value_factory()
          ->CreateBasicPromise<void>();

  // 1. If mediaKeys and the mediaKeys attribute are the same object, return
  //    a resolved promise.
  if (media_keys_ == media_keys) {
    promise->Resolve();
    return promise;
  }

  // 5.2. If the mediaKeys attribute is not null:
  if (media_keys_) {
    // 5.2.3. Stop using the CDM instance represented by the mediaKeys attribute
    //        to decrypt media data and remove the association with the media
    //        element.
    //
    // Since Starboard doesn't allow to detach SbDrmSystem from SbPlayer,
    // re-create the entire player.
    if (player_) {
      ClearMediaPlayer();
      // |media_keys_| is used in CreateMediaPlayer(). Reset it before calling
      // CreateMediaPlayer().
      media_keys_.reset();
      CreateMediaPlayer();
    }
  }

  // 5.3. If mediaKeys is not null:
  if (media_keys) {
    // 5.3.1. Associate the CDM instance represented by mediaKeys with
    //        the media element for decrypting media data.
    if (player_) {
      player_->SetDrmSystem(media_keys->drm_system());
    }
  }

  // 5.4. Set the mediaKeys attribute to mediaKeys.
  media_keys_ = media_keys;

  // 5.6. Resolve promise.
  promise->Resolve();

  // 6. Return promise.
  return promise;
}

uint16_t HTMLMediaElement::ready_state() const {
  MLOG() << ready_state_;
  return static_cast<uint16_t>(ready_state_);
}

bool HTMLMediaElement::seeking() const {
  MLOG() << seeking_;
  return seeking_;
}

double HTMLMediaElement::current_time(
    script::ExceptionState* exception_state) const {
  if (!player_) {
    LOG(INFO) << 0 << " (because player is NULL)";
    return 0.0;
  }

  if (seeking_) {
    MLOG() << last_seek_time_ << " (seeking)";
    return last_seek_time_;
  }

  const double time = player_->GetCurrentTime();
  MLOG() << time << " (from player)";
  return time;
}

void HTMLMediaElement::set_current_time(
    double time, script::ExceptionState* exception_state) {
  // 4.8.9.9 Seeking
  // 1 - If the media element's readyState is
  // WebMediaPlayer::kReadyStateHaveNothing, then raise an INVALID_STATE_ERR
  // exception.
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing || !player_) {
    LOG(ERROR) << "invalid state error";
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  Seek(time);
}

double HTMLMediaElement::duration() const {
  MLOG() << duration_;
  return duration_;
}

base::Time HTMLMediaElement::GetStartDate() const {
  MLOG() << start_date_.ToSbTime();
  return start_date_;
}

bool HTMLMediaElement::paused() const {
  MLOG() << paused_;
  return paused_;
}

bool HTMLMediaElement::resume_frozen_flag() const {
  return resume_frozen_flag_;
}

float HTMLMediaElement::default_playback_rate() const {
  MLOG() << default_playback_rate_;
  return default_playback_rate_;
}

void HTMLMediaElement::set_default_playback_rate(float rate) {
  MLOG() << rate;
  if (default_playback_rate_ != rate) {
    default_playback_rate_ = rate;
    ScheduleOwnEvent(base::Tokens::ratechange());
  }
}

float HTMLMediaElement::playback_rate() const {
  MLOG() << playback_rate_;
  return playback_rate_;
}

void HTMLMediaElement::set_playback_rate(float rate) {
  LOG(INFO) << "Change playback rate from " << playback_rate_ << " to " << rate
            << ".";

  if (playback_rate_ != rate) {
    playback_rate_ = rate;
    ScheduleOwnEvent(base::Tokens::ratechange());
  }

  if (player_ && PotentiallyPlaying()) {
    player_->SetRate(rate *
                     html_element_context()->video_playback_rate_multiplier());
  }
}

const scoped_refptr<TimeRanges>& HTMLMediaElement::played() {
  MLOG();
  if (playing_) {
    const double time = current_time(NULL);
    if (time > last_seek_time_) {
      AddPlayedRange(last_seek_time_, time);
    }
  }

  if (!played_time_ranges_) {
    played_time_ranges_ = new TimeRanges;
  }

  return played_time_ranges_;
}

scoped_refptr<TimeRanges> HTMLMediaElement::seekable() const {
  if (player_ && player_->GetMaxTimeSeekable() != 0.0) {
    const double max_time_seekable = player_->GetMaxTimeSeekable();
    MLOG() << "(0, " << max_time_seekable << ")";
    return new TimeRanges(0, max_time_seekable);
  }
  MLOG() << "(empty)";
  return new TimeRanges;
}

bool HTMLMediaElement::ended() const {
  // 4.8.10.8 Playing the media resource
  // The ended attribute must return true if the media element has ended
  // playback and the direction of playback is forwards, and false otherwise.
  bool playback_ended = EndedPlayback() && playback_rate_ > 0;
  MLOG() << playback_ended;
  return playback_ended;
}

bool HTMLMediaElement::autoplay() const {
  MLOG() << HasAttribute("autoplay");
  return HasAttribute("autoplay");
}

void HTMLMediaElement::set_autoplay(bool autoplay) {
  LOG(INFO) << "Set autoplay to " << autoplay << ".";
  // The value of 'autoplay' is true when the 'autoplay' attribute is present.
  // The value of the attribute is irrelevant.
  if (autoplay) {
    SetAttribute("autoplay", "");
  } else {
    RemoveAttribute("autoplay");
  }
}

bool HTMLMediaElement::loop() const {
  MLOG() << HasAttribute("loop");
  return HasAttribute("loop");
}

void HTMLMediaElement::set_loop(bool loop) {
  LOG(INFO) << "Set loop to " << loop << ".";
  // The value of 'loop' is true when the 'loop' attribute is present.
  // The value of the attribute is irrelevant.
  if (loop) {
    SetAttribute("loop", "");
  } else {
    RemoveAttribute("loop");
  }
}

void HTMLMediaElement::Play() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::Play()");
  LOG(INFO) << "Play.";
  // 4.8.10.9. Playing the media resource
  if (!player_ || network_state_ == kNetworkEmpty) {
    ScheduleLoad();
  }

  if (EndedPlayback()) {
    Seek(0);
  }

  if (paused_) {
    paused_ = false;
    ScheduleOwnEvent(base::Tokens::play());

    if (ready_state_ <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleOwnEvent(base::Tokens::waiting());
    } else if (ready_state_ >= WebMediaPlayer::kReadyStateHaveFutureData) {
      ScheduleOwnEvent(base::Tokens::playing());
    }
  }
  autoplaying_ = false;

  UpdatePlayState();
}

void HTMLMediaElement::Pause() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::Pause()");
  LOG(INFO) << "Pause.";
  // 4.8.10.9. Playing the media resource
  if (!player_ || network_state_ == kNetworkEmpty) {
    ScheduleLoad();
  }

  autoplaying_ = false;

  if (!paused_) {
    paused_ = true;
    ScheduleTimeupdateEvent(false);
    ScheduleOwnEvent(base::Tokens::pause());
  }

  UpdatePlayState();
}

void HTMLMediaElement::set_resume_frozen_flag(bool resume_frozen_flag) {
  resume_frozen_flag_ = resume_frozen_flag;
}

bool HTMLMediaElement::controls() const {
  MLOG() << controls_;
  return controls_;
}

void HTMLMediaElement::set_controls(bool controls) {
  MLOG() << controls_;
  controls_ = controls;
  ConfigureMediaControls();
}

float HTMLMediaElement::volume(script::ExceptionState* exception_state) const {
  MLOG() << volume_;
  return volume_;
}

void HTMLMediaElement::set_volume(float volume,
                                  script::ExceptionState* exception_state) {
  LOG(INFO) << "Change volume from " << volume_ << " to " << volume << ".";
  if (volume < 0.0f || volume > 1.0f) {
    web::DOMException::Raise(web::DOMException::kIndexSizeErr, exception_state);
    return;
  }

  if (volume_ != volume) {
    volume_ = volume;
    UpdateVolume();
    ScheduleOwnEvent(base::Tokens::volumechange());
  }
}

bool HTMLMediaElement::muted() const {
  MLOG() << muted_;
  return muted_;
}

void HTMLMediaElement::set_muted(bool muted) {
  LOG(INFO) << "Change muted from " << muted_ << " to " << muted << ".";
  if (muted_ != muted) {
    muted_ = muted;
    // Avoid recursion when the player reports volume changes.
    if (!ProcessingMediaPlayerCallback()) {
      if (player_) {
        player_->SetVolume(muted_ ? 0 : volume_);
      }
    }
    ScheduleOwnEvent(base::Tokens::volumechange());
  }
}

void HTMLMediaElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();

  std::string src = GetAttribute("src").value_or("");
  if (!src.empty()) {
    set_src(src);
  }

  if (HasAttribute("muted")) {
    set_muted(true);
  }
}

void HTMLMediaElement::TraceMembers(script::Tracer* tracer) {
  HTMLElement::TraceMembers(tracer);

  tracer->Trace(event_queue_);
  tracer->Trace(played_time_ranges_);
  tracer->Trace(media_source_);
  tracer->Trace(error_);
  tracer->Trace(media_keys_);
}

void HTMLMediaElement::DurationChanged(double duration, bool request_seek) {
  MLOG() << "DurationChanged(" << duration << ", " << request_seek << ")";

  // Abort if duration unchanged.
  if (duration_ == duration) {
    return;
  }
  duration_ = duration;

  ScheduleOwnEvent(base::Tokens::durationchange());

  if (request_seek) {
    Seek(duration);
  }
}

void HTMLMediaElement::ScheduleEvent(const scoped_refptr<web::Event>& event) {
  TRACE_EVENT1("cobalt::dom", "HTMLMediaElement::ScheduleEvent()", "event",
               TRACE_STR_COPY(event->type().c_str()));
  MLOG() << "Schedule event " << event->type() << ".";
  event_queue_.Enqueue(event);
}

std::string HTMLMediaElement::h5vcc_audio_connectors(
    script::ExceptionState* exception_state) const {
#if SB_API_VERSION >= 15
  if (!player_) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return std::string();
  }

  std::vector<std::string> configs = player_->GetAudioConnectors();
  return base::JoinString(configs, ";");
#else   // SB_API_VERSION >= 15
  web::DOMException::Raise(web::DOMException::kNotSupportedErr,
                           exception_state);
  return std::string();
#endif  // SB_API_VERSION >= 15
}

void HTMLMediaElement::CreateMediaPlayer() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::CreateMediaPlayer()");
  LOG(INFO) << "Create media player.";
  if (src().empty()) {
    reduced_image_cache_capacity_request_ = base::nullopt;
  } else if (html_element_context()
                 ->reduced_image_cache_capacity_manager()
                 ->reduced_capacity_percentage() != 1.0f) {
    // Platforms with constrained GPU memory typically are placed in their most
    // fragile position when a video is playing.  Thus, we request a lower image
    // cache size and empty the cache before we start playing a video, in
    // an effort to make room in memory for video decoding.  Reducing the
    // image cache during this time should also be fine as the UI is not usually
    // in the foreground during this time.

    // Set a new reduced cache capacity.
    reduced_image_cache_capacity_request_.emplace(
        html_element_context()->reduced_image_cache_capacity_manager());

    // Ensure that all resource destructions are flushed and the memory is
    // reclaimed.
    if (*html_element_context()->resource_provider()) {
      (*html_element_context()->resource_provider())->Finish();
    }
  }

  if (!html_element_context()->web_media_player_factory()) {
    DLOG(ERROR) << "Media playback in CONCEALED is not supported.";
    return;
  }

  player_ =
      html_element_context()->web_media_player_factory()->CreateWebMediaPlayer(
          this);
  if (media_keys_) {
    player_->SetDrmSystem(media_keys_->drm_system());
  }
  node_document()->OnDOMMutation();
  InvalidateLayoutBoxesOfNodeAndAncestors();
}

void HTMLMediaElement::ScheduleLoad() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::ScheduleLoad()");
  if (!pending_load_) {
    PrepareForLoad();
    pending_load_ = true;
  }

  if (!load_timer_.IsRunning()) {
    load_timer_.Start(FROM_HERE, base::TimeDelta(), this,
                      &HTMLMediaElement::OnLoadTimer);
  }
}

void HTMLMediaElement::PrepareForLoad() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::PrepareForLoad()");
  // Perform the cleanup required for the resource load algorithm to run.
  StopPeriodicTimers();
  load_timer_.Stop();
  sent_end_event_ = false;
  sent_stalled_event_ = false;
  have_fired_loaded_data_ = false;

  // 1 - Abort any already-running instance of the resource selection algorithm
  // for this element.
  load_state_ = kWaitingForSource;

  // 2 - If there are any tasks from the media element's media element event
  // task source in
  // one of the task queues, then remove those tasks.
  CancelPendingEventsAndCallbacks();

  // 3 - If the media element's networkState is set to kNetworkLoading or
  // kNetworkIdle, queue a task to fire a simple event named abort at the media
  // element.
  if (network_state_ == kNetworkLoading || network_state_ == kNetworkIdle) {
    ScheduleOwnEvent(base::Tokens::abort());
  }

  ClearMediaPlayer();
  CreateMediaPlayer();

  // 4 - If the media element's networkState is not set to kNetworkEmpty, then
  // run these substeps
  if (network_state_ != kNetworkEmpty) {
    network_state_ = kNetworkEmpty;
    ready_state_ = WebMediaPlayer::kReadyStateHaveNothing;
    ready_state_maximum_ = WebMediaPlayer::kReadyStateHaveNothing;
    paused_ = true;
    seeking_ = false;
    ScheduleOwnEvent(base::Tokens::emptied());
  }

  // 5 - Set the playbackRate attribute to the value of the defaultPlaybackRate
  // attribute.
  set_playback_rate(default_playback_rate());

  // 6 - Set the error attribute to null and the autoplaying flag to true.
  error_ = NULL;
  autoplaying_ = true;

  // 7 - Invoke the media element's resource selection algorithm.

  // 8 - Note: Playback of any previously playing media resource for this
  // element stops.

  // The resource selection algorithm
  // 1 - Set the networkState to kNetworkNoSource.
  network_state_ = kNetworkNoSource;
  // When network_state_ is |kNetworkNoSource|, set duration to NaN.
  duration_ = std::numeric_limits<double>::quiet_NaN();

  // 2 - Asynchronously await a stable state.
  played_time_ranges_ = new TimeRanges;
  last_seek_time_ = 0.0;

  ConfigureMediaControls();
}

void HTMLMediaElement::LoadInternal() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::LoadInternal()");
  DCHECK(node_document());

  // Select media resource.
  enum Mode { kAttribute, kChildren };

  // 3 - If the media element has a src attribute, then let mode be kAttribute.
  std::string src = this->src();
  Mode mode = kAttribute;
  if (src.empty()) {
    // Otherwise the media element has neither a src attribute nor a source
    // element child: set the networkState to kNetworkEmpty, and abort these
    // steps; the synchronous section ends.
    load_state_ = kWaitingForSource;
    network_state_ = kNetworkEmpty;

    DLOG(WARNING) << "HTMLMediaElement::LoadInternal, nothing to load";
    return;
  }

  // 4 - Set the media element's delaying-the-load-event flag to true (this
  // delays the load event), and set its networkState to kNetworkLoading.
  network_state_ = kNetworkLoading;

  // 5 - Queue a task to fire a simple event named loadstart at the media
  // element.
  ScheduleOwnEvent(base::Tokens::loadstart());

  // 6 - If mode is kAttribute, then run these substeps.
  if (mode == kAttribute) {
    load_state_ = kLoadingFromSrcAttr;

    // If the src attribute's value is the empty string ... jump down to the
    // failed step below.
    GURL media_url(src);
    if (media_url.is_empty()) {
      // Try to resolve it as a relative url.
      media_url = node_document()->location()->url().Resolve(src);
    }
    if (media_url.is_empty()) {
      MediaLoadingFailed(WebMediaPlayer::kNetworkStateFormatError,
                         "Invalid source.");
      DLOG(WARNING) << "HTMLMediaElement::LoadInternal, invalid 'src' " << src;
      return;
    }

    // No type or key system information is available when the url comes from
    // the 'src' attribute so WebMediaPlayer will have to pick a media engine
    // based on the file extension.
    LoadResource(media_url, "", std::string());
    DLOG(INFO) << "HTMLMediaElement::LoadInternal, using 'src' attribute url";
    return;
  }
}

void HTMLMediaElement::LoadResource(const GURL& initial_url,
                                    const std::string& content_type,
                                    const std::string& key_system) {
  DLOG(INFO) << "HTMLMediaElement::LoadResource(" << initial_url << ", "
             << content_type << ", " << key_system;
  if (!player_) {
    LOG(WARNING) << "LoadResource() without player.";
    return;
  }

  GURL url = initial_url;

  if (!url.is_valid()) {
    // Try to filter out invalid urls as GURL::spec() DCHECKs if the url is
    // valid.
    NoneSupported("URL is invalid.");
    return;
  }

  DCHECK(!media_source_);
  if (url.SchemeIs(kMediaSourceUrlProtocol)) {
    // Check whether url is allowed by security policy.
    if (!node_document()->GetCSPDelegate()->CanLoad(web::CspDelegate::kMedia,
                                                    url, false)) {
      DLOG(INFO) << "URL " << url << " is rejected by security policy.";
      NoneSupported("URL is rejected by security policy.");
      return;
    }

    media_source_ =
        html_element_context()->media_source_registry()->Retrieve(url.spec());
    if (!media_source_) {
      NoneSupported("Media source is NULL.");
      return;
    }
    if (!media_source_->AttachToElement(this)) {
      media_source_ = nullptr;
      NoneSupported("Unable to attach media source.");
      return;
    }
    media_source_url_ = url;
  }
  // The resource fetch algorithm
  network_state_ = kNetworkLoading;

  // Set current_src_ *before* changing to the cache url, the fact that we are
  // loading from the app cache is an internal detail not exposed through the
  // media element API.
  current_src_ = url.spec();

  StartProgressEventTimer();

  UpdateVolume();

  if (url.is_empty()) {
    return;
  }

#if SB_HAS(PLAYER_WITH_URL)
  if (ResourceNeedsUrlPlayer(url)) {
    // TODO: Investigate if we have to support csp and sop for url player.
    player_->LoadUrl(url);
    return;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  if (url.spec() == SourceURL()) {
    player_->LoadMediaSource();
  } else {
    csp::SecurityCallback csp_callback =
        base::Bind(&web::CspDelegate::CanLoad,
                   base::Unretained(node_document()->GetCSPDelegate()),
                   web::CspDelegate::kMedia);
    request_mode_ = GetRequestMode(GetAttribute("crossOrigin"));
    DCHECK(node_document()->location());
    std::unique_ptr<DataSource> data_source(new media::URLFetcherDataSource(
        base::ThreadTaskRunnerHandle::Get(), url, csp_callback,
        html_element_context()->fetcher_factory()->network_module(),
        request_mode_, node_document()->location()->GetOriginAsObject()));
    player_->LoadProgressive(url, std::move(data_source));
  }
}

void HTMLMediaElement::ClearMediaPlayer() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::ClearMediaPlayer()");
  LOG(INFO) << "Clear media player.";

  ClearMediaSource();

  player_.reset(NULL);

  StopPeriodicTimers();
  load_timer_.Stop();

  pending_load_ = false;
  load_state_ = kWaitingForSource;

  reduced_image_cache_capacity_request_ = base::nullopt;

  if (node_document()) {
    node_document()->OnDOMMutation();
  }
}

void HTMLMediaElement::NoneSupported(const std::string& message) {
  MLOG();

  DLOG(WARNING) << "HTMLMediaElement::NoneSupported() error with message: "
                << message;

  StopPeriodicTimers();
  load_state_ = kWaitingForSource;

  // 4.8.10.5
  // 6 - Reaching this step indicates that the media resource failed to load or
  // that the given URL could not be resolved. In one atomic operation, run the
  // following steps:

  // 6.1 - Set the error attribute to a new MediaError object whose code
  // attribute is set to MEDIA_ERR_SRC_NOT_SUPPORTED.
  error_ = new MediaError(MediaError::kMediaErrSrcNotSupported,
                          message.empty() ? "Source not supported." : message);
  // 6.2 - Forget the media element's media-resource-specific text tracks.

  // 6.3 - Set the element's networkState attribute to the kNetworkNoSource
  // value.
  network_state_ = kNetworkNoSource;

  // 7 - Queue a task to fire a simple event named error at the media element.
  ScheduleOwnEvent(base::Tokens::error());

  ClearMediaSource();
}

void HTMLMediaElement::MediaLoadingFailed(WebMediaPlayer::NetworkState error,
                                          const std::string& message) {
  DLOG(INFO) << "media loading failed";
  StopPeriodicTimers();

  if (error == WebMediaPlayer::kNetworkStateNetworkError &&
      ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata) {
    MediaEngineError(new MediaError(
        MediaError::kMediaErrNetwork,
        message.empty() ? "Media loading failed with network error."
                        : message));
  } else if (error == WebMediaPlayer::kNetworkStateDecodeError) {
    MediaEngineError(new MediaError(
        MediaError::kMediaErrDecode,
        message.empty() ? "Media loading failed with decode error." : message));
  } else if (error == WebMediaPlayer::kNetworkStateCapabilityChangedError) {
    MediaEngineError(new MediaError(
        MediaError::kMediaErrCapabilityChanged,
        message.empty() ? "Media loading failed with capability changed error."
                        : message));
  } else if ((error == WebMediaPlayer::kNetworkStateFormatError ||
              error == WebMediaPlayer::kNetworkStateNetworkError) &&
             load_state_ == kLoadingFromSrcAttr) {
    NoneSupported(message.empty() ? "Media loading failed with none supported."
                                  : message);
  }
}

// This is kept as a one shot timer to be in sync with the original code. It
// should be replaced by PostTask if this is any future rewrite.
void HTMLMediaElement::OnLoadTimer() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::OnLoadTimer()");
  scoped_refptr<HTMLMediaElement> protect(this);

  if (pending_load_) {
    LoadInternal();
  }

  pending_load_ = false;
}

void HTMLMediaElement::OnProgressEventTimer() {
  DCHECK(player_);
  if (!player_) {
    return;
  }
  if (network_state_ != kNetworkLoading) {
    return;
  }

  const double time = base::Time::Now().ToDoubleT();
  const double time_delta = time - previous_progress_time_;

  if (player_->DidLoadingProgress()) {
    ScheduleOwnEvent(base::Tokens::progress());
    previous_progress_time_ = time;
    sent_stalled_event_ = false;
  } else if (time_delta > 3.0 && !sent_stalled_event_) {
    ScheduleOwnEvent(base::Tokens::stalled());
    sent_stalled_event_ = true;
  }
}

void HTMLMediaElement::OnPlaybackProgressTimer() {
  DCHECK(player_);
  if (!player_) {
    return;
  }

  ScheduleTimeupdateEvent(true);
}

void HTMLMediaElement::StartPlaybackProgressTimer() {
  if (playback_progress_timer_.IsRunning()) {
    return;
  }

  previous_progress_time_ = base::Time::Now().ToDoubleT();

  DCHECK(environment_settings());
  DCHECK(environment_settings()->context());
  DCHECK(environment_settings()->context()->web_settings());

  const auto& media_settings =
      environment_settings()->context()->web_settings()->media_settings();
  const int interval_in_milliseconds =
      media_settings.GetMediaElementTimeupdateEventIntervalInMilliseconds()
          .value_or(kTimeupdateEventIntervalInMilliseconds);

  playback_progress_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(interval_in_milliseconds),
      this, &HTMLMediaElement::OnPlaybackProgressTimer);
}

void HTMLMediaElement::StartProgressEventTimer() {
  if (progress_event_timer_.IsRunning()) {
    return;
  }

  previous_progress_time_ = base::Time::Now().ToDoubleT();
  // 350ms is not magic, it is in the spec!
  progress_event_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(350),
                              this, &HTMLMediaElement::OnProgressEventTimer);
}

void HTMLMediaElement::StopPeriodicTimers() {
  progress_event_timer_.Stop();
  playback_progress_timer_.Stop();
}

void HTMLMediaElement::ScheduleTimeupdateEvent(bool periodic_event) {
  // Some media engines make multiple "time changed" callbacks at the same time,
  // but we only want one event at a given time so filter here
  const double movie_time = current_time(NULL);
  if (movie_time != last_time_update_event_movie_time_) {
    if (!periodic_event && playback_progress_timer_.IsRunning()) {
      playback_progress_timer_.Reset();
    }
    ScheduleOwnEvent(base::Tokens::timeupdate());
    last_time_update_event_movie_time_ = movie_time;
  }
}

void HTMLMediaElement::ScheduleOwnEvent(base::Token event_name) {
  LOG_IF(INFO, event_name == base::Tokens::error())
      << "onerror event fired with error " << (error_ ? error_->code() : 0);
  MLOG() << event_name;
  scoped_refptr<web::Event> event = new web::Event(
      event_name, web::Event::kNotBubbles, web::Event::kCancelable);
  event->set_target(this);

  ScheduleEvent(event);
}

void HTMLMediaElement::CancelPendingEventsAndCallbacks() {
  event_queue_.CancelAllEvents();
}

void HTMLMediaElement::SetReadyState(WebMediaPlayer::ReadyState state) {
  TRACE_EVENT1("cobalt::dom", "HTMLMediaElement::SetReadyState()", "state",
               state);
  LOG(INFO) << "Set ready state from " << ready_state_ << " to " << state
            << ".";
  // Set "was_potentially_playing" BEFORE updating ready_state_,
  // PotentiallyPlaying() uses it
  bool was_potentially_playing = PotentiallyPlaying();

  WebMediaPlayer::ReadyState old_state = ready_state_;
  WebMediaPlayer::ReadyState new_state =
      static_cast<WebMediaPlayer::ReadyState>(state);

  if (new_state == old_state) {
    return;
  }

  ready_state_ = new_state;

  if (old_state > ready_state_maximum_) {
    ready_state_maximum_ = old_state;
  }

  if (network_state_ == kNetworkEmpty) {
    return;
  }

  if (seeking_) {
    // 4.8.10.9, step 11
    if (was_potentially_playing &&
        ready_state_ < WebMediaPlayer::kReadyStateHaveFutureData) {
      ScheduleOwnEvent(base::Tokens::waiting());
    }
    // 4.8.10.10 step 14 & 15.
    if (ready_state_ >= WebMediaPlayer::kReadyStateHaveCurrentData) {
      FinishSeek();
    }
  } else {
    if (was_potentially_playing &&
        ready_state_ < WebMediaPlayer::kReadyStateHaveFutureData) {
      // 4.8.10.8
      ScheduleTimeupdateEvent(false);
      ScheduleOwnEvent(base::Tokens::waiting());
    }
  }

  if (ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata &&
      old_state < WebMediaPlayer::kReadyStateHaveMetadata) {
    duration_ = player_->GetDuration();
#if SB_HAS(PLAYER_WITH_URL)
    start_date_ = player_->GetStartDate();
#endif  // SB_HAS(PLAYER_WITH_URL)
    ScheduleOwnEvent(base::Tokens::durationchange());
    ScheduleOwnEvent(base::Tokens::loadedmetadata());
  }

  if (ready_state_ >= WebMediaPlayer::kReadyStateHaveCurrentData &&
      old_state < WebMediaPlayer::kReadyStateHaveCurrentData &&
      !have_fired_loaded_data_) {
    have_fired_loaded_data_ = true;
    ScheduleOwnEvent(base::Tokens::loadeddata());
  }

  bool is_potentially_playing = PotentiallyPlaying();
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveFutureData &&
      old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
    ScheduleOwnEvent(base::Tokens::canplay());
    if (is_potentially_playing) {
      ScheduleOwnEvent(base::Tokens::playing());
    }
  }

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveEnoughData &&
      old_state < WebMediaPlayer::kReadyStateHaveEnoughData) {
    if (old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleOwnEvent(base::Tokens::canplay());
    }

    ScheduleOwnEvent(base::Tokens::canplaythrough());

    if (is_potentially_playing &&
        old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleOwnEvent(base::Tokens::playing());
    }

    if (autoplaying_ && paused_ && autoplay()) {
      paused_ = false;
      ScheduleOwnEvent(base::Tokens::play());
      ScheduleOwnEvent(base::Tokens::playing());
    }
  }

  UpdatePlayState();
}

void HTMLMediaElement::SetNetworkState(WebMediaPlayer::NetworkState state) {
  LOG(INFO) << "Set network state from " << network_state_ << " to " << state
            << ".";
  switch (state) {
    case WebMediaPlayer::kNetworkStateEmpty:
      // Just update the cached state and leave, we can't do anything.
      network_state_ = kNetworkEmpty;
      break;
    case WebMediaPlayer::kNetworkStateIdle:
      if (network_state_ > kNetworkIdle) {
        ChangeNetworkStateFromLoadingToIdle();
      } else {
        network_state_ = kNetworkIdle;
      }
      break;
    case WebMediaPlayer::kNetworkStateLoading:
      if (network_state_ < kNetworkLoading ||
          network_state_ == kNetworkNoSource) {
        StartProgressEventTimer();
      }
      network_state_ = kNetworkLoading;
      break;
    case WebMediaPlayer::kNetworkStateLoaded:
      if (network_state_ != kNetworkIdle) {
        ChangeNetworkStateFromLoadingToIdle();
      }
      break;
    case WebMediaPlayer::kNetworkStateFormatError:
    case WebMediaPlayer::kNetworkStateNetworkError:
    case WebMediaPlayer::kNetworkStateDecodeError:
    case WebMediaPlayer::kNetworkStateCapabilityChangedError:
      NOTREACHED() << "Passed SetNetworkState an error state";
      break;
  }
}

void HTMLMediaElement::SetNetworkError(WebMediaPlayer::NetworkState state,
                                       const std::string& message) {
  LOG(ERROR) << "Set network error " << state << " with message \"" << message
             << "\".";
  switch (state) {
    case WebMediaPlayer::kNetworkStateFormatError:
    case WebMediaPlayer::kNetworkStateNetworkError:
    case WebMediaPlayer::kNetworkStateDecodeError:
    case WebMediaPlayer::kNetworkStateCapabilityChangedError:
      MediaLoadingFailed(state, message);
      break;
    case WebMediaPlayer::kNetworkStateEmpty:
    case WebMediaPlayer::kNetworkStateIdle:
    case WebMediaPlayer::kNetworkStateLoading:
    case WebMediaPlayer::kNetworkStateLoaded:
      NOTREACHED() << "Passed SetNetworkError a non-error state";
      break;
  }
}

void HTMLMediaElement::ChangeNetworkStateFromLoadingToIdle() {
  progress_event_timer_.Stop();

  // Schedule one last progress event so we guarantee that at least one is fired
  // for files that load very quickly.
  ScheduleOwnEvent(base::Tokens::progress());
  ScheduleOwnEvent(base::Tokens::suspend());
  network_state_ = kNetworkIdle;
}

void HTMLMediaElement::Seek(double time) {
  LOG(INFO) << "Seek to " << time << ".";
  // 4.8.9.9 Seeking - continued from set_current_time().

  // Check state again as Seek() can be called by HTMLMediaElement internally.
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing || !player_) {
    return;
  }

  // Get the current time before setting seeking_, last_seek_time_ is returned
  // once it is set.
  const double now = current_time(NULL);

  // 2 - If the element's seeking IDL attribute is true, then another instance
  // of this algorithm is already running. Abort that other instance of the
  // algorithm without waiting for the step that it is running to complete.
  // Nothing specific to be done here.

  // 3 - Set the seeking IDL attribute to true.
  // The flag will be cleared when the engine tells us the time has actually
  // changed.
  seeking_ = true;

  // 5 - If the new playback position is later than the end of the media
  // resource, then let it be the end of the media resource instead.
  time = std::min(time, duration());

  // 6 - If the new playback position is less than the earliest possible
  // position, let it be that position instead.
  time = std::max(time, 0.0);

  // 7 - If the (possibly now changed) new playback position is not in one of
  // the ranges given in the seekable attribute, then let it be the position in
  // one of the ranges given in the seekable attribute that is the nearest to
  // the new playback position. ... If there are no ranges given in the seekable
  // attribute then set the seeking IDL attribute to false and abort these
  // steps.
  scoped_refptr<TimeRanges> seekable_ranges = seekable();

  // Short circuit seeking to the current time by just firing the events if no
  // seek is required. Don't skip calling the media engine if we are in poster
  // mode because a seek should always cancel poster display.
  bool no_seek_required = seekable_ranges->length() == 0 || time == now;

  // Always notify the media engine of a seek if the source is not closed. This
  // ensures that the source is always in a flushed state when the 'seeking'
  // event fires.
  if (media_source_ &&
      media_source_->ready_state() != kMediaSourceReadyStateClosed) {
    no_seek_required = false;
  }

  if (no_seek_required) {
    if (time == now) {
      ScheduleOwnEvent(base::Tokens::seeking());
      ScheduleTimeupdateEvent(false);
      ScheduleOwnEvent(base::Tokens::seeked());
    }
    seeking_ = false;
    return;
  }
  time = seekable_ranges->Nearest(time);

  if (playing_) {
    if (last_seek_time_ < now) {
      AddPlayedRange(last_seek_time_, now);
    }
  }
  last_seek_time_ = time;
  sent_end_event_ = false;

  // 8 - Set the current playback position to the given new playback position
  player_->Seek(time);

  // 9 - Queue a task to fire a simple event named seeking at the element.
  ScheduleOwnEvent(base::Tokens::seeking());

  // 10 - Queue a task to fire a simple event named timeupdate at the element.
  ScheduleTimeupdateEvent(false);

  // 11-15 are handled, if necessary, when the engine signals a readystate
  // change.
}

void HTMLMediaElement::FinishSeek() {
  LOG(INFO) << "Finish seek.";
  // 4.8.10.9 Seeking step 14
  seeking_ = false;

  // 4.8.10.9 Seeking step 15
  ScheduleOwnEvent(base::Tokens::seeked());
}

void HTMLMediaElement::AddPlayedRange(double start, double end) {
  if (!played_time_ranges_) {
    played_time_ranges_ = new TimeRanges;
  }
  played_time_ranges_->Add(start, end);
}

void HTMLMediaElement::UpdateVolume() {
  if (!player_) {
    return;
  }

  if (!ProcessingMediaPlayerCallback()) {
    player_->SetVolume(muted_ ? 0 : volume_);
  }
}

void HTMLMediaElement::UpdatePlayState() {
  if (!player_) {
    return;
  }

  bool should_be_playing = PotentiallyPlaying();
  bool player_paused = player_->IsPaused();

  if (should_be_playing) {
    if (player_paused) {
      // Set rate, muted before calling play in case they were set before the
      // media engine was setup. The media engine should just stash the rate and
      // muted values since it isn't already playing.
      player_->SetRate(
          playback_rate_ *
          html_element_context()->video_playback_rate_multiplier());
      player_->SetVolume(muted_ ? 0 : volume_);

      player_->Play();
    }

    StartPlaybackProgressTimer();
    playing_ = true;
    if (AsHTMLVideoElement().get() != NULL) {
      dom_stat_tracker_->OnHtmlVideoElementPlaying();
    }
  } else {  // Should not be playing right now.
    if (!player_paused) {
      player_->Pause();
    }

    playback_progress_timer_.Stop();
    playing_ = false;
    const double time = current_time(NULL);
    if (time > last_seek_time_) {
      AddPlayedRange(last_seek_time_, time);
    }
  }
}

bool HTMLMediaElement::PotentiallyPlaying() const {
  // "paused_to_buffer" means the media engine's rate is 0, but only because it
  // had to stop playing when it ran out of buffered data. A movie is this state
  // is "potentially playing", modulo the checks in CouldPlayIfEnoughData().
  bool paused_to_buffer =
      ready_state_maximum_ >= WebMediaPlayer::kReadyStateHaveFutureData &&
      ready_state_ < WebMediaPlayer::kReadyStateHaveFutureData;
  return (paused_to_buffer ||
          ready_state_ >= WebMediaPlayer::kReadyStateHaveFutureData) &&
         CouldPlayIfEnoughData();
}

bool HTMLMediaElement::EndedPlayback() const {
  const double dur = duration();
  if (!player_ || std::isnan(dur)) {
    return false;
  }

  // 4.8.10.8 Playing the media resource

  // A media element is said to have ended playback when the element's
  // readyState attribute is WebMediaPlayer::kReadyStateHaveMetadata or greater,
  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata) {
    return false;
  }

  // and the current playback position is the end of the media resource and the
  // direction of playback is forwards, Either the media element does not have a
  // loop attribute specified, or the media element has a current media
  // controller.
  const double now = current_time(NULL);
  if (playback_rate_ > 0.f) {
    return dur > 0.0 && now >= dur && !loop();
  }

  // or the current playback position is the earliest possible position and the
  // direction of playback is backwards
  if (playback_rate_ < 0.f) {
    return now <= 0.0;
  }

  return false;
}

bool HTMLMediaElement::StoppedDueToErrors() const {
  if (ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata && error_) {
    scoped_refptr<TimeRanges> seekable_ranges = seekable();
    if (!seekable_ranges->Contains(current_time(NULL))) {
      return true;
    }
  }
  return false;
}

bool HTMLMediaElement::CouldPlayIfEnoughData() const {
  return !paused() && !EndedPlayback() && !StoppedDueToErrors();
}

void HTMLMediaElement::ConfigureMediaControls() {
  DLOG_IF(WARNING, controls_) << "media control is not supported";
}

void HTMLMediaElement::MediaEngineError(scoped_refptr<MediaError> error) {
  if (error->message().empty()) {
    LOG(WARNING) << "HTMLMediaElement::MediaEngineError " << error->code();
  } else {
    LOG(WARNING) << "HTMLMediaElement::MediaEngineError " << error->code()
                 << " message: " << error->message();
  }

  // 1 - The user agent should cancel the fetching process.
  StopPeriodicTimers();
  load_state_ = kWaitingForSource;

  // 2 - Set the error attribute to a new MediaError object whose code attribute
  // is set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
  error_ = error;

  // 3 - Queue a task to fire a simple event named error at the media element.
  ScheduleOwnEvent(base::Tokens::error());

  ClearMediaSource();

  // 4 - Set the element's networkState attribute to the kNetworkEmpty value and
  // queue a task to fire a simple event called emptied at the element.
  network_state_ = kNetworkEmpty;
  ScheduleOwnEvent(base::Tokens::emptied());
}

void HTMLMediaElement::NetworkStateChanged() {
  DCHECK(player_);
  if (!player_) {
    return;
  }
  BeginProcessingMediaPlayerCallback();
  SetNetworkState(player_->GetNetworkState());
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::NetworkError(const std::string& message) {
  DCHECK(player_);
  if (!player_) {
    return;
  }
  BeginProcessingMediaPlayerCallback();
  SetNetworkError(player_->GetNetworkState(), message);
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::ReadyStateChanged() {
  DCHECK(player_);
  if (!player_) {
    return;
  }
  BeginProcessingMediaPlayerCallback();
  SetReadyState(player_->GetReadyState());
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::TimeChanged(bool eos_played) {
  DCHECK(player_);
  if (!player_) {
    return;
  }

  BeginProcessingMediaPlayerCallback();

  // 4.8.10.9 step 14 & 15.  Needed if no ReadyState change is associated with
  // the seek.
  if (seeking_ && ready_state_ >= WebMediaPlayer::kReadyStateHaveCurrentData &&
      !player_->IsSeeking()) {
    FinishSeek();
  }

  // Always call ScheduleTimeupdateEvent when the media engine reports a time
  // discontinuity, it will only queue a 'timeupdate' event if we haven't
  // already posted one at the current movie time.
  ScheduleTimeupdateEvent(false);

  const double now = current_time(NULL);
  const double dur = duration();

  // When the current playback position reaches the end of the media resource
  // when the direction of playback is forwards, then the user agent must follow
  // these steps:
  eos_played |=
      !std::isnan(dur) && (0.0 != dur) && now >= dur && playback_rate_ > 0.f;
  if (eos_played) {
    LOG(INFO) << "End of stream is played.";
    // If the media element has a loop attribute specified and does not have a
    // current media controller,
    if (loop()) {
      sent_end_event_ = false;
      // then seek to the earliest possible position of the media resource and
      // abort these steps.
      Seek(0.0);
    } else {
      // If the media element does not have a current media controller, and the
      // media element has still ended playback, and the direction of playback
      // is still forwards, and paused is false,
      if (!paused_) {
        // changes paused to true and fires a simple event named pause at the
        // media element.
        paused_ = true;
        ScheduleOwnEvent(base::Tokens::pause());
      }
      // Queue a task to fire a simple event named ended at the media element.
      if (!sent_end_event_) {
        sent_end_event_ = true;
        ScheduleOwnEvent(base::Tokens::ended());
      }
    }
  } else {
    sent_end_event_ = false;
  }

  UpdatePlayState();
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::DurationChanged() {
  BeginProcessingMediaPlayerCallback();

  ScheduleOwnEvent(base::Tokens::durationchange());

  const double now = current_time(NULL);
  // Reset and update |duration_|.
  duration_ = std::numeric_limits<double>::quiet_NaN();
  if (player_ && ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata) {
    duration_ = player_->GetDuration();
  }

  if (now > duration_) {
    Seek(duration_);
  }

  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::OutputModeChanged() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::OutputModeChanged()");
  // If the player mode is updated, trigger a re-layout so that we can setup
  // the video render tree differently depending on whether we are in punch-out
  // or decode-to-texture.
  node_document()->OnDOMMutation();
  InvalidateLayoutBoxesOfNodeAndAncestors();
}

void HTMLMediaElement::ContentSizeChanged() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::ContentSizeChanged()");
  // If the player content size is updated, trigger a re-layout so that we can
  // setup the video render tree differently with letterboxing.
  node_document()->OnDOMMutation();
  InvalidateLayoutBoxesOfNodeAndAncestors();
}

void HTMLMediaElement::PlaybackStateChanged() {
  if (!player_) {
    return;
  }

  BeginProcessingMediaPlayerCallback();
  if (player_->IsPaused()) {
    Pause();
  } else {
    Play();
  }
  EndProcessingMediaPlayerCallback();
}

float HTMLMediaElement::Volume() const { return muted_ ? 0 : volume(NULL); }

void HTMLMediaElement::SourceOpened(ChunkDemuxer* chunk_demuxer) {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::SourceOpened()");
  DCHECK(chunk_demuxer);
  BeginProcessingMediaPlayerCallback();
  DCHECK(media_source_);
  media_source_->SetChunkDemuxerAndOpen(chunk_demuxer);
  EndProcessingMediaPlayerCallback();
}

std::string HTMLMediaElement::SourceURL() const {
  return media_source_url_.spec();
}

std::string HTMLMediaElement::MaxVideoCapabilities() const {
  return max_video_capabilities_;
}

bool HTMLMediaElement::PreferDecodeToTexture() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::PreferDecodeToTexture()");

  if (!node_document()->window()->enable_map_to_mesh()) return false;

  cssom::PropertyValue* filter = NULL;
  if (node_document()->UpdateComputedStyleOnElementAndAncestor(this) &&
      computed_style()) {
    filter = computed_style()->filter();
  } else {
    LOG(WARNING) << "PreferDecodeToTexture() could not get the computed style "
                    "in order to know if map-to-mesh is applied to the "
                    "HTMLMediaElement or not. Perhaps the HTMLMediaElement is "
                    "not attached to the document?  Falling back to checking "
                    "the inline style instead.";
    if (style() && style()->data()) {
      filter = style()->data()->GetPropertyValue(cssom::kFilterProperty);
    }
  }

  if (!filter) {
    LOG(WARNING) << "PreferDecodeToTexture() could not find filter "
                    "property.";
    return false;
  }

  LOG(INFO) << "PreferDecodeToTexture() get filter property: "
            << filter->ToString() << ".";

  const cssom::MapToMeshFunction* map_to_mesh_filter =
      cssom::MapToMeshFunction::ExtractFromFilterList(filter);

  return map_to_mesh_filter;
}

// See https://www.w3.org/TR/encrypted-media/#initdata-encountered.
void HTMLMediaElement::EncryptedMediaInitDataEncountered(
    const char* init_data_type, const unsigned char* init_data,
    unsigned int init_data_length) {
  DCHECK(init_data_type);

  // 4. If the media data is CORS-same-origin and not mixed content.

  // 5. Queue a task to create an event named encrypted that does not bubble
  //    and is not cancellable using the MediaEncryptedEvent interface [...],
  //    and dispatch it at the media element.
  //
  // TODO: Implement Event.isTrusted as per
  //       https://www.w3.org/TR/dom/#dom-event-istrusted and set it to true.
  eme::MediaEncryptedEventInit media_encrypted_event_init;
  DCHECK(node_document()->location());
  std::string src = this->src();
  GURL current_url = GURL(src);
  if (current_url.is_empty()) {
    current_url = node_document()->location()->url().Resolve(src);
  }
  if (!current_url.SchemeIs("http") &&
      OriginIsSafe(request_mode_, current_url,
                   node_document()->location()->GetOriginAsObject())) {
    media_encrypted_event_init.set_init_data_type(init_data_type);
    auto* global_environment =
        html_element_context()->script_runner()->GetGlobalEnvironment();
    media_encrypted_event_init.set_init_data(
        script::ArrayBuffer::New(global_environment, init_data,
                                 init_data_length)
            .GetScriptValue());
  }
  auto* environment_settings = html_element_context()->environment_settings();
  event_queue_.Enqueue(new eme::MediaEncryptedEvent(
      environment_settings, "encrypted", media_encrypted_event_init));
}

void HTMLMediaElement::ClearMediaSource() {
  if (media_source_) {
    media_source_->Close();
    media_source_ = NULL;
  }
}

void HTMLMediaElement::SetMaxVideoCapabilities(
    const std::string& max_video_capabilities,
    script::ExceptionState* exception_state) {
  if (GetAttribute("src").value_or("").length() > 0) {
    LOG(WARNING) << "Cannot set maximum capabilities after src is defined.";
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  LOG(INFO) << "max video capabilities is changed from \""
            << max_video_capabilities_ << "\" to \"" << max_video_capabilities
            << "\"";
  max_video_capabilities_ = max_video_capabilities;
}

}  // namespace dom
}  // namespace cobalt
