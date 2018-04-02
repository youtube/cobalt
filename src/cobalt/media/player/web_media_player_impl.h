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

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/demuxer.h"
#include "cobalt/media/base/eme_constants.h"
#include "cobalt/media/base/pipeline.h"
#include "cobalt/media/base/ranges.h"
#include "cobalt/media/base/video_frame_provider.h"
#include "cobalt/media/player/web_media_player.h"
#include "cobalt/media/player/web_media_player_delegate.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/size.h"

#if defined(OS_STARBOARD)

#define COBALT_USE_PUNCHOUT
#define COBALT_SKIP_SEEK_REQUEST_NEAR_END

#endif  // defined(OS_STARBOARD)

namespace cobalt {
namespace media {

class ChunkDemuxer;
class MediaLog;
class WebMediaPlayerProxy;

class WebMediaPlayerImpl : public WebMediaPlayer,
                           public MessageLoop::DestructionObserver,
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

  WebMediaPlayerImpl(
      PipelineWindow window, WebMediaPlayerClient* client,
      WebMediaPlayerDelegate* delegate,
      DecoderBuffer::Allocator* buffer_allocator,
      const scoped_refptr<MediaLog>& media_log);
  ~WebMediaPlayerImpl() override;

#if SB_HAS(PLAYER_WITH_URL)
  void LoadUrl(const GURL& url) override;
#else   // SB_HAS(PLAYER_WITH_URL)
  void LoadMediaSource() override;
  void LoadProgressive(const GURL& url,
                       scoped_ptr<BufferedDataSource> data_source) override;
#endif  // SB_HAS(PLAYER_WITH_URL)
  void CancelLoad() override;

  // Playback controls.
  void Play() override;
  void Pause() override;
  bool SupportsFullscreen() const override;
  bool SupportsSave() const override;
  void Seek(float seconds) override;
  void SetEndTime(float seconds) override;
  void SetRate(float rate) override;
  void SetVolume(float volume) override;
  void SetVisible(bool visible) override;
  const Ranges<base::TimeDelta>& GetBufferedTimeRanges() override;
  float GetMaxTimeSeekable() const override;

  // Suspend/Resume
  void Suspend() override;
  void Resume() override;

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const override;
  bool HasAudio() const override;

  // Dimensions of the video.
  gfx::Size GetNaturalSize() const override;

  // Getters of playback state.
  bool IsPaused() const override;
  bool IsSeeking() const override;
  float GetDuration() const override;
#if SB_HAS(PLAYER_WITH_URL)
  base::Time GetStartDate() const override;
#endif  // SB_HAS(PLAYER_WITH_URL)
  float GetCurrentTime() const override;

  // Get rate of loading the resource.
  int32 GetDataRate() const override;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  WebMediaPlayer::NetworkState GetNetworkState() const override;
  WebMediaPlayer::ReadyState GetReadyState() const override;

  bool DidLoadingProgress() const override;

  bool HasSingleSecurityOrigin() const override;
  bool DidPassCORSAccessCheck() const override;

  float MediaTimeForTimeValue(float timeValue) const override;

  unsigned GetDecodedFrameCount() const override;
  unsigned GetDroppedFrameCount() const override;
  unsigned GetAudioDecodedByteCount() const override;
  unsigned GetVideoDecodedByteCount() const override;

  scoped_refptr<VideoFrameProvider> GetVideoFrameProvider() override;

  SetBoundsCB GetSetBoundsCB() override;

  // As we are closing the tab or even the browser, |main_loop_| is destroyed
  // even before this object gets destructed, so we need to know when
  // |main_loop_| is being destroyed and we can stop posting repaint task
  // to it.
  void WillDestroyCurrentMessageLoop() override;

  bool GetDebugReportDataAddress(void** out_address, size_t* out_size) override;

  void SetDrmSystem(DrmSystem* drm_system) override;
  void SetDrmSystemReadyCB(const DrmSystemReadyCB& drm_system_ready_cb);

  void OnPipelineSeek(PipelineStatus status);
  void OnPipelineEnded(PipelineStatus status);
  void OnPipelineError(PipelineStatus error, const std::string& message);
  void OnPipelineBufferingState(Pipeline::BufferingState buffering_state);
  void OnDemuxerOpened();
  void SetOpaque(bool);

 private:
  // Called when the data source is downloading or paused.
  void OnDownloadingStatusChanged(bool is_downloading);

  // Finishes starting the pipeline due to a call to load().
#if SB_HAS(PLAYER_WITH_URL)
  void StartPipeline(const GURL& url);
#else   // SB_HAS(PLAYER_WITH_URL)
  void StartPipeline(Demuxer* demuxer);
#endif  // SB_HAS(PLAYER_WITH_URL)

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(WebMediaPlayer::NetworkState state);
  void SetNetworkError(WebMediaPlayer::NetworkState state,
                       const std::string& message);
  void SetReadyState(WebMediaPlayer::ReadyState state);

  // Destroy resources held.
  void Destroy();

  void GetMediaTimeAndSeekingState(base::TimeDelta* media_time,
                                   bool* is_seeking) const;
  void OnEncryptedMediaInitDataEncountered(
      EmeInitDataType init_data_type, const std::vector<uint8_t>& init_data);
  void OnEncryptedMediaInitDataEncounteredWrapper(
      const char* init_data_type, const unsigned char* init_data,
      unsigned int init_data_length);

  // Getter method to |client_|.
  WebMediaPlayerClient* GetClient();

 private:
  // Callbacks that forward duration change from |pipeline_| to |client_|.
  void OnDurationChanged();
  void OnOutputModeChanged();
  void OnContentSizeChanged();

  base::Thread pipeline_thread_;

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  WebMediaPlayer::NetworkState network_state_;
  WebMediaPlayer::ReadyState ready_state_;

  // Keep a list of buffered time ranges.
  Ranges<base::TimeDelta> buffered_;

  // Message loops for posting tasks between Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  MessageLoop* main_loop_;

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
          pending_seek_seconds(0.0f),
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
    float pending_seek_seconds;

    bool starting;

    bool is_progressive;
    bool is_media_source;
  } state_;

  WebMediaPlayerClient* client_;
  WebMediaPlayerDelegate* delegate_;
  DecoderBuffer::Allocator* buffer_allocator_;
  scoped_refptr<VideoFrameProvider> video_frame_provider_;

  scoped_refptr<WebMediaPlayerProxy> proxy_;

  scoped_refptr<MediaLog> media_log_;

  bool incremented_externally_allocated_memory_;

  bool is_local_source_;
  bool supports_save_;

  scoped_ptr<Demuxer> progressive_demuxer_;
  scoped_ptr<ChunkDemuxer> chunk_demuxer_;

#if defined(__LB_ANDROID__)
  AudioFocusBridge audio_focus_bridge_;
#endif  // defined(__LB_ANDROID__)

  // Suppresses calls to OnPipelineError() after destruction / shutdown has been
  // started; prevents us from spuriously logging errors that are transient or
  // unimportant.
  bool suppress_destruction_errors_;

  base::Callback<void(base::TimeDelta*, bool*)>
      media_time_and_seeking_state_cb_;

  DrmSystemReadyCB drm_system_ready_cb_;
  DrmSystem* drm_system_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_IMPL_H_
