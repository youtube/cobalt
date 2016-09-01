/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/html_media_element.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "cobalt/base/tokens.h"
#include "cobalt/base/user_log.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/media_key_complete_event.h"
#include "cobalt/dom/media_key_error_event.h"
#include "cobalt/dom/media_key_message_event.h"
#include "cobalt/dom/media_key_needed_event.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/fetcher_buffered_data_source.h"
#include "cobalt/media/web_media_player_factory.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/player/can_play_type.h"

namespace cobalt {
namespace dom {

using ::media::WebMediaPlayer;

const char HTMLMediaElement::kMediaSourceUrlProtocol[] = "blob";
const double HTMLMediaElement::kMaxTimeupdateEventFrequency = 0.25;

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

}  // namespace

HTMLMediaElement::HTMLMediaElement(Document* document, base::Token tag_name)
    : HTMLElement(document, tag_name),
      load_state_(kWaitingForSource),
      ALLOW_THIS_IN_INITIALIZER_LIST(event_queue_(this)),
      playback_rate_(1.0f),
      default_playback_rate_(1.0f),
      network_state_(kNetworkEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      ready_state_maximum_(WebMediaPlayer::kReadyStateHaveNothing),
      volume_(1.0f),
      last_seek_time_(0),
      previous_progress_time_(std::numeric_limits<double>::max()),
      playing_(false),
      have_fired_loaded_data_(false),
      autoplaying_(true),
      muted_(false),
      paused_(true),
      seeking_(false),
      loop_(false),
      controls_(false),
      last_time_update_event_wall_time_(0),
      last_time_update_event_movie_time_(std::numeric_limits<float>::max()),
      processing_media_player_callback_(0),
      media_source_url_(std::string(kMediaSourceUrlProtocol) + ':' +
                        base::GenerateGUID()),
      pending_load_(false),
      sent_stalled_event_(false),
      sent_end_event_(false) {
  html_media_element_count_log.Get().count++;
}

HTMLMediaElement::~HTMLMediaElement() {
  SetSourceState(MediaSource::kReadyStateClosed);
  html_media_element_count_log.Get().count--;
}

std::string HTMLMediaElement::src() const {
  return GetAttribute("src").value_or("");
}

void HTMLMediaElement::set_src(const std::string& src) {
  SetAttribute("src", src);
  ClearMediaPlayer();
  ScheduleLoad();
}

uint16_t HTMLMediaElement::network_state() const {
  return static_cast<uint16_t>(network_state_);
}

scoped_refptr<TimeRanges> HTMLMediaElement::buffered() const {
  scoped_refptr<TimeRanges> buffered = new TimeRanges;

  if (!player_) {
    return buffered;
  }

  const ::media::Ranges<base::TimeDelta>& player_buffered =
      player_->GetBufferedTimeRanges();

  for (int i = 0; i < static_cast<int>(player_buffered.size()); ++i) {
    buffered->Add(player_buffered.start(i).InSecondsF(),
                  player_buffered.end(i).InSecondsF());
  }

  return buffered;
}

void HTMLMediaElement::Load() {
  // LoadInternal may result in a 'beforeload' event, which can make arbitrary
  // DOM mutations.
  scoped_refptr<HTMLMediaElement> protect(this);

  PrepareForLoad();
  LoadInternal();
}

std::string HTMLMediaElement::CanPlayType(const std::string& mime_type) const {
  return CanPlayType(mime_type, "");
}

std::string HTMLMediaElement::CanPlayType(const std::string& mime_type,
                                          const std::string& key_system) const {
  std::string result = ::media::CanPlayType(mime_type, key_system);
  DLOG(INFO) << "HTMLMediaElement::canPlayType(" << mime_type << ", "
             << key_system << ") -> " << result;
  return result;
}

void HTMLMediaElement::GenerateKeyRequest(
    const std::string& key_system,
    const base::optional<scoped_refptr<Uint8Array> >& init_data,
    script::ExceptionState* exception_state) {
  // https://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-generatekeyrequest
  // 1. If the first argument is null, throw a SYNTAX_ERR.
  if (key_system.empty()) {
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }

  // 2. If networkState is NETWORK_EMPTY, throw an INVALID_STATE_ERR.
  if (network_state_ == kNetworkEmpty || !player_) {
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
    RaiseMediaKeyException(exception, exception_state);
  }
}

void HTMLMediaElement::AddKey(
    const std::string& key_system, const scoped_refptr<const Uint8Array>& key,
    const base::optional<scoped_refptr<Uint8Array> >& init_data,
    const base::optional<std::string>& session_id,
    script::ExceptionState* exception_state) {
  // https://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-addkey
  // 1. If the first or second argument is null, throw a SYNTAX_ERR.
  if (key_system.empty() || !key) {
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }

  // 2. If the second argument is an empty array, throw a TYPE_MISMATCH_ERR.
  if (!key->length()) {
    DOMException::Raise(DOMException::kTypeMismatchErr, exception_state);
    return;
  }

  // 3. If networkState is NETWORK_EMPTY, throw an INVALID_STATE_ERR.
  if (network_state_ == kNetworkEmpty || !player_) {
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
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }

  if (!player_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  // The rest is handled by WebMediaPlayer::CancelKeyRequest().
  WebMediaPlayer::MediaKeyException exception =
      player_->CancelKeyRequest(key_system, session_id.value_or(""));
  if (exception != WebMediaPlayer::kMediaKeyExceptionNoError) {
    RaiseMediaKeyException(exception, exception_state);
  }
}

WebMediaPlayer::ReadyState HTMLMediaElement::ready_state() const {
  return ready_state_;
}

bool HTMLMediaElement::seeking() const { return seeking_; }

float HTMLMediaElement::current_time(
    script::ExceptionState* exception_state) const {
  UNREFERENCED_PARAMETER(exception_state);

  if (!player_) {
    return 0;
  }

  if (seeking_) {
    return last_seek_time_;
  }

  return player_->GetCurrentTime();
}

void HTMLMediaElement::set_current_time(
    float time, script::ExceptionState* exception_state) {
  // 4.8.9.9 Seeking

  // 1 - If the media element's readyState is
  // WebMediaPlayer::kReadyStateHaveNothing, then raise an INVALID_STATE_ERR
  // exception.
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing || !player_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  Seek(time);
}

float HTMLMediaElement::duration() const {
  if (player_ && ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata) {
    return player_->GetDuration();
  }

  return std::numeric_limits<float>::quiet_NaN();
}

bool HTMLMediaElement::paused() const { return paused_; }

float HTMLMediaElement::default_playback_rate() const {
  return default_playback_rate_;
}

void HTMLMediaElement::set_default_playback_rate(float rate) {
  if (default_playback_rate_ != rate) {
    default_playback_rate_ = rate;
    ScheduleEvent(base::Tokens::ratechange());
  }
}

float HTMLMediaElement::playback_rate() const { return playback_rate_; }

void HTMLMediaElement::set_playback_rate(float rate) {
  if (playback_rate_ != rate) {
    playback_rate_ = rate;
    ScheduleEvent(base::Tokens::ratechange());
  }

  if (player_ && PotentiallyPlaying()) {
    player_->SetRate(rate);
  }
}

const scoped_refptr<TimeRanges>& HTMLMediaElement::played() {
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
    return new TimeRanges(0, player_->GetMaxTimeSeekable());
  }
  return new TimeRanges;
}

bool HTMLMediaElement::ended() const {
  // 4.8.10.8 Playing the media resource
  // The ended attribute must return true if the media element has ended
  // playback and the direction of playback is forwards, and false otherwise.
  return EndedPlayback() && playback_rate_ > 0;
}

bool HTMLMediaElement::autoplay() const { return HasAttribute("autoplay"); }

void HTMLMediaElement::set_autoplay(bool autoplay) {
  // The value of 'autoplay' is true when the 'autoplay' attribute is present.
  // The value of the attribute is irrelevant.
  if (autoplay) {
    SetAttribute("autoplay", "");
  } else {
    RemoveAttribute("autoplay");
  }
}

bool HTMLMediaElement::loop() const { return loop_; }

void HTMLMediaElement::set_loop(bool loop) { loop_ = loop; }

void HTMLMediaElement::Play() {
  // 4.8.10.9. Playing the media resource
  if (!player_ || network_state_ == kNetworkEmpty) {
    ScheduleLoad();
  }

  if (EndedPlayback()) {
    Seek(0);
  }

  if (paused_) {
    paused_ = false;
    ScheduleEvent(base::Tokens::play());

    if (ready_state_ <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleEvent(base::Tokens::waiting());
    } else if (ready_state_ >= WebMediaPlayer::kReadyStateHaveFutureData) {
      ScheduleEvent(base::Tokens::playing());
    }
  }
  autoplaying_ = false;

  UpdatePlayState();
}

void HTMLMediaElement::Pause() {
  // 4.8.10.9. Playing the media resource
  if (!player_ || network_state_ == kNetworkEmpty) {
    ScheduleLoad();
  }

  autoplaying_ = false;

  if (!paused_) {
    paused_ = true;
    ScheduleTimeupdateEvent(false);
    ScheduleEvent(base::Tokens::pause());
  }

  UpdatePlayState();
}

bool HTMLMediaElement::controls() const { return controls_; }

void HTMLMediaElement::set_controls(bool controls) {
  controls_ = controls;
  ConfigureMediaControls();
}

float HTMLMediaElement::volume(script::ExceptionState* exception_state) const {
  UNREFERENCED_PARAMETER(exception_state);

  return volume_;
}

void HTMLMediaElement::set_volume(float vol,
                                  script::ExceptionState* exception_state) {
  if (vol < 0.0f || vol > 1.0f) {
    DOMException::Raise(DOMException::kIndexSizeErr, exception_state);
    return;
  }

  if (volume_ != vol) {
    volume_ = vol;
    UpdateVolume();
    ScheduleEvent(base::Tokens::volumechange());
  }
}

bool HTMLMediaElement::muted() const { return muted_; }

void HTMLMediaElement::set_muted(bool muted) {
  if (muted_ != muted) {
    muted_ = muted;
    // Avoid recursion when the player reports volume changes.
    if (!ProcessingMediaPlayerCallback()) {
      if (player_) {
        player_->SetVolume(muted_ ? 0 : volume_);
      }
    }
    ScheduleEvent(base::Tokens::volumechange());
  }
}

void HTMLMediaElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();

  std::string src = GetAttribute("src").value_or("");
  if (!src.empty()) {
    set_src(src);
  }
}

void HTMLMediaElement::CreateMediaPlayer() {
  player_.reset();
  player_ =
      html_element_context()->web_media_player_factory()->CreateWebMediaPlayer(
          this);
  if (media_source_) {
    media_source_->SetPlayer(player_.get());
  }
  node_document()->OnDOMMutation();
}

void HTMLMediaElement::ScheduleLoad() {
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
    ScheduleEvent(base::Tokens::abort());
  }

