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
#if defined(__LB_ANDROID__)
#include "media/base/android/audio_focus_bridge.h"
#endif  // defined(__LB_ANDROID__)
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"
#include "media/base/ranges.h"
#include "media/player/crypto/key_systems.h"
#include "media/player/crypto/proxy_decryptor.h"
#include "media/player/web_media_player.h"
#include "ui/gfx/size.h"

#if defined(__LB_XB1__) || defined(__LB_XB360__)
#define LB_USE_SHELL_PIPELINE
#endif  // defined(__LB_XB1__) || defined(__LB_XB360__)

#if defined(LB_USE_SHELL_PIPELINE)
#include "chromium/media/base/shell_pipeline.h"
#endif  // defined(LB_USE_SHELL_PIPELINE)

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

  WebMediaPlayerImpl(WebMediaPlayerClient* client,
                     base::WeakPtr<WebMediaPlayerDelegate> delegate,
                     FilterCollection* collection,
                     AudioRendererSink* audio_renderer_sink,
                     MessageLoopFactory* message_loop_factory,
                     MediaLog* media_log);
  virtual ~WebMediaPlayerImpl();

  virtual void Load(const GURL& url, CORSMode cors_mode);
  virtual void CancelLoad();

  // Playback controls.
  virtual void Play();
  virtual void Pause();
  virtual bool SupportsFullscreen() const;
  virtual bool SupportsSave() const;
  virtual void Seek(float seconds);
  virtual void SetEndTime(float seconds);
  virtual void SetRate(float rate);
  virtual void SetVolume(float volume);
  virtual void SetVisible(bool visible);
  virtual bool GetTotalBytesKnown();
  virtual const Ranges<base::TimeDelta>& Buffered();
  virtual float MaxTimeSeekable() const;

  // True if the loaded media has a playable video/audio track.
  virtual bool HasVideo() const;
  virtual bool HasAudio() const;

  // Dimensions of the video.
  virtual gfx::Size NaturalSize() const;

  // Getters of playback state.
  virtual bool Paused() const;
  virtual bool Seeking() const;
  virtual float Duration() const;
  virtual float CurrentTime() const;

  // Get rate of loading the resource.
  virtual int32 DataRate() const;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  virtual WebMediaPlayer::NetworkState GetNetworkState() const;
  virtual WebMediaPlayer::ReadyState GetReadyState() const;

  virtual bool DidLoadingProgress() const;
  virtual unsigned long long TotalBytes() const;

  virtual bool HasSingleSecurityOrigin() const;
  virtual bool DidPassCORSAccessCheck() const;

  virtual float MediaTimeForTimeValue(float timeValue) const;

  virtual unsigned DecodedFrameCount() const;
  virtual unsigned DroppedFrameCount() const;
  virtual unsigned AudioDecodedByteCount() const;
  virtual unsigned VideoDecodedByteCount() const;

  virtual scoped_refptr<VideoFrame> GetCurrentFrame();
  virtual void PutCurrentFrame(const scoped_refptr<VideoFrame>& video_frame);

  virtual AddIdStatus SourceAddId(const std::string& id,
                                  const std::string& type,
                                  const std::vector<std::string>& codecs);
  virtual bool SourceRemoveId(const std::string& id);
  virtual Ranges<base::TimeDelta> SourceBuffered(const std::string& id);
  virtual bool SourceAppend(const std::string& id,
                            const unsigned char* data,
                            unsigned length);
  virtual bool SourceAbort(const std::string& id);
  virtual double SourceGetDuration() const;
  virtual void SourceSetDuration(double new_duration);
  virtual void SourceEndOfStream(EndOfStreamStatus status);
  virtual bool SourceSetTimestampOffset(const std::string& id, double offset);

  virtual MediaKeyException GenerateKeyRequest(const std::string& key_system,
                                               const unsigned char* init_data,
                                               unsigned init_data_length);

  virtual MediaKeyException AddKey(const std::string& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const std::string& session_id);

  virtual MediaKeyException CancelKeyRequest(const std::string& key_system,
                                             const std::string& session_id);

  // As we are closing the tab or even the browser, |main_loop_| is destroyed
  // even before this object gets destructed, so we need to know when
  // |main_loop_| is being destroyed and we can stop posting repaint task
  // to it.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  void Repaint();

  void OnPipelineSeek(PipelineStatus status);
  void OnPipelineEnded(PipelineStatus status);
  void OnPipelineError(PipelineStatus error);
#if defined(LB_USE_SHELL_PIPELINE)
  void OnPipelineBufferingState(ShellPipeline::BufferingState buffering_state);
#else   // defined(LB_USE_SHELL_PIPELINE)
  void OnPipelineBufferingState(Pipeline::BufferingState buffering_state);
#endif  // defined(LB_USE_SHELL_PIPELINE)
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
  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(const GURL& gurl, bool success);

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

  // Getter method to |client_|.
  WebMediaPlayerClient* GetClient();

  // Lets V8 know that player uses extra resources not managed by V8.
  void IncrementExternallyAllocatedMemory();

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
#if defined(LB_USE_SHELL_PIPELINE)
  scoped_refptr<ShellPipeline> pipeline_;
  scoped_ptr<VideoFrame> punch_out_video_frame_;
#else   // defined(LB_USE_SHELL_PIPELINE)
  scoped_refptr<Pipeline> pipeline_;
#endif  // defined(LB_USE_SHELL_PIPELINE)

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  std::string current_key_system_;

  scoped_ptr<MessageLoopFactory> message_loop_factory_;

  // Playback state.
  //
  // TODO(scherkus): we have these because Pipeline favours the simplicity of a
  // single "playback rate" over worrying about paused/stopped etc...  It forces
  // all clients to manage the pause+playback rate externally, but is that
  // really a bad thing?
  //
  // TODO(scherkus): since SetPlaybackRate(0) is asynchronous and we don't want
  // to hang the render thread during pause(), we record the time at the same
  // time we pause and then return that value in currentTime().  Otherwise our
  // clock can creep forward a little bit while the asynchronous
  // SetPlaybackRate(0) is being executed.
  bool paused_;
  bool seeking_;
  float playback_rate_;
  base::TimeDelta paused_time_;

  // Seek gets pending if another seek is in progress. Only last pending seek
  // will have effect.
  bool pending_seek_;
  float pending_seek_seconds_;

  WebMediaPlayerClient* client_;

  scoped_refptr<WebMediaPlayerProxy> proxy_;

  base::WeakPtr<WebMediaPlayerDelegate> delegate_;

  scoped_refptr<MediaLog> media_log_;

  // Since accelerated compositing status is only known after the first layout,
  // we delay reporting it to UMA until that time.
  bool accelerated_compositing_reported_;

  bool incremented_externally_allocated_memory_;

  scoped_refptr<AudioRendererSink> audio_renderer_sink_;

  bool is_local_source_;
  bool supports_save_;

  // The decryptor that manages decryption keys and decrypts encrypted frames.
  scoped_ptr<ProxyDecryptor> decryptor_;

  bool starting_;

  scoped_refptr<ChunkDemuxer> chunk_demuxer_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly.
  std::string init_data_type_;

#if defined(__LB_ANDROID__)
  AudioFocusBridge audio_focus_bridge_;
#endif  // defined(__LB_ANDROID__)

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace media

#endif  // MEDIA_PLAYER_WEB_MEDIA_PLAYER_IMPL_H_
