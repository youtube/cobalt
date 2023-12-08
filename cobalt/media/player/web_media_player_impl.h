// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Delegate calls from WebCore::MediaPlayerPrivate to Chrome's video player.
// It contains Pipeline which is the actual media player pipeline, it glues
// the media player pipeline, data source, audio renderer and renderer.
// Pipeline would creates multiple threads and access some public methods
// of this class, so we need to be extra careful about concurrent access of
// methods and members.
//
// WebMediaPlayerImpl works with multiple objects, the most important ones are:
//
// Pipeline
//   The media playback pipeline.
//
// VideoRendererBase
//   Video renderer object.
//
// WebMediaPlayerClient
//   Client of this media player object.
//
// The following diagram shows the relationship of these objects:
//   (note: ref-counted reference is marked by a "r".)
//
// WebMediaPlayerClient
//    ^
//    |
// WebMediaPlayerImpl ---> Pipeline
//    |        ^                  |
//    |        |                  v r
//    |        |        VideoRendererBase
//    |        |          |       ^ r
//    |   r    |          v r     |
//    '---> WebMediaPlayerProxy --'
//
// Notice that WebMediaPlayerProxy and VideoRendererBase are referencing each
// other. This interdependency has to be treated carefully.
//
// Other issues:
// During tear down of the whole browser or a tab, the DOM tree may not be
// destructed nicely, and there will be some dangling media threads trying to
// the main thread, so we need this class to listen to destruction event of the
// main thread and cleanup the media threads when the even is received. Also
// at destruction of this class we will need to unhook it from destruction event
// list of the main thread.

#ifndef COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_IMPL_H_
#define COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "cobalt/math/size.h"
#include "cobalt/media/base/decode_target_provider.h"
#include "cobalt/media/base/metrics_provider.h"
#include "cobalt/media/base/pipeline.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "cobalt/media/player/web_media_player.h"
#include "cobalt/media/player/web_media_player_delegate.h"
#include "third_party/chromium/media/base/demuxer.h"
#include "third_party/chromium/media/base/eme_constants.h"
#include "third_party/chromium/media/base/media_log.h"
#include "url/gurl.h"

#if defined(STARBOARD)

#define COBALT_USE_PUNCHOUT
#define COBALT_SKIP_SEEK_REQUEST_NEAR_END

#endif  // defined(STARBOARD)

