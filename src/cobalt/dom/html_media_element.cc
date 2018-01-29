// Copyright 2015 Google Inc. All Rights Reserved.
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
#include <limits>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "cobalt/base/tokens.h"
#include "cobalt/base/user_log.h"
#include "cobalt/cssom/map_to_mesh_function.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_video_element.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/media_source_ready_state.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/fetcher_buffered_data_source.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/script_value_factory.h"
#include "starboard/double.h"

#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/dom/eme/media_encrypted_event.h"
#include "cobalt/dom/eme/media_encrypted_event_init.h"
#include "cobalt/media/base/media_log.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/dom/media_key_complete_event.h"
#include "cobalt/dom/media_key_error_event.h"
#include "cobalt/dom/media_key_message_event.h"
#include "cobalt/dom/media_key_needed_event.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace dom {

#if defined(COBALT_MEDIA_SOURCE_2016)
using media::BufferedDataSource;
using media::Ranges;
using media::WebMediaPlayer;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
using ::media::BufferedDataSource;
using ::media::Ranges;
using ::media::WebMediaPlayer;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

const char HTMLMediaElement::kMediaSourceUrlProtocol[] = "blob";
const double HTMLMediaElement::kMaxTimeupdateEventFrequency = 0.25;

namespace {

#define LOG_MEDIA_ELEMENT_ACTIVITIES 0

#if LOG_MEDIA_ELEMENT_ACTIVITIES

#define MLOG() LOG(INFO) << __FUNCTION__ << ": "

#else  // LOG_MEDIA_ELEMENT_ACTIVITIES

#define MLOG() EAT_STREAM_PARAMETERS

#endif  // LOG_MEDIA_ELEMENT_ACTIVITIES

// This struct manages the user log information for HTMLMediaElement count.
struct HTMLMediaElementCountLog {
  HTMLMediaElementCountLog() : count(0) {
    base::UserLog::Register(base::UserLog::kHTMLMediaElementCountIndex,
                            "MediaElementCnt", &count, sizeof(count));
  }
  ~HTMLMediaElementCountLog() {
    base::UserLog::Deregister(base::UserLog::kHTMLMediaElementCountIndex);
  }

  int count;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTMLMediaElementCountLog);
};

base::LazyInstance<HTMLMediaElementCountLog> html_media_element_count_log =
    LAZY_INSTANCE_INITIALIZER;

#if !SB_HAS(PLAYER_WITH_URL)
loader::RequestMode GetRequestMode(
    const base::optional<std::string>& cross_origin_attribute) {
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
#endif  // SB_HAS(PLAYER_WITH_URL)

#if defined(COBALT_MEDIA_SOURCE_2016)
bool OriginIsSafe(loader::RequestMode request_mode, const GURL& resource_url,
                  const loader::Origin& origin) {
#if SB_HAS(PLAYER_WITH_URL)
  UNREFERENCED_PARAMETER(request_mode);
  UNREFERENCED_PARAMETER(resource_url);
  UNREFERENCED_PARAMETER(origin);
  return true;
#else  // SB_HAS(PLAYER_WITH_URL)
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
#endif  // SB_HAS(PLAYER_WITH_URL)
}
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

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
      last_seek_time_(0),
      previous_progress_time_(std::numeric_limits<double>::max()),
      duration_(std::numeric_limits<double>::quiet_NaN()),
      playing_(false),
      have_fired_loaded_data_(false),
      autoplaying_(true),
      muted_(false),
      paused_(true),
      seeking_(false),
      controls_(false),
      last_time_update_event_wall_time_(0),
      last_time_update_event_movie_time_(std::numeric_limits<float>::max()),
      processing_media_player_callback_(0),
      media_source_url_(std::string(kMediaSourceUrlProtocol) + ':' +
                        base::GenerateGUID()),
      pending_load_(false),
      sent_stalled_event_(false),
      sent_end_event_(false),
      request_mode_(loader::kNoCORSMode) {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::HTMLMediaElement()");
  MLOG();
  html_media_element_count_log.Get().count++;
}

HTMLMediaElement::~HTMLMediaElement() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::~HTMLMediaElement()");
  MLOG();
  ClearMediaSource();
  html_media_element_count_log.Get().count--;
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
  MLOG() << src;
  SetAttribute("src", src);
  ClearMediaPlayer();
  ScheduleLoad();
}

