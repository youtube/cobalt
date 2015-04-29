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
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/guid.h"
#include "cobalt/dom/event.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"

namespace cobalt {
namespace dom {

using ::media::WebMediaPlayer;

const char HTMLMediaElement::kTagName[] = "video";
const char HTMLMediaElement::kMediaSourceUrlProtocol[] = "blob";
const double HTMLMediaElement::kMaxTimeupdateEventFrequency = 0.25;

HTMLMediaElement::HTMLMediaElement(
    HTMLElementFactory* html_element_factory, cssom::CSSParser* css_parser,
    media::WebMediaPlayerFactory* web_media_player_factory)
    : HTMLElement(html_element_factory, css_parser),
      web_media_player_factory_(web_media_player_factory),
      load_state_(kWaitingForSource),
      async_event_queue_(this),
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
      autoplay_(false),
      loop_(false),
      controls_(false),
      last_time_update_event_wall_time_(0),
      last_time_update_event_movie_time_(std::numeric_limits<float>::max()),
      processing_media_player_callback_(0),
      cached_time_(-1.),
      cached_time_wall_clock_update_time_(0),
      minimum_wall_clock_time_to_cached_media_time_(0),
      pending_load_(false),
      sent_stalled_event_(false),
      sent_end_event_(false) {
  // TODO(***REMOVED***): Remove the following lines when the returned binding of
  //                  HTMLElement derived class is correct.
  static const int kLoadVideoHack = true;
  if (kLoadVideoHack) {
    src_ = "file:///cobalt/dom/testdata/media-element/avengers-2015.mp4";
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&HTMLMediaElement::Play, this));
  }
  std::string media_source_url = kMediaSourceUrlProtocol;
  media_source_url = media_source_url + ':' + base::GenerateGUID();
  media_source_url_ = GURL(media_source_url);
}

const std::string& HTMLMediaElement::tag_name() const {
  static const std::string kElementName(kTagName);
  return kElementName;
}

void HTMLMediaElement::set_src(const std::string& src) {
  src_ = src;
  ClearMediaPlayer();
  ScheduleLoad();
}

uint16_t HTMLMediaElement::network_state() const { return network_state_; }

scoped_refptr<TimeRanges> HTMLMediaElement::buffered() const {
  if (!player_) {
    return new TimeRanges;
  }
  return new TimeRanges;
  // TODO(***REMOVED***): Copy buffered over.
  // return player_->Buffered();
}

void HTMLMediaElement::Load() {
  // LoadInternal may result in a 'beforeload' event, which can make arbitrary
  // DOM mutations.
  scoped_refptr<HTMLMediaElement> protect(this);

  PrepareForLoad();
  LoadInternal();
}

std::string HTMLMediaElement::CanPlayType(const std::string& mime_type,
                                          const std::string& key_system) const {
  return "probably";
  /*  WebMediaPlayer::SupportsType support =
        WebMediaPlayer::supportsType(content_type(mime_type), key_system);
    std::string canPlay;

    // 4.8.10.3
    switch (support) {
      case WebMediaPlayer::IsNotSupported:
        canPlay = "";
        break;
      case WebMediaPlayer::MayBeSupported:
        canPlay = "maybe";
        break;
      case WebMediaPlayer::IsSupported:
        canPlay = "probably";
        break;
    }

    DLOG(INFO) << "HTMLMediaElement::canPlayType(" << mime_type << ", "
               << key_system << ") -> " << canPlay;

    return canPlay;*/
}

WebMediaPlayer::ReadyState HTMLMediaElement::ready_state() const {
  return ready_state_;
}

bool HTMLMediaElement::seeking() const { return seeking_; }

float HTMLMediaElement::current_time() const {
  if (!player_) {
    return 0;
  }

  if (seeking_) {
    return last_seek_time_;
  }

  if (cached_time_ != -1 && paused_) {
    return cached_time_;
  }

  // Is it too soon use a cached time?
  double now = base::Time::Now().ToDoubleT();
  double maximum_duration_to_cache_media_time = 0;

  if (maximum_duration_to_cache_media_time && cached_time_ != -1 && !paused_ &&
      now > minimum_wall_clock_time_to_cached_media_time_) {
    double wall_clock_delta = now - cached_time_wall_clock_update_time_;

    // Not too soon, use the cached time only if it hasn't expired.
    if (wall_clock_delta < maximum_duration_to_cache_media_time) {
      float adjusted_cache_time = static_cast<float>(
          cached_time_ + (playback_rate_ * wall_clock_delta));

      return adjusted_cache_time;
    }
  }

  RefreshCachedTime();

  return cached_time_;
}

