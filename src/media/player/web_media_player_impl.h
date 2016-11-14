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

#ifndef MEDIA_PLAYER_WEB_MEDIA_PLAYER_IMPL_H_
#define MEDIA_PLAYER_WEB_MEDIA_PLAYER_IMPL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"
#include "media/base/ranges.h"
#include "media/base/shell_video_frame_provider.h"
#include "media/player/crypto/key_systems.h"
#include "media/player/crypto/proxy_decryptor.h"
#include "media/player/web_media_player.h"
#include "media/player/web_media_player_delegate.h"
#include "ui/gfx/size.h"

#if defined(OS_STARBOARD)

#if SB_HAS(PLAYER)
#define COBALT_USE_PUNCHOUT
#define COBALT_SKIP_SEEK_REQUEST_NEAR_END
#endif  // SB_HAS(PLAYER)

#endif  // defined(OS_STARBOARD)

namespace media {

class AudioRendererSink;
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
      PipelineWindow window,
      WebMediaPlayerClient* client,
      WebMediaPlayerDelegate* delegate,
      const scoped_refptr<ShellVideoFrameProvider>& video_frame_provider,
      scoped_ptr<FilterCollection> collection,
      const scoped_refptr<AudioRendererSink>& audio_renderer_sink,
      scoped_ptr<MessageLoopFactory> message_loop_factory,
      const scoped_refptr<MediaLog>& media_log);
  ~WebMediaPlayerImpl() OVERRIDE;

  void LoadMediaSource() OVERRIDE;
  void LoadProgressive(const GURL& url,
                       const scoped_refptr<BufferedDataSource>& data_source,
                       CORSMode cors_mode) OVERRIDE;
  void CancelLoad() OVERRIDE;

  // Playback controls.
  void Play() OVERRIDE;
  void Pause() OVERRIDE;
  bool SupportsFullscreen() const OVERRIDE;
  bool SupportsSave() const OVERRIDE;
  void Seek(float seconds) OVERRIDE;
  void SetEndTime(float seconds) OVERRIDE;
  void SetRate(float rate) OVERRIDE;
  void SetVolume(float volume) OVERRIDE;
  void SetVisible(bool visible) OVERRIDE;
  virtual bool GetTotalBytesKnown();
  const Ranges<base::TimeDelta>& GetBufferedTimeRanges() OVERRIDE;
  float GetMaxTimeSeekable() const OVERRIDE;

  // Suspend/Resume
  void Suspend() OVERRIDE;
  void Resume() OVERRIDE;

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const OVERRIDE;
  bool HasAudio() const OVERRIDE;

  // Dimensions of the video.
  gfx::Size GetNaturalSize() const OVERRIDE;

  // Getters of playback state.
  bool IsPaused() const OVERRIDE;
  bool IsSeeking() const OVERRIDE;
  float GetDuration() const OVERRIDE;
  float GetCurrentTime() const OVERRIDE;

  // Get rate of loading the resource.
  int32 GetDataRate() const OVERRIDE;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  WebMediaPlayer::NetworkState GetNetworkState() const OVERRIDE;
  WebMediaPlayer::ReadyState GetReadyState() const OVERRIDE;

  bool DidLoadingProgress() const OVERRIDE;
  unsigned long long GetTotalBytes() const OVERRIDE;

  bool HasSingleSecurityOrigin() const OVERRIDE;
  bool DidPassCORSAccessCheck() const OVERRIDE;

  float MediaTimeForTimeValue(float timeValue) const OVERRIDE;

  unsigned GetDecodedFrameCount() const OVERRIDE;
  unsigned GetDroppedFrameCount() const OVERRIDE;
  unsigned GetAudioDecodedByteCount() const OVERRIDE;
  unsigned GetVideoDecodedByteCount() const OVERRIDE;

  scoped_refptr<ShellVideoFrameProvider> GetVideoFrameProvider() OVERRIDE;
  // TODO: Remove Get/PutCurrentFrame.
  scoped_refptr<VideoFrame> GetCurrentFrame() OVERRIDE;
  void PutCurrentFrame(const scoped_refptr<VideoFrame>& video_frame) OVERRIDE;

  AddIdStatus SourceAddId(const std::string& id,
                          const std::string& type,
                          const std::vector<std::string>& codecs) OVERRIDE;
  bool SourceRemoveId(const std::string& id) OVERRIDE;
  Ranges<base::TimeDelta> SourceBuffered(const std::string& id) OVERRIDE;
  bool SourceAppend(const std::string& id,
                    const unsigned char* data,
                    unsigned length) OVERRIDE;
  bool SourceAbort(const std::string& id) OVERRIDE;
  double SourceGetDuration() const OVERRIDE;
  void SourceSetDuration(double new_duration) OVERRIDE;
  void SourceEndOfStream(EndOfStreamStatus status) OVERRIDE;
  bool SourceSetTimestampOffset(const std::string& id, double offset) OVERRIDE;

  MediaKeyException GenerateKeyRequest(const std::string& key_system,
                                       const unsigned char* init_data,
                                       unsigned init_data_length) OVERRIDE;

  MediaKeyException AddKey(const std::string& key_system,
                           const unsigned char* key,
                           unsigned key_length,
                           const unsigned char* init_data,
                           unsigned init_data_length,
                           const std::string& session_id) OVERRIDE;

  MediaKeyException CancelKeyRequest(const std::string& key_system,
                                     const std::string& session_id) OVERRIDE;

  SetBoundsCB GetSetBoundsCB() OVERRIDE;

  // As we are closing the tab or even the browser, |main_loop_| is destroyed
  // even before this object gets destructed, so we need to know when
  // |main_loop_| is being destroyed and we can stop posting repaint task
  // to it.
  void WillDestroyCurrentMessageLoop() OVERRIDE;

  bool GetDebugReportDataAddress(void** out_address, size_t* out_size) OVERRIDE;

  void OnPipelineSeek(PipelineStatus status);
  void OnPipelineEnded(PipelineStatus status);
  void OnPipelineError(PipelineStatus error);
  void OnPipelineBufferingState(Pipeline::BufferingState buffering_state);
  void OnDemuxerOpened();
  void OnKeyAdded(const std::string& key_system, const std::string& session_id);
  void OnKeyError(const std::string& key_system,
                  const std::string& session_id,
                  Decryptor::KeyError error_code,
                  int system_code);
  void OnKeyMessage(const std::string& key_system,
                    const std::string& session_id,
                    const std::string& message,
                    const GURL& default_url);
  void OnNeedKey(const std::string& key_system,
                 const std::string& type,
                 const std::string& session_id,
                 scoped_array<uint8> init_data,
                 int init_data_size);
  void SetOpaque(bool);

 private:
  // Called when the data source is downloading or paused.
  void NotifyDownloading(bool is_downloading);

  // Finishes starting the pipeline due to a call to load().
  void StartPipeline();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(WebMediaPlayer::NetworkState state);
  void SetReadyState(WebMediaPlayer::ReadyState state);

  // Destroy resources held.
  void Destroy();

  void GetMediaTimeAndSeekingState(base::TimeDelta* media_time,
                                   bool* is_seeking) const;

  // Getter method to |client_|.
  WebMediaPlayerClient* GetClient();

  // Callbacks that forward duration change from |pipeline_| to |client_|.
  void OnDurationChanged();

  // Actually do the work for generateKeyRequest/addKey so they can easily
  // report results to UMA.
  MediaKeyException GenerateKeyRequestInternal(const std::string& key_system,
                                               const unsigned char* init_data,
                                               unsigned init_data_length);
  MediaKeyException AddKeyInternal(const std::string& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const std::string& session_id);
  MediaKeyException CancelKeyRequestInternal(const std::string& key_system,
                                             const std::string& session_id);

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  WebMediaPlayer::NetworkState network_state_;
  WebMediaPlayer::ReadyState ready_state_;

  // Keep a list of buffered time ranges.
  Ranges<base::TimeDelta> buffered_;

  // Message loops for posting tasks between Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  MessageLoop* main_loop_;

  scoped_ptr<FilterCollection> filter_collection_;
  scoped_refptr<Pipeline> pipeline_;