base::optional<std::string> HTMLMediaElement::cross_origin() const {
  base::optional<std::string> cross_origin_attribute =
      GetAttribute("crossOrigin");
  if (cross_origin_attribute &&
      (*cross_origin_attribute != "anonymous" &&
       *cross_origin_attribute != "use-credentials")) {
    return std::string();
  }
  return cross_origin_attribute;
}

void HTMLMediaElement::set_cross_origin(
    const base::optional<std::string>& value) {
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
    MLOG() << "(empty)";
    return buffered;
  }

  const Ranges<base::TimeDelta>& player_buffered =
      player_->GetBufferedTimeRanges();

  MLOG() << "================================";
  for (int i = 0; i < static_cast<int>(player_buffered.size()); ++i) {
    MLOG() << player_buffered.start(i).InSecondsF() << " - "
           << player_buffered.end(i).InSecondsF();
    buffered->Add(player_buffered.start(i).InSecondsF(),
                  player_buffered.end(i).InSecondsF());
  }

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
  DCHECK(html_element_context()->can_play_type_handler());

#if defined(COBALT_MEDIA_SOURCE_2016)
  DLOG_IF(ERROR, !key_system.empty())
      << "CanPlayType() only accepts one parameter but (" << key_system
      << ") is passed as a second parameter.";
  std::string result =
      html_element_context()->can_play_type_handler()->CanPlayType(mime_type,
                                                                   "");
  MLOG() << "(" << mime_type << ") => " << result;
  LOG(INFO) << "HTMLMediaElement::canPlayType(" << mime_type << ") -> "
            << result;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  std::string result =
      html_element_context()->can_play_type_handler()->CanPlayType(mime_type,
                                                                   key_system);
  MLOG() << "(" << mime_type << ", " << key_system << ") => " << result;
  LOG(INFO) << "HTMLMediaElement::canPlayType(" << mime_type << ", "
            << key_system << ") -> " << result;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
  return result;
}

#if defined(COBALT_MEDIA_SOURCE_2016)

const EventTarget::EventListenerScriptValue* HTMLMediaElement::onencrypted()
    const {
  return GetAttributeEventListener(base::Tokens::encrypted());
}

void HTMLMediaElement::set_onencrypted(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::encrypted(), event_listener);
}

