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

#ifndef COBALT_DOM_HTML_MEDIA_ELEMENT_H_
#define COBALT_DOM_HTML_MEDIA_ELEMENT_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "cobalt/dom/eme/media_keys.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/media_error.h"
#include "cobalt/dom/time_ranges.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/media/player/web_media_player.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/typed_arrays.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

class MediaSource;

// The HTMLMediaElement is the base of HTMLAudioElement and HTMLVideoElement.
//   https://www.w3.org/TR/html50/embedded-content-0.html#media-element
class HTMLMediaElement : public HTMLElement,
                         private cobalt::media::WebMediaPlayerClient {
 public:
  typedef ::media::ChunkDemuxer ChunkDemuxer;
  typedef ::cobalt::media::WebMediaPlayer WebMediaPlayer;
  typedef ::cobalt::media::WebMediaPlayerClient WebMediaPlayerClient;

  HTMLMediaElement(Document* document, base::Token tag_name);
  ~HTMLMediaElement() override;

  // Web API: HTMLMediaElement
  //
  // Error state
  scoped_refptr<MediaError> error() const;

  // Custom, not in any spec
  //
  // From HTMLElement
  scoped_refptr<HTMLMediaElement> AsHTMLMediaElement() override { return this; }

  // Network state
  std::string src() const;
  void set_src(const std::string& src);
  const std::string& current_src() const { return current_src_; }

  base::Optional<std::string> cross_origin() const;
  void set_cross_origin(const base::Optional<std::string>& value);

  enum NetworkState {
    kNetworkEmpty,
    kNetworkIdle,
    kNetworkLoading,
    kNetworkNoSource
  };
  uint16_t network_state() const;

  scoped_refptr<TimeRanges> buffered() const;
  void Load();
  std::string CanPlayType(const std::string& mime_type);
  std::string CanPlayType(const std::string& mime_type,
                          const std::string& key_system);

  const EventListenerScriptValue* onencrypted() const;
  void set_onencrypted(const EventListenerScriptValue& event_listener);

  const scoped_refptr<eme::MediaKeys>& media_keys() const {
    return media_keys_;
  }
  typedef script::ScriptValue<script::Promise<void>> VoidPromiseValue;
  script::Handle<script::Promise<void>> SetMediaKeys(
      const scoped_refptr<eme::MediaKeys>& media_keys);

  // Ready state
  enum ReadyState {
    kHaveNothing = WebMediaPlayer::kReadyStateHaveNothing,
    kHaveMetadata = WebMediaPlayer::kReadyStateHaveMetadata,
    kHaveCurrentData = WebMediaPlayer::kReadyStateHaveCurrentData,
    kHaveFutureData = WebMediaPlayer::kReadyStateHaveFutureData,
    kHaveEnoughData = WebMediaPlayer::kReadyStateHaveEnoughData,
  };

  uint16_t ready_state() const;
  bool seeking() const;

  // Playback state
  double current_time(script::ExceptionState* exception_state) const;
  void set_current_time(double time, script::ExceptionState* exception_state);
  double duration() const;
  base::Time GetStartDate() const;
  bool paused() const;
  bool resume_frozen_flag() const;
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
  void set_resume_frozen_flag(bool resume_frozen_flag);

  // Controls
  bool controls() const;
  void set_controls(bool controls);
  float volume(script::ExceptionState* exception_state) const;
  void set_volume(float volume, script::ExceptionState* exception_state);
  bool muted() const;
  void set_muted(bool muted);

  // Custom, not in any spec
  //
  // From Node
  void OnInsertedIntoDocument() override;

  // Called by MediaSource
  void DurationChanged(double duration, bool request_seek);

  // Let other objects add event to the EventQueue of HTMLMediaElement.  This
  // function won't modify the target of the |event| passed in.
  void ScheduleEvent(const scoped_refptr<web::Event>& event);

  // Returns semicolon separated names of audio connectors, like
  // "hdmi;bluetooth".
  // TODO(b/267678497): The current interface is tentative, to be refined.
  std::string h5vcc_audio_connectors(
      script::ExceptionState* exception_state) const;

  // Set max video capabilities.
  void SetMaxVideoCapabilities(const std::string& max_video_capabilities,
                               script::ExceptionState* exception_state);
  bool HasMaxVideoCapabilities() const {
    return !max_video_capabilities_.empty();
  }

  DEFINE_WRAPPABLE_TYPE(HTMLMediaElement);
  void TraceMembers(script::Tracer* tracer) override;

 protected:
  WebMediaPlayer* player() { return player_.get(); }
  const WebMediaPlayer* player() const { return player_.get(); }

 private:
  // Loading
  void CreateMediaPlayer();
  void ScheduleLoad();
  void PrepareForLoad();
  void LoadInternal();
  void LoadResource(const GURL& initial_url, const std::string& content_type,
                    const std::string& key_system);
  void ClearMediaPlayer();
  void NoneSupported(const std::string& message);
  void MediaLoadingFailed(WebMediaPlayer::NetworkState error,
                          const std::string& message);

  // Timers
  void OnLoadTimer();
  void OnProgressEventTimer();
  void OnPlaybackProgressTimer();
  void StartPlaybackProgressTimer();
  void StartProgressEventTimer();
  void StopPeriodicTimers();

  // Events
  void ScheduleTimeupdateEvent(bool periodic_event);
  void ScheduleOwnEvent(base::Token event_name);
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
  void SetNetworkError(WebMediaPlayer::NetworkState state,
                       const std::string& message);
  void ChangeNetworkStateFromLoadingToIdle();

  // Playback
  void Seek(double time);
  void FinishSeek();

  void AddPlayedRange(double start, double end);

  void UpdateVolume();
  void UpdatePlayState();
  bool PotentiallyPlaying() const;
  bool EndedPlayback() const;
  bool StoppedDueToErrors() const;
  bool CouldPlayIfEnoughData() const;

  void ConfigureMediaControls();

  // Error report
  void MediaEngineError(scoped_refptr<MediaError> error);

  // WebMediaPlayerClient methods
  void NetworkStateChanged() override;
  void NetworkError(const std::string& message) override;
  void ReadyStateChanged() override;
  void TimeChanged(bool eos_played) override;
  void DurationChanged() override;
  void OutputModeChanged() override;
  void ContentSizeChanged() override;
  void PlaybackStateChanged() override;
  float Volume() const override;
  void SourceOpened(ChunkDemuxer* chunk_demuxer) override;
  std::string SourceURL() const override;
  std::string MaxVideoCapabilities() const override;
  bool PreferDecodeToTexture() override;
  void EncryptedMediaInitDataEncountered(
      const char* init_data_type, const unsigned char* init_data,
      unsigned int init_data_length) override;
  void ClearMediaSource();

  std::unique_ptr<WebMediaPlayer> player_;

  std::string current_src_;

  std::string max_video_capabilities_;

  // Loading state.
  enum LoadState { kWaitingForSource, kLoadingFromSrcAttr };
  LoadState load_state_;

  EventQueue event_queue_;

  base::OneShotTimer load_timer_;
  base::RepeatingTimer progress_event_timer_;
  base::RepeatingTimer playback_progress_timer_;
  scoped_refptr<TimeRanges> played_time_ranges_;
  float playback_rate_;
  float default_playback_rate_;
  NetworkState network_state_;
  WebMediaPlayer::ReadyState ready_state_;
  WebMediaPlayer::ReadyState ready_state_maximum_;

  float volume_;
  double last_seek_time_;
  double previous_progress_time_;

  double duration_;
  base::Time start_date_;

  bool playing_;
  bool have_fired_loaded_data_;
  bool autoplaying_;
  bool muted_;
  bool paused_;
  //  https://wicg.github.io/page-lifecycle/#htmlmediaelement-resume-frozen-flag
  bool resume_frozen_flag_;
  bool seeking_;
  bool controls_;

  // The last time a timeupdate event was sent in movie time.
  double last_time_update_event_movie_time_;

  // Counter incremented while processing a callback from the media player, so
  // we can avoid calling the low level player recursively.
  int processing_media_player_callback_;

  GURL media_source_url_;
  scoped_refptr<MediaSource> media_source_;

  bool pending_load_;

  // Data has not been loaded since sending a "stalled" event.
  bool sent_stalled_event_;
  // Time has not changed since sending an "ended" event.
  bool sent_end_event_;

  scoped_refptr<MediaError> error_;

  // Helper object to reduce the image capacity while a video is playing.
  base::Optional<loader::image::ReducedCacheCapacityManager::Request>
      reduced_image_cache_capacity_request_;

  scoped_refptr<eme::MediaKeys> media_keys_;

  loader::RequestMode request_mode_;

  DISALLOW_COPY_AND_ASSIGN(HTMLMediaElement);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_MEDIA_ELEMENT_H_