  SetSourceState(MediaSource::kReadyStateClosed);

  CreateMediaPlayer();

  // 4 - If the media element's networkState is not set to kNetworkEmpty, then
  // run these substeps
  if (network_state_ != kNetworkEmpty) {
    network_state_ = kNetworkEmpty;
    ready_state_ = WebMediaPlayer::kReadyStateHaveNothing;
    ready_state_maximum_ = WebMediaPlayer::kReadyStateHaveNothing;
    paused_ = true;
    seeking_ = false;
    ScheduleEvent(base::Tokens::emptied());
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

  // 2 - Asynchronously await a stable state.
  played_time_ranges_ = new TimeRanges;
  last_seek_time_ = 0;

  ConfigureMediaControls();
}

void HTMLMediaElement::LoadInternal() {
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
  ScheduleEvent(base::Tokens::loadstart());

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
      media_source_->SetPlayer(player_.get());
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
  if (url.spec() == SourceURL()) {
    player_->LoadMediaSource();
  } else {
    csp::SecurityCallback csp_callback = base::Bind(
        &CspDelegate::CanLoad,
        base::Unretained(node_document()->csp_delegate()), CspDelegate::kMedia);
    player_->LoadProgressive(
        url, new media::FetcherBufferedDataSource(
                 base::MessageLoopProxy::current(), url, csp_callback,
                 html_element_context()->fetcher_factory()->network_module()),
        WebMediaPlayer::kCORSModeUnspecified);
  }
}

void HTMLMediaElement::ClearMediaPlayer() {
  SetSourceState(MediaSource::kReadyStateClosed);

  player_.reset(NULL);

  StopPeriodicTimers();
  load_timer_.Stop();

  pending_load_ = false;
  load_state_ = kWaitingForSource;

  if (node_document()) {
    node_document()->OnDOMMutation();
  }
}

void HTMLMediaElement::NoneSupported() {
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
  ScheduleEvent(base::Tokens::error());

  SetSourceState(MediaSource::kReadyStateClosed);
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
    ScheduleEvent(base::Tokens::progress());
    previous_progress_time_ = time;
    sent_stalled_event_ = false;
  } else if (time_delta > 3.0 && !sent_stalled_event_) {
    ScheduleEvent(base::Tokens::stalled());
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
    ScheduleEvent(base::Tokens::timeupdate());
    last_time_update_event_wall_time_ = now;
    last_time_update_event_movie_time_ = movie_time;
  }
}