void HTMLMediaElement::set_current_time(float time /*, ExceptionCode* ec*/) {
  ExceptionCode ec;
  Seek(time, &ec);
}

float HTMLMediaElement::duration() const {
  if (player_ && ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata) {
    return player_->Duration();
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
    ScheduleEvent("ratechange");
  }
}

float HTMLMediaElement::playback_rate() const { return playback_rate_; }

void HTMLMediaElement::set_playback_rate(float rate) {
  if (playback_rate_ != rate) {
    playback_rate_ = rate;
    InvalidateCachedTime();
    ScheduleEvent("ratechange");
  }

  if (player_ && PotentiallyPlaying()) {
    player_->SetRate(rate);
  }
}

const scoped_refptr<TimeRanges>& HTMLMediaElement::played() {
  if (playing_) {
    float time = current_time();
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
  if (player_ && player_->MaxTimeSeekable() != 0) {
    return new TimeRanges(0, player_->MaxTimeSeekable());
  }
  return new TimeRanges;
}

bool HTMLMediaElement::ended() const {
  // 4.8.10.8 Playing the media resource
  // The ended attribute must return true if the media element has ended
  // playback and the direction of playback is forwards, and false otherwise.
  return EndedPlayback() && playback_rate_ > 0;
}

bool HTMLMediaElement::autoplay() const { return autoplay_; }

void HTMLMediaElement::set_autoplay(bool autoplay) { autoplay_ = autoplay; }

bool HTMLMediaElement::loop() const { return loop_; }

void HTMLMediaElement::set_loop(bool loop) { loop_ = loop; }

void HTMLMediaElement::Play() {
  // 4.8.10.9. Playing the media resource
  if (!player_ || network_state_ == kNetworkEmpty) {
    ScheduleLoad();
  }

  if (EndedPlayback()) {
    ExceptionCode unused;
    Seek(0, &unused);
  }

  if (paused_) {
    paused_ = false;
    InvalidateCachedTime();
    ScheduleEvent("play");

    if (ready_state_ <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleEvent("waiting");
    } else if (ready_state_ >= WebMediaPlayer::kReadyStateHaveFutureData) {
      ScheduleEvent("playing");
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
    ScheduleEvent("pause");
  }

  UpdatePlayState();
}

bool HTMLMediaElement::controls() const { return controls_; }

void HTMLMediaElement::set_controls(bool controls) {
  controls_ = controls;
  ConfigureMediaControls();
}

float HTMLMediaElement::volume() const { return volume_; }

void HTMLMediaElement::set_volume(float vol /*, ExceptionCode* ec*/) {
  if (vol < 0.0f || vol > 1.0f) {
    // *ec = kIndexSizeErr;
    return;
  }

  if (volume_ != vol) {
    volume_ = vol;
    UpdateVolume();
    ScheduleEvent("volumechange");
  }
}

bool HTMLMediaElement::muted() const { return muted_; }

void HTMLMediaElement::set_muted(bool muted) {
  if (muted_ != muted) {
    muted_ = muted;
    // Avoid recursion when the player reports volume changes.
    if (!ProcessingMediaPlayerCallback()) {
      if (player_) {
        // player_->setMuted(muted_);
      }
    }
    ScheduleEvent("volumechange");
  }
}

void HTMLMediaElement::CreateMediaPlayer() {
  player_ = web_media_player_factory_->CreateWebMediaPlayer(this);
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
    ScheduleEvent("abort");
  }

  CreateMediaPlayer();

  // 4 - If the media element's networkState is not set to kNetworkEmpty, then
  // run these substeps
  if (network_state_ != kNetworkEmpty) {
    network_state_ = kNetworkEmpty;
    ready_state_ = WebMediaPlayer::kReadyStateHaveNothing;
    ready_state_maximum_ = WebMediaPlayer::kReadyStateHaveNothing;
    RefreshCachedTime();
    paused_ = true;
    seeking_ = false;
    InvalidateCachedTime();
    ScheduleEvent("emptied");
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
  // Select media resource.
  enum Mode { attribute, children };

  // 3 - If the media element has a src attribute, then let mode be attribute.
  Mode mode = attribute;
  if (src_.empty()) {
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
  ScheduleEvent("loadstart");

  // 6 - If mode is attribute, then run these substeps.
  if (mode == attribute) {
    load_state_ = kLoadingFromSrcAttr;

    // If the src attribute's value is the empty string ... jump down to the
    // failed step below.
    GURL mediaURL(src_);
    if (mediaURL.is_empty()) {
      MediaLoadingFailed(WebMediaPlayer::kNetworkStateFormatError);
      DLOG(WARNING) << "HTMLMediaElement::LoadInternal, empty 'src'";
      return;
    }

    // No type or key system information is available when the url comes from
    // the 'src' attribute so WebMediaPlayer will have to pick a media engine
    // based on the file extension.
    LoadResource(mediaURL, "", std::string());
    DLOG(INFO) << "HTMLMediaElement::LoadInternal, using 'src' attribute url";
    return;
  }
}

void HTMLMediaElement::LoadResource(const GURL& initial_url,
                                    const std::string& content_type,
                                    const std::string& key_system) {
  DLOG(INFO) << "HTMLMediaElement::LoadResource(" << initial_url << ", "
             << content_type << ", " << key_system;

  GURL url = initial_url;

  // The resource fetch algorithm
  network_state_ = kNetworkLoading;

  // Set current_src_ *before* changing to the cache url, the fact that we are
  // loading from the app cache is an internal detail not exposed through the
  // media element API.
  current_src_ = url.spec();

  StartProgressEventTimer();

  UpdateVolume();

  // TODO(***REMOVED***): deal with load error.
  player_->Load(url, WebMediaPlayer::kCORSModeUnspecified);
  // if (!player_->Load(url, WebMediaPlayer::kCORSModeUnspecified))
  //   MediaLoadingFailed(WebMediaPlayer::kNetworkStateFormatError);
}

void HTMLMediaElement::ClearMediaPlayer() {
  player_.reset(NULL);

  StopPeriodicTimers();
  load_timer_.Stop();

  pending_load_ = false;
  load_state_ = kWaitingForSource;
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
  ScheduleEvent("error");
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
  if (network_state_ != kNetworkLoading) {
    return;
  }

  double time = base::Time::Now().ToDoubleT();
  double time_delta = time - previous_progress_time_;

  if (player_->DidLoadingProgress()) {
    ScheduleEvent("progress");
    previous_progress_time_ = time;
    sent_stalled_event_ = false;
  } else if (time_delta > 3.0 && !sent_stalled_event_) {
    ScheduleEvent("stalled");
    sent_stalled_event_ = true;
  }
}

void HTMLMediaElement::OnPlaybackProgressTimer() {
  DCHECK(player_);

  ScheduleTimeupdateEvent(true);
}

void HTMLMediaElement::StartPlaybackProgressTimer() {
  if (playback_progress_timer_.IsRunning()) {
    return;
  }

  previous_progress_time_ = base::Time::Now().ToDoubleT();
  playback_progress_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kMaxTimeupdateEventFrequency * 1000),
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

void HTMLMediaElement::ScheduleTimeupdateEvent(bool periodicEvent) {
  double now = base::Time::Now().ToDoubleT();
  double time_delta = now - last_time_update_event_wall_time_;

  // throttle the periodic events
  if (periodicEvent && time_delta < kMaxTimeupdateEventFrequency) {
    return;
  }

  // Some media engines make multiple "time changed" callbacks at the same time,
  // but we only want one event at a given time so filter here
  float movie_time = current_time();
  if (movie_time != last_time_update_event_movie_time_) {
    ScheduleEvent("timeupdate");
    last_time_update_event_wall_time_ = now;
    last_time_update_event_movie_time_ = movie_time;
  }
}

void HTMLMediaElement::ScheduleEvent(const std::string& eventName) {
  scoped_refptr<Event> event =
      new Event(eventName, Event::kNotBubbles, Event::kCancelable);
  event->set_target(this);

  async_event_queue_.Enqueue(event);
}

void HTMLMediaElement::CancelPendingEventsAndCallbacks() {
  async_event_queue_.CancelAllEvents();
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
      ScheduleEvent("waiting");
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
      ScheduleEvent("waiting");
    }
  }

  if (ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata &&
      old_state < WebMediaPlayer::kReadyStateHaveMetadata) {
    ScheduleEvent("durationchange");
    ScheduleEvent("loadedmetadata");
  }

  if (ready_state_ >= WebMediaPlayer::kReadyStateHaveCurrentData &&
      old_state < WebMediaPlayer::kReadyStateHaveCurrentData &&
      !have_fired_loaded_data_) {
    have_fired_loaded_data_ = true;
    ScheduleEvent("loadeddata");
  }

  bool is_potentially_playing = PotentiallyPlaying();
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveFutureData &&
      old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
    ScheduleEvent("canplay");
    if (is_potentially_playing) {
      ScheduleEvent("playing");
    }
  }

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveEnoughData &&
      old_state < WebMediaPlayer::kReadyStateHaveEnoughData) {
    if (old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleEvent("canplay");
    }

    ScheduleEvent("canplaythrough");

    if (is_potentially_playing &&
        old_state <= WebMediaPlayer::kReadyStateHaveCurrentData) {
      ScheduleEvent("playing");
    }

    if (autoplaying_ && paused_ && autoplay()) {
      paused_ = false;
      InvalidateCachedTime();
      ScheduleEvent("play");
      ScheduleEvent("playing");
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
  ScheduleEvent("progress");
  ScheduleEvent("suspend");
  network_state_ = kNetworkIdle;
}

void HTMLMediaElement::Seek(float time, ExceptionCode* ec) {
  // 4.8.9.9 Seeking

  // 1 - If the media element's readyState is
  // WebMediaPlayer::kReadyStateHaveNothing, then raise an INVALID_STATE_ERR
  // exception.
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing || !player_) {
    // *ec = kInvalidStateErr;
    return;
  }

  // Get the current time before setting seeking_, last_seek_time_ is returned
  // once it is set.
  RefreshCachedTime();
  float now = current_time();

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

  if (no_seek_required) {
    if (time == now) {
      ScheduleEvent("seeking");
      ScheduleTimeupdateEvent(false);
      ScheduleEvent("seeked");
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
  ScheduleEvent("seeking");

  // 10 - Queue a task to fire a simple event named timeupdate at the element.
  ScheduleTimeupdateEvent(false);

  // 11-15 are handled, if necessary, when the engine signals a readystate
  // change.
}

void HTMLMediaElement::FinishSeek() {
  // 4.8.10.9 Seeking step 14
  seeking_ = false;

  // 4.8.10.9 Seeking step 15
  ScheduleEvent("seeked");
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
    // player_->SetMuted(muted_);
    player_->SetVolume(volume_);
  }
}

void HTMLMediaElement::UpdatePlayState() {
  if (!player_) {
    return;
  }

  bool should_be_playing = PotentiallyPlaying();
  bool player_paused = player_->Paused();

  if (should_be_playing) {
    InvalidateCachedTime();

    if (player_paused) {
      // Set rate, muted before calling play in case they were set before the
      // media engine was setup. The media engine should just stash the rate and
      // muted values since it isn't already playing.
      player_->SetRate(playback_rate_);
      // player_->SetMuted(muted_);

      player_->Play();
    }

    StartPlaybackProgressTimer();
    playing_ = true;

  } else {  // Should not be playing right now.
    if (!player_paused) {
      player_->Pause();
    }
    RefreshCachedTime();

    playback_progress_timer_.Stop();
    playing_ = false;
    float time = current_time();
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
  float now = current_time();
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
    if (!seekable_ranges->Contains(current_time())) {
      return true;
    }
  }

  return false;
}

bool HTMLMediaElement::CouldPlayIfEnoughData() const {
  return !paused() && !EndedPlayback() && !StoppedDueToErrors();
}

void HTMLMediaElement::InvalidateCachedTime() {
  // Don't try to cache movie time when playback first starts as the time
  // reported by the engine sometimes fluctuates for a short amount of time, so
  // the cached time will be off if we take it too early.
  static const double kMinimumTimePlayingBeforeCacheSnapshot = 0.5;

  minimum_wall_clock_time_to_cached_media_time_ =
      base::Time::Now().ToDoubleT() + kMinimumTimePlayingBeforeCacheSnapshot;
  cached_time_ = -1;
}

void HTMLMediaElement::RefreshCachedTime() const {
  cached_time_ = player_->CurrentTime();
  cached_time_wall_clock_update_time_ = base::Time::Now().ToDoubleT();
}

void HTMLMediaElement::ConfigureMediaControls() { NOTIMPLEMENTED(); }

void HTMLMediaElement::MediaEngineError(scoped_refptr<MediaError> error) {
  DLOG(WARNING) << "HTMLMediaElement::MediaEngineError " << error->code();

  // 1 - The user agent should cancel the fetching process.
  StopPeriodicTimers();
  load_state_ = kWaitingForSource;

  // 2 - Set the error attribute to a new MediaError object whose code attribute
  // is set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
  error_ = error;

  // 3 - Queue a task to fire a simple event named error at the media element.
  ScheduleEvent("error");

  // 4 - Set the element's networkState attribute to the kNetworkEmpty value and
  // queue a task to fire a simple event called emptied at the element.
  network_state_ = kNetworkEmpty;
  ScheduleEvent("emptied");
}

void HTMLMediaElement::NetworkStateChanged() {
  BeginProcessingMediaPlayerCallback();
  SetNetworkState(player_->GetNetworkState());
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::ReadyStateChanged() {
  BeginProcessingMediaPlayerCallback();
  SetReadyState(player_->GetReadyState());
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::VolumeChanged(float volume) {
  BeginProcessingMediaPlayerCallback();
  if (player_) {
    float vol = 1.0;  // player_->Volume();
    if (vol != volume_) {
      volume_ = vol;
      UpdateVolume();
      ScheduleEvent("volumechange");
    }
  }
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::MuteChanged(bool mute) {
  BeginProcessingMediaPlayerCallback();
  NOTIMPLEMENTED();
  // if (player_) { setMuted(player_->muted()); }
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::TimeChanged() {
  BeginProcessingMediaPlayerCallback();

  InvalidateCachedTime();

  // 4.8.10.9 step 14 & 15.  Needed if no ReadyState change is associated with
  // the seek.
  if (seeking_ && ready_state_ >= WebMediaPlayer::kReadyStateHaveCurrentData &&
      !player_->Seeking()) {
    FinishSeek();
  }

  // Always call ScheduleTimeupdateEvent when the media engine reports a time
  // discontinuity, it will only queue a 'timeupdate' event if we haven't
  // already posted one at the current movie time.
  ScheduleTimeupdateEvent(false);

  float now = current_time();
  float dur = duration();

  // When the current playback position reaches the end of the media resource
  // when the direction of playback is forwards, then the user agent must follow
  // these steps:
  if (!isnan(dur) && dur && now >= dur && playback_rate_ > 0) {
    // If the media element has a loop attribute specified and does not have a
    // current media controller,
    if (loop()) {
      sent_end_event_ = false;
      // then seek to the earliest possible position of the media resource and
      // abort these steps.
      ExceptionCode unused;

      Seek(0, &unused);
    } else {
      // If the media element does not have a current media controller, and the
      // media element has still ended playback, and the direction of playback
      // is still forwards, and paused is false,
      if (!paused_) {
        // changes paused to true and fires a simple event named pause at the
        // media element.
        paused_ = true;
        ScheduleEvent("pause");
      }
      // Queue a task to fire a simple event named ended at the media element.
      if (!sent_end_event_) {
        sent_end_event_ = true;
        ScheduleEvent("ended");
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

  ScheduleEvent("durationchange");

  float now = current_time();
  float dur = duration();
  if (now > dur) {
    ExceptionCode unused;
    Seek(dur, &unused);
  }

  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::RateChanged() {
  // TODO(***REMOVED***): Remove mediaPlayerRateChanged.
  NOTREACHED();
}

void HTMLMediaElement::SizeChanged() {
  // TODO(***REMOVED***): Remove mediaPlayerSizeChanged.
  NOTREACHED();
}

void HTMLMediaElement::PlaybackStateChanged() {
  if (!player_) {
    return;
  }

  BeginProcessingMediaPlayerCallback();
  if (player_->Paused()) {
    Pause();
  } else {
    Play();
  }
  EndProcessingMediaPlayerCallback();
}

void HTMLMediaElement::Repaint() { NOTIMPLEMENTED(); }

void HTMLMediaElement::SawUnsupportedTracks() { NOTIMPLEMENTED(); }

float HTMLMediaElement::Volume() const { return volume(); }

void HTMLMediaElement::SourceOpened() {
  // TODO(***REMOVED***): Remove mediaPlayerSourceOpened.
  NOTIMPLEMENTED();
}

std::string HTMLMediaElement::SourceURL() const {
  return media_source_url_.spec();
}

}  // namespace dom
}  // namespace cobalt