#if defined(COBALT_USE_PUNCHOUT)
  scoped_refptr<VideoFrame> punch_out_video_frame_;
#endif  // defined(COBALT_USE_PUNCHOUT)

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  std::string current_key_system_;

  scoped_ptr<MessageLoopFactory> message_loop_factory_;

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
  scoped_refptr<ShellVideoFrameProvider> video_frame_provider_;

  scoped_refptr<WebMediaPlayerProxy> proxy_;

  scoped_refptr<MediaLog> media_log_;

  bool incremented_externally_allocated_memory_;

  scoped_refptr<AudioRendererSink> audio_renderer_sink_;

  bool is_local_source_;
  bool supports_save_;

  // The decryptor that manages decryption keys and decrypts encrypted frames.
  scoped_ptr<ProxyDecryptor> decryptor_;

  scoped_refptr<ChunkDemuxer> chunk_demuxer_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly.
  std::string init_data_type_;

#if defined(__LB_ANDROID__)
  AudioFocusBridge audio_focus_bridge_;
#endif  // defined(__LB_ANDROID__)

  // Suppresses calls to OnPipelineError() after destruction / shutdown has been
  // started; prevents us from spuriously logging errors that are transient or
  // unimportant.
  bool suppress_destruction_errors_;

  base::Callback<void(base::TimeDelta*, bool*)>
      media_time_and_seeking_state_cb_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace media

#endif  // MEDIA_PLAYER_WEB_MEDIA_PLAYER_IMPL_H_