// See https://www.w3.org/TR/encrypted-media/#dom-htmlmediaelement-setmediakeys.
scoped_ptr<HTMLMediaElement::VoidPromiseValue> HTMLMediaElement::SetMediaKeys(
    const scoped_refptr<eme::MediaKeys>& media_keys) {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::SetMediaKeys()");

  scoped_ptr<VoidPromiseValue> promise = node_document()
                                             ->html_element_context()
                                             ->script_value_factory()
                                             ->CreateBasicPromise<void>();
  VoidPromiseValue::StrongReference promise_reference(*promise);

  // 1. If mediaKeys and the mediaKeys attribute are the same object, return
  //    a resolved promise.
  if (media_keys_ == media_keys) {
    promise_reference.value().Resolve();
    return promise.Pass();
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
  promise_reference.value().Resolve();

  // 6. Return promise.
  return promise.Pass();
}

#else  // defined(COBALT_MEDIA_SOURCE_2016)

namespace {

void RaiseMediaKeyException(WebMediaPlayer::MediaKeyException exception,
                            script::ExceptionState* exception_state) {
  DCHECK_NE(exception, WebMediaPlayer::kMediaKeyExceptionNoError);
  switch (exception) {
    case WebMediaPlayer::kMediaKeyExceptionInvalidPlayerState:
      DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
      break;
    case WebMediaPlayer::kMediaKeyExceptionKeySystemNotSupported:
      DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
      break;
    case WebMediaPlayer::kMediaKeyExceptionNoError:
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

void HTMLMediaElement::GenerateKeyRequest(
    const std::string& key_system,
    const base::optional<scoped_refptr<Uint8Array> >& init_data,
    script::ExceptionState* exception_state) {
  MLOG() << key_system;
  // https://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-generatekeyrequest
  // 1. If the first argument is null, throw a SYNTAX_ERR.
  if (key_system.empty()) {
    MLOG() << "syntax error";
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }

  // 2. If networkState is NETWORK_EMPTY, throw an INVALID_STATE_ERR.
  if (network_state_ == kNetworkEmpty || !player_) {
    MLOG() << "invalid state error";
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  // The rest is handled by WebMediaPlayer::GenerateKeyRequest().
  WebMediaPlayer::MediaKeyException exception;

  if (init_data) {
    scoped_refptr<const Uint8Array> const_init_data = init_data.value();
    exception = player_->GenerateKeyRequest(key_system, const_init_data->data(),
                                            const_init_data->length());
  } else {
    exception = player_->GenerateKeyRequest(key_system, NULL, 0);
  }

  if (exception != WebMediaPlayer::kMediaKeyExceptionNoError) {
    MLOG() << "exception: " << exception;
    RaiseMediaKeyException(exception, exception_state);
  }
}

void HTMLMediaElement::AddKey(
    const std::string& key_system, const scoped_refptr<const Uint8Array>& key,
    const base::optional<scoped_refptr<Uint8Array> >& init_data,
    const base::optional<std::string>& session_id,
    script::ExceptionState* exception_state) {
  MLOG() << key_system;
  // https://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-addkey
  // 1. If the first or second argument is null, throw a SYNTAX_ERR.
  if (key_system.empty() || !key) {
    MLOG() << "syntax error";
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }

  // 2. If the second argument is an empty array, throw a TYPE_MISMATCH_ERR.
  if (!key->length()) {
    MLOG() << "type mismatch error";
    DOMException::Raise(DOMException::kTypeMismatchErr, exception_state);
    return;
  }

  // 3. If networkState is NETWORK_EMPTY, throw an INVALID_STATE_ERR.
  if (network_state_ == kNetworkEmpty || !player_) {
    MLOG() << "invalid state error";
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  // The rest is handled by WebMediaPlayer::AddKey().
  WebMediaPlayer::MediaKeyException exception;

  if (init_data) {
    scoped_refptr<const Uint8Array> const_init_data = init_data.value();
    exception = player_->AddKey(
        key_system, key->data(), key->length(), const_init_data->data(),
        const_init_data->length(), session_id.value_or(""));
  } else {
    exception = player_->AddKey(key_system, key->data(), key->length(), NULL, 0,
                                session_id.value_or(""));
  }

  if (exception != WebMediaPlayer::kMediaKeyExceptionNoError) {
    MLOG() << "exception: " << exception;
    RaiseMediaKeyException(exception, exception_state);
  }
}

void HTMLMediaElement::CancelKeyRequest(
    const std::string& key_system,
    const base::optional<std::string>& session_id,
    script::ExceptionState* exception_state) {
  // https://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-addkey
  // 1. If the first argument is null, throw a SYNTAX_ERR.
  if (key_system.empty()) {
    MLOG() << "syntax error";
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }

  if (!player_) {
    MLOG() << "invalid state error";
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  // The rest is handled by WebMediaPlayer::CancelKeyRequest().
  WebMediaPlayer::MediaKeyException exception =
      player_->CancelKeyRequest(key_system, session_id.value_or(""));
  if (exception != WebMediaPlayer::kMediaKeyExceptionNoError) {
    MLOG() << "exception: " << exception;
    RaiseMediaKeyException(exception, exception_state);
  }
}

#endif  // defined(COBALT_MEDIA_SOURCE_2016)

uint16_t HTMLMediaElement::ready_state() const {
  MLOG() << ready_state_;
  return static_cast<uint16_t>(ready_state_);
}

bool HTMLMediaElement::seeking() const {
  MLOG() << seeking_;
  return seeking_;
}

float HTMLMediaElement::current_time(
    script::ExceptionState* exception_state) const {
  UNREFERENCED_PARAMETER(exception_state);

  if (!player_) {
    MLOG() << 0 << " (because player is NULL)";
    return 0;
  }

  if (seeking_) {
    MLOG() << last_seek_time_ << " (seeking)";
    return last_seek_time_;
  }

  float time = player_->GetCurrentTime();
  MLOG() << time << " (from player)";
  return time;
}

void HTMLMediaElement::set_current_time(
    float time, script::ExceptionState* exception_state) {
  // 4.8.9.9 Seeking

  // 1 - If the media element's readyState is
  // WebMediaPlayer::kReadyStateHaveNothing, then raise an INVALID_STATE_ERR
  // exception.
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing || !player_) {
    MLOG() << "invalid state error";
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  MLOG() << "seek to " << time;
  Seek(time);
}

float HTMLMediaElement::duration() const {
  MLOG() << duration_;
  // TODO: Turn duration into double.
  return static_cast<float>(duration_);
}

base::Time HTMLMediaElement::GetStartDate() const {
  MLOG() << start_date_.ToSbTime();
  return start_date_;
}

bool HTMLMediaElement::paused() const {
  MLOG() << paused_;
  return paused_;
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
  MLOG() << rate;

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
    float time = current_time(NULL);
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
  if (player_ && player_->GetMaxTimeSeekable() != 0) {
    double max_time_seekable = player_->GetMaxTimeSeekable();
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
  MLOG() << autoplay;
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
  MLOG();
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
  MLOG();
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
  UNREFERENCED_PARAMETER(exception_state);
  MLOG() << volume_;
  return volume_;
}

void HTMLMediaElement::set_volume(float volume,
                                  script::ExceptionState* exception_state) {
  MLOG() << volume;
  if (volume < 0.0f || volume > 1.0f) {
    DOMException::Raise(DOMException::kIndexSizeErr, exception_state);
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
  MLOG() << muted;
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
}

void HTMLMediaElement::TraceMembers(script::Tracer* tracer) {
  HTMLElement::TraceMembers(tracer);

  tracer->Trace(event_queue_);
  tracer->Trace(played_time_ranges_);
  tracer->Trace(media_source_);
  tracer->Trace(error_);
#if defined(COBALT_MEDIA_SOURCE_2016)
  tracer->Trace(media_keys_);
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
}

#if defined(COBALT_MEDIA_SOURCE_2016)
void HTMLMediaElement::DurationChanged(double duration, bool request_seek) {
  MLOG() << "DurationChanged(" << duration << ", " << request_seek << ")";

  // Abort if duration unchanged.
  if (duration_ == duration) {
    return;
  }
  duration_ = duration;

  ScheduleOwnEvent(base::Tokens::durationchange());

  if (request_seek) {
    Seek(static_cast<float>(duration));
  }
}
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

void HTMLMediaElement::ScheduleEvent(const scoped_refptr<Event>& event) {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::ScheduleEvent()");
  MLOG() << event->type();
  event_queue_.Enqueue(event);
}

void HTMLMediaElement::CreateMediaPlayer() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::CreateMediaPlayer()");
  MLOG();
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
    DLOG(ERROR) << "Media playback in PRELOADING is not supported.";
    return;
  }

  player_ =
      html_element_context()->web_media_player_factory()->CreateWebMediaPlayer(
          this);
#if defined(COBALT_MEDIA_SOURCE_2016)
  if (media_keys_) {
    player_->SetDrmSystem(media_keys_->drm_system());
  }
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  if (media_source_) {
    media_source_->SetPlayer(player_.get());
  }
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
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
  last_seek_time_ = 0;

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
      media_url = node_document()->url_as_gurl().Resolve(src);
    }
    if (media_url.is_empty()) {
      MediaLoadingFailed(WebMediaPlayer::kNetworkStateFormatError);
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
  DCHECK(player_);
  if (!player_) {
    return;
  }

  GURL url = initial_url;

  if (!url.is_valid()) {
    // Try to filter out invalid urls as GURL::spec() DCHECKs if the url is
    // valid.
    NoneSupported();
    return;
  }

  DCHECK(!media_source_);
  if (url.SchemeIs(kMediaSourceUrlProtocol)) {
    // Check whether url is allowed by security policy.
    if (!node_document()->csp_delegate()->CanLoad(CspDelegate::kMedia, url,
                                                  false)) {
      DLOG(INFO) << "URL " << url << " is rejected by security policy.";
      NoneSupported();
      return;
    }

    media_source_ =
        html_element_context()->media_source_registry()->Retrieve(url.spec());
    // If media_source_ is NULL, the player will try to load it as a normal
    // media resource url and throw a DOM exception when it fails.
    if (media_source_) {
#if defined(COBALT_MEDIA_SOURCE_2016)
      media_source_->AttachToElement(this);
#else   // defined(COBALT_MEDIA_SOURCE_2016)
      media_source_->SetPlayer(player_.get());
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
      media_source_url_ = url;
    } else {
      NoneSupported();
      return;
    }
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
  // TODO: Investigate if we have to support csp and sop for url player.
  player_->LoadUrl(url);
#else   // SB_HAS(PLAYER_WITH_URL)
  if (url.spec() == SourceURL()) {
    player_->LoadMediaSource();
  } else {
    csp::SecurityCallback csp_callback = base::Bind(
        &CspDelegate::CanLoad,
        base::Unretained(node_document()->csp_delegate()), CspDelegate::kMedia);
    request_mode_ = GetRequestMode(GetAttribute("crossOrigin"));
    DCHECK(node_document()->location());
    scoped_ptr<BufferedDataSource> data_source(
        new media::FetcherBufferedDataSource(
            base::MessageLoopProxy::current(), url, csp_callback,
            html_element_context()->fetcher_factory()->network_module(),
            request_mode_, node_document()->location()->GetOriginAsObject()));
    player_->LoadProgressive(url, data_source.Pass());
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
}

void HTMLMediaElement::ClearMediaPlayer() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::ClearMediaPlayer()");
  MLOG();

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

void HTMLMediaElement::NoneSupported() {
  MLOG();

  DLOG(WARNING) << "HTMLMediaElement::NoneSupported() error.";

  StopPeriodicTimers();
  load_state_ = kWaitingForSource;

  // 4.8.10.5
  // 6 - Reaching this step indicates that the media resource failed to load or
  // that the given URL could not be resolved. In one atomic operation, run the
  // following steps:

  // 6.1 - Set the error attribute to a new MediaError object whose code
  // attribute is set to MEDIA_ERR_SRC_NOT_SUPPORTED.
  error_ = new MediaError(MediaError::kMediaErrSrcNotSupported);

  // 6.2 - Forget the media element's media-resource-specific text tracks.

  // 6.3 - Set the element's networkState attribute to the kNetworkNoSource
  // value.
  network_state_ = kNetworkNoSource;

  // 7 - Queue a task to fire a simple event named error at the media element.
  ScheduleOwnEvent(base::Tokens::error());

  ClearMediaSource();
}

void HTMLMediaElement::MediaLoadingFailed(WebMediaPlayer::NetworkState error) {
  StopPeriodicTimers();

  if (error == WebMediaPlayer::kNetworkStateNetworkError &&
      ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata) {
    MediaEngineError(new MediaError(MediaError::kMediaErrNetwork));
  } else if (error == WebMediaPlayer::kNetworkStateDecodeError) {
    MediaEngineError(new MediaError(MediaError::kMediaErrDecode));
  } else if ((error == WebMediaPlayer::kNetworkStateFormatError ||
              error == WebMediaPlayer::kNetworkStateNetworkError) &&
             load_state_ == kLoadingFromSrcAttr) {
    NoneSupported();
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

  double time = base::Time::Now().ToDoubleT();
  double time_delta = time - previous_progress_time_;

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
  playback_progress_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(
                     static_cast<int64>(kMaxTimeupdateEventFrequency * 1000)),
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
  double now = base::Time::Now().ToDoubleT();
  double time_delta = now - last_time_update_event_wall_time_;

  // throttle the periodic events
  if (periodic_event && time_delta < kMaxTimeupdateEventFrequency) {
    return;
  }

  // Some media engines make multiple "time changed" callbacks at the same time,
  // but we only want one event at a given time so filter here
  float movie_time = current_time(NULL);
  if (movie_time != last_time_update_event_movie_time_) {
    ScheduleOwnEvent(base::Tokens::timeupdate());
    last_time_update_event_wall_time_ = now;
    last_time_update_event_movie_time_ = movie_time;
  }
}

void HTMLMediaElement::ScheduleOwnEvent(base::Token event_name) {
  LOG_IF(INFO, event_name == base::Tokens::error())
      << "onerror event fired with error " << (error_ ? error_->code() : 0);
  MLOG() << event_name;
  scoped_refptr<Event> event =
      new Event(event_name, Event::kNotBubbles, Event::kCancelable);
  event->set_target(this);

  ScheduleEvent(event);
}

void HTMLMediaElement::CancelPendingEventsAndCallbacks() {
  event_queue_.CancelAllEvents();
}

void HTMLMediaElement::SetReadyState(WebMediaPlayer::ReadyState state) {
  TRACE_EVENT1("cobalt::dom", "HTMLMediaElement::SetReadyState()", "state",
               state);
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
#endif
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
  if (state == WebMediaPlayer::kNetworkStateEmpty) {
    // Just update the cached state and leave, we can't do anything.
    network_state_ = kNetworkEmpty;
    return;
  }

  if (state == WebMediaPlayer::kNetworkStateFormatError ||
      state == WebMediaPlayer::kNetworkStateNetworkError ||
      state == WebMediaPlayer::kNetworkStateDecodeError) {
    MediaLoadingFailed(state);
    return;
  }

  if (state == WebMediaPlayer::kNetworkStateIdle) {
    if (network_state_ > kNetworkIdle) {
      ChangeNetworkStateFromLoadingToIdle();
    } else {
      network_state_ = kNetworkIdle;
    }
  }

  if (state == WebMediaPlayer::kNetworkStateLoading) {
    if (network_state_ < kNetworkLoading ||
        network_state_ == kNetworkNoSource) {
      StartProgressEventTimer();
    }
    network_state_ = kNetworkLoading;
  }

  if (state == WebMediaPlayer::kNetworkStateLoaded) {
    if (network_state_ != kNetworkIdle) {
      ChangeNetworkStateFromLoadingToIdle();
    }
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

void HTMLMediaElement::Seek(float time) {
  // 4.8.9.9 Seeking - continued from set_current_time().

  // Check state again as Seek() can be called by HTMLMediaElement internally.
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing || !player_) {
    return;
  }

  // Get the current time before setting seeking_, last_seek_time_ is returned
  // once it is set.
  float now = current_time(NULL);

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
  time = std::max(time, 0.f);

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
  time = static_cast<float>(seekable_ranges->Nearest(time));

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
  // 4.8.10.9 Seeking step 14
  seeking_ = false;

  // 4.8.10.9 Seeking step 15
  ScheduleOwnEvent(base::Tokens::seeked());
}

void HTMLMediaElement::AddPlayedRange(float start, float end) {
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
    if (AsHTMLVideoElement() != NULL) {
      dom_stat_tracker_->OnHtmlVideoElementPlaying();
    }
  } else {  // Should not be playing right now.
    if (!player_paused) {
      player_->Pause();
    }

    playback_progress_timer_.Stop();
    playing_ = false;
    float time = current_time(NULL);
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
  float dur = duration();
  if (!player_ || SbDoubleIsNan(dur)) {
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
  float now = current_time(NULL);
  if (playback_rate_ > 0) {
    return dur > 0 && now >= dur && !loop();
  }

  // or the current playback position is the earliest possible position and the
  // direction of playback is backwards
  if (playback_rate_ < 0) {
    return now <= 0;
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
  MLOG() << error->code();
  LOG(WARNING) << "HTMLMediaElement::MediaEngineError " << error->code();

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

  float now = current_time(NULL);
  float dur = duration();

  // When the current playback position reaches the end of the media resource
  // when the direction of playback is forwards, then the user agent must follow
  // these steps:
  eos_played |=
      !SbDoubleIsNan(dur) && (0.0f != dur) && now >= dur && playback_rate_ > 0;
  if (eos_played) {
    // If the media element has a loop attribute specified and does not have a
    // current media controller,
    if (loop()) {
      sent_end_event_ = false;
      // then seek to the earliest possible position of the media resource and
      // abort these steps.
      Seek(0);
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

  float now = current_time(NULL);
  float dur = duration();
  if (now > dur) {
    Seek(dur);
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

void HTMLMediaElement::SawUnsupportedTracks() { NOTIMPLEMENTED(); }

float HTMLMediaElement::Volume() const { return volume(NULL); }

void HTMLMediaElement::SourceOpened(ChunkDemuxer* chunk_demuxer) {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::SourceOpened()");
  DCHECK(chunk_demuxer);
  BeginProcessingMediaPlayerCallback();
#if defined(COBALT_MEDIA_SOURCE_2016)
  DCHECK(media_source_);
  media_source_->SetChunkDemuxerAndOpen(chunk_demuxer);
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  SetSourceState(kMediaSourceReadyStateOpen);
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
  EndProcessingMediaPlayerCallback();
}

std::string HTMLMediaElement::SourceURL() const {
  return media_source_url_.spec();
}

bool HTMLMediaElement::PreferDecodeToTexture() {
  TRACE_EVENT0("cobalt::dom", "HTMLMediaElement::PreferDecodeToTexture()");

#if defined(ENABLE_MAP_TO_MESH)
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
    return false;
  }

  const cssom::MapToMeshFunction* map_to_mesh_filter =
      cssom::MapToMeshFunction::ExtractFromFilterList(filter);

  return map_to_mesh_filter;
#else  // defined(ENABLE_MAP_TO_MESH)
  // If map-to-mesh is disabled, we never prefer decode-to-texture.
  return false;
#endif  // defined(ENABLE_MAP_TO_MESH)
}

#if defined(COBALT_MEDIA_SOURCE_2016)

namespace {

// Initialization data types are defined in
// https://www.w3.org/TR/eme-initdata-registry/#registry.
std::string ToInitDataTypeString(media::EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case media::kEmeInitDataTypeCenc:
      return "cenc";
    case media::kEmeInitDataTypeFairplay:
      return "fairplay";
    case media::kEmeInitDataTypeKeyIds:
      return "keyids";
    case media::kEmeInitDataTypeWebM:
      return "webm";
    default:
      LOG(WARNING) << "Unknown EME initialization data type.";
      return "";
  }
}

}  // namespace

// See https://www.w3.org/TR/encrypted-media/#initdata-encountered.
void HTMLMediaElement::EncryptedMediaInitDataEncountered(
    media::EmeInitDataType init_data_type, const unsigned char* init_data,
    unsigned int init_data_length) {
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
    current_url = node_document()->url_as_gurl().Resolve(src);
  }
  if (!current_url.SchemeIs("http") &&
      OriginIsSafe(request_mode_, current_url,
                   node_document()->location()->GetOriginAsObject())) {
    media_encrypted_event_init.set_init_data_type(
        ToInitDataTypeString(init_data_type));
    media_encrypted_event_init.set_init_data(
        new ArrayBuffer(NULL, init_data, init_data_length));
  }
  event_queue_.Enqueue(
      new eme::MediaEncryptedEvent("encrypted", media_encrypted_event_init));
}

#else  // defined(COBALT_MEDIA_SOURCE_2016)

void HTMLMediaElement::KeyAdded(const std::string& key_system,
                                const std::string& session_id) {
  MLOG() << key_system;
  event_queue_.Enqueue(new MediaKeyCompleteEvent(key_system, session_id));
}

void HTMLMediaElement::KeyError(const std::string& key_system,
                                const std::string& session_id,
                                MediaKeyErrorCode error_code,
                                uint16 system_code) {
  MLOG() << key_system;
  MediaKeyError::Code code;
  switch (error_code) {
    case kUnknownError:
      code = MediaKeyError::kMediaKeyerrUnknown;
      break;
    case kClientError:
      code = MediaKeyError::kMediaKeyerrClient;
      break;
    case kServiceError:
      code = MediaKeyError::kMediaKeyerrService;
      break;
    case kOutputError:
      code = MediaKeyError::kMediaKeyerrOutput;
      break;
    case kHardwareChangeError:
      code = MediaKeyError::kMediaKeyerrHardwarechange;
      break;
    case kDomainError:
      code = MediaKeyError::kMediaKeyerrDomain;
      break;
    default:
      NOTREACHED();
      code = MediaKeyError::kMediaKeyerrUnknown;
      break;
  }
  event_queue_.Enqueue(
      new MediaKeyErrorEvent(key_system, session_id, code, system_code));
}

void HTMLMediaElement::KeyMessage(const std::string& key_system,
                                  const std::string& session_id,
                                  const unsigned char* message,
                                  unsigned int message_length,
                                  const std::string& default_url) {
  MLOG() << key_system;
  event_queue_.Enqueue(new MediaKeyMessageEvent(
      key_system, session_id,
      new Uint8Array(NULL, message, message_length, NULL), default_url));
}

void HTMLMediaElement::KeyNeeded(const std::string& key_system,
                                 const std::string& session_id,
                                 const unsigned char* init_data,
                                 unsigned int init_data_length) {
  MLOG() << key_system;
  event_queue_.Enqueue(new MediaKeyNeededEvent(
      key_system, session_id,
      new Uint8Array(NULL, init_data, init_data_length, NULL)));
}

#endif  // defined(COBALT_MEDIA_SOURCE_2016)

void HTMLMediaElement::ClearMediaSource() {
#if defined(COBALT_MEDIA_SOURCE_2016)
  if (media_source_) {
    media_source_->Close();
    media_source_ = NULL;
  }
#else  // defined(COBALT_MEDIA_SOURCE_2016)
  SetSourceState(kMediaSourceReadyStateClosed);
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
}

#if !defined(COBALT_MEDIA_SOURCE_2016)
void HTMLMediaElement::SetSourceState(MediaSourceReadyState ready_state) {
  MLOG() << ready_state;
  if (!media_source_) {
    return;
  }
  media_source_->SetReadyState(ready_state);
  if (ready_state == kMediaSourceReadyStateClosed) {
    media_source_ = NULL;
  }
}
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

}  // namespace dom
}  // namespace cobalt
