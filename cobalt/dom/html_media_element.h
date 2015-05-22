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

#ifndef DOM_HTML_MEDIA_ELEMENT_H_
#define DOM_HTML_MEDIA_ELEMENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/media_error.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/time_ranges.h"
#include "cobalt/media/web_media_player_factory.h"
#include "googleurl/src/gurl.h"
#include "media/base/shell_video_frame_provider.h"
#include "media/player/web_media_player.h"

namespace cobalt {
namespace dom {

// TODO(***REMOVED***): Remove the following typedef after we have proper dom
// exception support.
typedef int ExceptionCode;

// The HTMLMediaElement is used to play videos.
//   http://www.w3.org/TR/html5/embedded-content-0.html#media-element
class HTMLMediaElement : public HTMLElement,
                         private ::media::WebMediaPlayerClient {
 public:
  typedef ::media::WebMediaPlayer WebMediaPlayer;

  static const char kTagName[];

  HTMLMediaElement(HTMLElementFactory* html_element_factory,
                   cssom::CSSParser* css_parser,
                   media::WebMediaPlayerFactory* web_media_player_factory);
  ~HTMLMediaElement() OVERRIDE;

  // Web API: Element
  //
  const std::string& tag_name() const OVERRIDE;

  // Web API: HTMLMediaElement
  //
  // Error state
  scoped_refptr<MediaError> error() const { return error_; }

  // Network state
  const std::string& src() const { return src_; }
  void set_src(const std::string& src);
  const std::string& current_src() const { return current_src_; }

  enum NetworkState {
    kNetworkEmpty,
    kNetworkIdle,
    kNetworkLoading,
    kNetworkNoSource
  };
  uint16_t network_state() const;

  scoped_refptr<TimeRanges> buffered() const;
  void Load();
  std::string CanPlayType(const std::string& mimeType) const;
  std::string CanPlayType(const std::string& mimeType,
                          const std::string& keySystem) const;

  // Ready state
  enum ReadyState {
    kHaveNothing = WebMediaPlayer::kReadyStateHaveNothing,
    kHaveMetadata = WebMediaPlayer::kReadyStateHaveMetadata,
    kHaveCurrentData = WebMediaPlayer::kReadyStateHaveCurrentData,
    kHaveFutureData = WebMediaPlayer::kReadyStateHaveFutureData,
    kHaveEnoughData = WebMediaPlayer::kReadyStateHaveEnoughData,
  };

  WebMediaPlayer::ReadyState ready_state() const;
  bool seeking() const;

  // Playback state
  float current_time() const;
  void set_current_time(float time /*, ExceptionCode**/);
  float duration() const;
  bool paused() const;
  float default_playback_rate() const;
  void set_default_playback_rate(float rate);
  float playback_rate() const;
  void set_playback_rate(float rate);
  const scoped_refptr<TimeRanges>& played();
  scoped_refptr<TimeRanges> seekable() const;
  bool ended() const;
  bool autoplay() const;
  void set_autoplay(bool autoplay);
  bool loop() const;
  void set_loop(bool loop);
  void Play();
  void Pause();

  // Controls
  bool controls() const;
  void set_controls(bool controls);
  float volume() const;
  void set_volume(float volume /*, ExceptionCode**/);
  bool muted() const;
  void set_muted(bool muted);

  // Custom, not in any spec
  //
  // From Node
  void AttachToDocument(Document* document) OVERRIDE;
  // From HTMLElement
  scoped_refptr<HTMLMediaElement> AsHTMLMediaElement() OVERRIDE { return this; }

  // TODO(***REMOVED***): ShellVideoFrameProvider is guaranteed to be long live and
  // thread safe. However, it is actually a singleton internally. We should find
  // a better way to support concurrent video playbacks.
  ::media::ShellVideoFrameProvider* GetVideoFrameProvider();

  DEFINE_WRAPPABLE_TYPE(HTMLMediaElement);

 private:
  static const char kMediaSourceUrlProtocol[];
  static const double kMaxTimeupdateEventFrequency;

  // Loading
  void CreateMediaPlayer();
  void ScheduleLoad();
  void PrepareForLoad();
  void LoadInternal();
  void LoadResource(const GURL& initial_url, const std::string& content_type,
                    const std::string& key_system);
  void ClearMediaPlayer();
  void NoneSupported();
  void MediaLoadingFailed(WebMediaPlayer::NetworkState error);

  // Timers
  void OnLoadTimer();
  void OnProgressEventTimer();
  void OnPlaybackProgressTimer();
  void StartPlaybackProgressTimer();
  void StartProgressEventTimer();
  void StopPeriodicTimers();

  // Events
  void ScheduleTimeupdateEvent(bool periodicEvent);
  void ScheduleEvent(const std::string& eventName);
  void CancelPendingEventsAndCallbacks();
  bool ProcessingMediaPlayerCallback() const {
    return processing_media_player_callback_ > 0;
  }
  void BeginProcessingMediaPlayerCallback() {
    ++processing_media_player_callback_;
  }
  void EndProcessingMediaPlayerCallback() {
    DCHECK(processing_media_player_callback_);
    --processing_media_player_callback_;
  }

  // States
  void SetReadyState(WebMediaPlayer::ReadyState state);
  void SetNetworkState(WebMediaPlayer::NetworkState state);
  void ChangeNetworkStateFromLoadingToIdle();

  // Playback
  void Seek(float time, ExceptionCode* ec);
  void FinishSeek();

  void AddPlayedRange(float start, float end);

  void UpdateVolume();
  void UpdatePlayState();
  bool PotentiallyPlaying() const;
  bool EndedPlayback() const;
  bool StoppedDueToErrors() const;
  bool CouldPlayIfEnoughData() const;

  void InvalidateCachedTime();
  void RefreshCachedTime() const;

  void ConfigureMediaControls();

  // Error report
  void MediaEngineError(scoped_refptr<MediaError> error);

  // WebMediaPlayerClient methods
  void NetworkStateChanged() OVERRIDE;
  void ReadyStateChanged() OVERRIDE;
  void VolumeChanged(float volume) OVERRIDE;
  void MuteChanged(bool mute) OVERRIDE;
  void TimeChanged() OVERRIDE;
  void DurationChanged() OVERRIDE;
  void RateChanged() OVERRIDE;
  void SizeChanged() OVERRIDE;
  void PlaybackStateChanged() OVERRIDE;
  void Repaint() OVERRIDE;
  void SawUnsupportedTracks() OVERRIDE;
  float Volume() const OVERRIDE;
  void SourceOpened() OVERRIDE;
  std::string SourceURL() const OVERRIDE;

  void SetSourceState(MediaSource::ReadyState ready_state);

  media::WebMediaPlayerFactory* web_media_player_factory_;
  scoped_ptr<WebMediaPlayer> player_;

  std::string src_;
  std::string current_src_;
  // Loading state.
  enum LoadState { kWaitingForSource, kLoadingFromSrcAttr };
  LoadState load_state_;

  EventQueue async_event_queue_;

  base::OneShotTimer<HTMLMediaElement> load_timer_;
  base::RepeatingTimer<HTMLMediaElement> progress_event_timer_;
  base::RepeatingTimer<HTMLMediaElement> playback_progress_timer_;
  scoped_refptr<TimeRanges> played_time_ranges_;
  float playback_rate_;
  float default_playback_rate_;
  NetworkState network_state_;
  WebMediaPlayer::ReadyState ready_state_;
  WebMediaPlayer::ReadyState ready_state_maximum_;

  float volume_;
  float last_seek_time_;
  double previous_progress_time_;

  bool playing_;
  bool have_fired_loaded_data_;
  bool autoplaying_;
  bool muted_;
  bool paused_;
  bool seeking_;
  bool loop_;
  bool controls_;

  // The last time a timeupdate event was sent (wall clock).
  double last_time_update_event_wall_time_;
  // The last time a timeupdate event was sent in movie time.
  double last_time_update_event_movie_time_;

  // Counter incremented while processing a callback from the media player, so
  // we can avoid calling the low level player recursively.
  int processing_media_player_callback_;

  GURL media_source_url_;
  scoped_refptr<MediaSource> media_source_;

  mutable float cached_time_;
  mutable double cached_time_wall_clock_update_time_;
  mutable double minimum_wall_clock_time_to_cached_media_time_;

  bool pending_load_;

  // Data has not been loaded since sending a "stalled" event.
  bool sent_stalled_event_;
  // Time has not changed since sending an "ended" event.
  bool sent_end_event_;

  scoped_refptr<MediaError> error_;

  // Thread checker ensures that HTMLMediaElement::GetVideoFrameProvider() is
  // only called from the thread that the HTMLMediaElement is created in.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HTMLMediaElement);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_MEDIA_ELEMENT_H_