namespace cobalt {
namespace media {

class WebMediaPlayerProxy;

class WebMediaPlayerImpl : public WebMediaPlayer,
                           public base::MessageLoop::DestructionObserver,
                           public base::SupportsWeakPtr<WebMediaPlayerImpl> {
 public:
  // Construct a WebMediaPlayerImpl with reference to the client, and media
  // filter collection. By providing the filter collection the implementor can
  // provide more specific media filters that does resource loading and
  // rendering.
  //
  // WebMediaPlayerImpl comes packaged with the following media filters:
  //   - URL fetching
  //   - Demuxing
  //   - Software audio/video decoding
  //   - Video rendering
  //
  // Clients are expected to add their platform-specific audio rendering media
  // filter if they wish to hear any sound coming out the speakers, otherwise
  // audio data is discarded and media plays back based on wall clock time.
  //
  // When calling this, the |audio_source_provider| and
  // |audio_renderer_sink| arguments should be the same object.

  WebMediaPlayerImpl(SbPlayerInterface* interface, PipelineWindow window,
                     const Pipeline::GetDecodeTargetGraphicsContextProviderFunc&
                         get_decode_target_graphics_context_provider_func,
                     WebMediaPlayerClient* client,
                     WebMediaPlayerDelegate* delegate,
                     bool allow_resume_after_suspend,
                     bool allow_batched_sample_write,
                     bool force_punch_out_by_default,
#if SB_API_VERSION >= 15
                     SbTime audio_write_duration_local,
                     SbTime audio_write_duration_remote,
#endif  // SB_API_VERSION >= 15
                     ::media::MediaLog* const media_log);
  ~WebMediaPlayerImpl() override;

#if SB_HAS(PLAYER_WITH_URL)
  void LoadUrl(const GURL& url) override;
#endif  // SB_HAS(PLAYER_WITH_URL)
  void LoadMediaSource() override;
  void LoadProgressive(const GURL& url,
                       std::unique_ptr<DataSource> data_source) override;

  void CancelLoad() override;

  // Playback controls.
  void Play() override;
  void Pause() override;
  void Seek(double seconds) override;
  void SetRate(float rate) override;
  void SetVolume(float volume) override;
  void SetVisible(bool visible) override;
  void UpdateBufferedTimeRanges(const AddRangeCB& add_range_cb) override;
  double GetMaxTimeSeekable() const override;

  // Suspend/Resume
  void Suspend() override;
  // TODO: This is temporary for supporting background media playback.
  //       Need to be removed with media refactor.
  void Resume(PipelineWindow window) override;

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const override;
  bool HasAudio() const override;

  // Dimensions of the video.
  int GetNaturalWidth() const override;
  int GetNaturalHeight() const override;

  // Names of audio connectors used by the playback.
  std::vector<std::string> GetAudioConnectors() const override;

  // Getters of playback state.
  bool IsPaused() const override;
  bool IsSeeking() const override;
  double GetDuration() const override;
#if SB_HAS(PLAYER_WITH_URL)
  base::Time GetStartDate() const override;
#endif  // SB_HAS(PLAYER_WITH_URL)
  double GetCurrentTime() const override;
  float GetPlaybackRate() const override;

  // Get rate of loading the resource.
  int32 GetDataRate() const override;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  NetworkState GetNetworkState() const override;
  ReadyState GetReadyState() const override;

  bool DidLoadingProgress() const override;

  double MediaTimeForTimeValue(double timeValue) const override;

  PlayerStatistics GetStatistics() const override;

  scoped_refptr<DecodeTargetProvider> GetDecodeTargetProvider() override;

  SetBoundsCB GetSetBoundsCB() override;

  // As we are closing the tab or even the browser, |main_loop_| is destroyed
  // even before this object gets destructed, so we need to know when
  // |main_loop_| is being destroyed and we can stop posting repaint task
  // to it.
  void WillDestroyCurrentMessageLoop() override;

  bool GetDebugReportDataAddress(void** out_address, size_t* out_size) override;

  void SetDrmSystem(const scoped_refptr<media::DrmSystem>& drm_system) override;
  void SetDrmSystemReadyCB(const DrmSystemReadyCB& drm_system_ready_cb);

  void OnPipelineSeek(::media::PipelineStatus status, bool is_initial_preroll,
                      const std::string& error_message);
  void OnPipelineEnded(::media::PipelineStatus status);
  void OnPipelineError(::media::PipelineStatus error,
                       const std::string& message);
  void OnPipelineBufferingState(Pipeline::BufferingState buffering_state);
  void OnDemuxerOpened();

 private:
  // Called when the data source is downloading or paused.
  void OnDownloadingStatusChanged(bool is_downloading);

// Finishes starting the pipeline due to a call to load().
#if SB_HAS(PLAYER_WITH_URL)
  void StartPipeline(const GURL& url);
#endif  // SB_HAS(PLAYER_WITH_URL)
  void StartPipeline(::media::Demuxer* demuxer);

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(NetworkState state);
  void SetNetworkError(NetworkState state, const std::string& message);
  void SetReadyState(ReadyState state);

  // Destroy resources held.
  void Destroy();

  void GetMediaTimeAndSeekingState(base::TimeDelta* media_time,
                                   bool* is_seeking) const;
  void OnEncryptedMediaInitDataEncounteredWrapper(
      ::media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data);
  void OnEncryptedMediaInitDataEncountered(
      const char* init_data_type, const std::vector<uint8_t>& init_data);

  // Getter method to |client_|.
  WebMediaPlayerClient* GetClient();

 private:
  // Callbacks that forward duration change from |pipeline_| to |client_|.
  void OnDurationChanged();
  void OnOutputModeChanged();
  void OnContentSizeChanged();

  base::Thread pipeline_thread_;

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  NetworkState network_state_;
  ReadyState ready_state_;

  // Message loops for posting tasks between Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  base::MessageLoop* main_loop_;

  scoped_refptr<Pipeline> pipeline_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  std::string current_key_system_;

  // Internal state of the WebMediaPlayer. Gathered in one struct to support
  // serialization of this state in debug logs. This should not contain any
  // sensitive or potentially PII.
  struct WebMediaPlayerState {
    WebMediaPlayerState()
        : paused(true),
          seeking(false),
          playback_rate(0.0f),
          pending_seek(false),
          pending_seek_seconds(0.0),
          starting(false),
          is_progressive(false),
          is_media_source(false) {}
    // Playback state.
    //
    // TODO(scherkus): we have these because Pipeline favours the simplicity of
    // a single "playback rate" over worrying about paused/stopped etc...  It
    // forces all clients to manage the pause+playback rate externally, but is
    // that really a bad thing?
    //
    // TODO(scherkus): since SetPlaybackRate(0) is asynchronous and we don't
    // want to hang the render thread during pause(), we record the time at the
    // same time we pause and then return that value in currentTime().
    // Otherwise our clock can creep forward a little bit while the asynchronous
    // SetPlaybackRate(0) is being executed.
    bool paused;
    bool seeking;
    float playback_rate;
    base::TimeDelta paused_time;

    // Seek gets pending if another seek is in progress. Only last pending seek
    // will have effect.
    bool pending_seek;
    double pending_seek_seconds;

    bool starting;

    bool is_progressive;
    bool is_media_source;
  } state_;

  WebMediaPlayerClient* const client_;
  WebMediaPlayerDelegate* const delegate_;
  const bool allow_resume_after_suspend_;
  const bool allow_batched_sample_write_;
  const bool force_punch_out_by_default_;
  scoped_refptr<DecodeTargetProvider> decode_target_provider_;

  scoped_refptr<WebMediaPlayerProxy> proxy_;

  ::media::MediaLog* const media_log_;

  MediaMetricsProvider media_metrics_provider_;

  bool is_local_source_;

  std::unique_ptr<::media::Demuxer> progressive_demuxer_;
  std::unique_ptr<::media::ChunkDemuxer> chunk_demuxer_;

  // Suppresses calls to OnPipelineError() after destruction / shutdown has been
  // started; prevents us from spuriously logging errors that are transient or
  // unimportant.
  bool suppress_destruction_errors_;

  DrmSystemReadyCB drm_system_ready_cb_;
  scoped_refptr<DrmSystem> drm_system_;

  // Used to determine when the player enters and exits background mode.
  PipelineWindow window_;

  bool is_resuming_from_background_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_IMPL_H_