void HTMLMediaElement::ScheduleEvent(base::Token event_name) {
  scoped_refptr<Event> event =
      new Event(event_name, Event::kNotBubbles, Event::kCancelable);
  event->set_target(this);

  event_queue_.Enqueue(event);
}

void HTMLMediaElement::CancelPendingEventsAndCallbacks() {
  event_queue_.CancelAllEvents();
}

void HTMLMediaElement::SetReadyState(WebMediaPlayer::ReadyState state) {
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
      ScheduleEvent(base::Tokens::waiting());
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
      ScheduleEvent(base::Tokens::waiting());
    }
  }

  if (ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata &&
      old_state < WebMediaPlayer::kReadyStateHaveMetadata) {
    ScheduleEvent(base::Tokens::durationchange());
    ScheduleEvent(base::Tokens::loadedmetadata());
  }

  if (ready_state_ >= WebMediaPlayer::kReadyStateHaveCurrentData &&
      old_state < WebMediaPlayer::kReadyStateHaveCurrentData &&
      !have_fired_loaded_data_) {
    have_fired_loaded_data_ = true;
    ScheduleEvent(base::Tokens::loadeddata());
  }

  bool is_potentially_playing = PotentiallyPlaying();
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveFutureData &&
      old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
    ScheduleEvent(base::Tokens::canplay());
    if (is_potentially_playing) {
      ScheduleEvent(base::Tokens::playing());
    }
  }

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveEnoughData &&
      old_state < WebMediaPlayer::kReadyStateHaveEnoughData) {
    if (old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleEvent(base::Tokens::canplay());
    }

    ScheduleEvent(base::Tokens::canplaythrough());

    if (is_potentially_playing &&
        old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleEvent(base::Tokens::playing());
    }

    if (autoplaying_ && paused_ && autoplay()) {
      paused_ = false;
      ScheduleEvent(base::Tokens::play());
      ScheduleEvent(base::Tokens::playing());
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
  ScheduleEvent(base::Tokens::progress());
  ScheduleEvent(base::Tokens::suspend());
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
      media_source_->GetReadyState() != MediaSource::kReadyStateClosed) {
    no_seek_required = false;
  }

  if (no_seek_required) {
    if (time == now) {
      ScheduleEvent(base::Tokens::seeking());
      ScheduleTimeupdateEvent(false);
      ScheduleEvent(base::Tokens::seeked());
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
  ScheduleEvent(base::Tokens::seeking());

  // 10 - Queue a task to fire a simple event named timeupdate at the element.
  ScheduleTimeupdateEvent(false);

  // 11-15 are handled, if necessary, when the engine signals a readystate
  // change.
}

void HTMLMediaElement::FinishSeek() {
  // 4.8.10.9 Seeking step 14
  seeking_ = false;

  // 4.8.10.9 Seeking step 15
  ScheduleEvent(base::Tokens::seeked());
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
      player_->SetRate(playback_rate_);
      player_->SetVolume(muted_ ? 0 : volume_);

      player_->Play();
    }

    StartPlaybackProgressTimer();
    playing_ = true;

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
  if (!player_ || isnan(dur)) {
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
  DLOG(WARNING) << "HTMLMediaElement::MediaEngineError " << error->code();

  // 1 - The user agent should cancel the fetching process.
  StopPeriodicTimers();
  load_state_ = kWaitingForSource;

  // 2 - Set the error attribute to a new MediaError object whose code attribute
  // is set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
  error_ = error;

  // 3 - Queue a task to fire a simple event named error at the media element.
  ScheduleEvent(base::Tokens::error());

  SetSourceState(MediaSource::kReadyStateClosed);

  // 4 - Set the element's networkState attribute to the kNetworkEmpty value and
  // queue a task to fire a simple event called emptied at the element.
  network_state_ = kNetworkEmpty;
  ScheduleEvent(base::Tokens::emptied());
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

void HTMLMediaElement::TimeChanged() {
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
  if (!isnan(dur) && (0.0f != dur) && now >= dur && playback_rate_ > 0) {
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
        ScheduleEvent(base::Tokens::pause());
      }
      // Queue a task to fire a simple event named ended at the media element.
      if (!sent_end_event_) {
        sent_end_event_ = true;
        ScheduleEvent(base::Tokens::ended());
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

  ScheduleEvent(base::Tokens::durationchange());

  float now = current_time(NULL);
  float dur = duration();
  if (now > dur) {
    Seek(dur);
  }

  EndProcessingMediaPlayerCallback();
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

void HTMLMediaElement::SourceOpened() {
  BeginProcessingMediaPlayerCallback();
  SetSourceState(MediaSource::kReadyStateOpen);
  EndProcessingMediaPlayerCallback();
}

std::string HTMLMediaElement::SourceURL() const {
  return media_source_url_.spec();
}

void HTMLMediaElement::KeyAdded(const std::string& key_system,
                                const std::string& session_id) {
  event_queue_.Enqueue(new MediaKeyCompleteEvent(key_system, session_id));
}

void HTMLMediaElement::KeyError(const std::string& key_system,
                                const std::string& session_id,
                                MediaKeyErrorCode error_code,
                                uint16 system_code) {
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
  event_queue_.Enqueue(new MediaKeyMessageEvent(
      key_system, session_id,
      new Uint8Array(NULL, message, message_length, NULL), default_url));
}

void HTMLMediaElement::KeyNeeded(const std::string& key_system,
                                 const std::string& session_id,
                                 const unsigned char* init_data,
                                 unsigned int init_data_length) {
  event_queue_.Enqueue(new MediaKeyNeededEvent(
      key_system, session_id,
      new Uint8Array(NULL, init_data, init_data_length, NULL)));
}

void HTMLMediaElement::SetSourceState(MediaSource::ReadyState ready_state) {
  if (!media_source_) {
    return;
  }
  media_source_->SetReadyState(ready_state);
  if (ready_state == MediaSource::kReadyStateClosed) {
    media_source_ = NULL;
  }
}

}  // namespace dom
}  // namespace cobalt
