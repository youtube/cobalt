// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_H_
#define COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_H_

// The temporary home for WebMediaPlayer and WebMediaPlayerClient. They are the
// interface between the HTMLMediaElement and the media stack.

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "cobalt/media/base/ranges.h"
#include "cobalt/media/base/video_frame_provider.h"
#include "cobalt/media/filters/chunk_demuxer.h"
#include "cobalt/media/player/buffered_data_source.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cobalt {
namespace media {

class DrmSystem;

class WebMediaPlayer {
 public:
  // Return true if the punch through box should be rendered.  Return false if
  // no punch through box should be rendered.
  typedef base::Callback<bool(const gfx::Rect&)> SetBoundsCB;

  enum NetworkState {
    kNetworkStateEmpty,
    kNetworkStateIdle,
    kNetworkStateLoading,
    kNetworkStateLoaded,
    kNetworkStateFormatError,
    kNetworkStateNetworkError,
    kNetworkStateDecodeError,
    kNetworkStateCapabilityChangedError,
  };

  enum ReadyState {
    kReadyStateHaveNothing,
    kReadyStateHaveMetadata,
    kReadyStateHaveCurrentData,
    kReadyStateHaveFutureData,
    kReadyStateHaveEnoughData,
  };

  enum AddIdStatus {
    kAddIdStatusOk,
    kAddIdStatusNotSupported,
    kAddIdStatusReachedIdLimit
  };

  enum EndOfStreamStatus {
    kEndOfStreamStatusNoError,
    kEndOfStreamStatusNetworkError,
    kEndOfStreamStatusDecodeError,
  };

  // Represents synchronous exceptions that can be thrown from the Encrypted
  // Media methods. This is different from the asynchronous MediaKeyError.
  enum MediaKeyException {
    kMediaKeyExceptionNoError,
    kMediaKeyExceptionInvalidPlayerState,
    kMediaKeyExceptionKeySystemNotSupported,
  };

  enum CORSMode {
    kCORSModeUnspecified,
    kCORSModeAnonymous,
    kCORSModeUseCredentials,
  };

  virtual ~WebMediaPlayer() {}

#if SB_HAS(PLAYER_WITH_URL)
  virtual void LoadUrl(const GURL& url) = 0;
#else   // SB_HAS(PLAYER_WITH_URL)
  virtual void LoadMediaSource() = 0;
  virtual void LoadProgressive(const GURL& url,
                               scoped_ptr<BufferedDataSource> data_source) = 0;
#endif  // SB_HAS(PLAYER_WITH_URL)

  virtual void CancelLoad() = 0;

  // Playback controls.
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual bool SupportsFullscreen() const = 0;
  virtual bool SupportsSave() const = 0;
  virtual void Seek(float seconds) = 0;
  virtual void SetEndTime(float seconds) = 0;
  virtual void SetRate(float rate) = 0;
  virtual void SetVolume(float volume) = 0;
  virtual void SetVisible(bool visible) = 0;
  virtual const Ranges<base::TimeDelta>& GetBufferedTimeRanges() = 0;
  virtual float GetMaxTimeSeekable() const = 0;

  // Suspend/Resume
  virtual void Suspend() = 0;
  virtual void Resume() = 0;

  // True if the loaded media has a playable video/audio track.
  virtual bool HasVideo() const = 0;
  virtual bool HasAudio() const = 0;

  // Dimension of the video.
  virtual gfx::Size GetNaturalSize() const = 0;

  // Getters of playback state.
  virtual bool IsPaused() const = 0;
  virtual bool IsSeeking() const = 0;
  virtual float GetDuration() const = 0;
#if SB_HAS(PLAYER_WITH_URL)
  virtual base::Time GetStartDate() const = 0;
#endif  // SB_HAS(PLAYER_WITH_URL)
  virtual float GetCurrentTime() const = 0;

  // Get rate of loading the resource.
  virtual int GetDataRate() const = 0;

  // Internal states of loading and network.
  virtual NetworkState GetNetworkState() const = 0;
  virtual ReadyState GetReadyState() const = 0;

  virtual bool DidLoadingProgress() const = 0;

  virtual bool HasSingleSecurityOrigin() const = 0;
  virtual bool DidPassCORSAccessCheck() const = 0;

  virtual float MediaTimeForTimeValue(float timeValue) const = 0;

  virtual unsigned GetDecodedFrameCount() const = 0;
  virtual unsigned GetDroppedFrameCount() const = 0;
  virtual unsigned GetAudioDecodedByteCount() const = 0;
  virtual unsigned GetVideoDecodedByteCount() const = 0;

  virtual scoped_refptr<VideoFrameProvider> GetVideoFrameProvider() {
    return NULL;
  }

  virtual AddIdStatus SourceAddId(
      const std::string& /* id */, const std::string& /* type */,
      const std::vector<std::string>& /* codecs */) {
    return kAddIdStatusNotSupported;
  }
  virtual bool SourceRemoveId(const std::string& /* id */) { return false; }
  virtual Ranges<base::TimeDelta> SourceBuffered(const std::string& /* id */) {
    return Ranges<base::TimeDelta>();
  }
  virtual bool SourceAppend(const std::string& /* id */,
                            const unsigned char* /* data */,
                            unsigned /* length */) {
    return false;
  }
  virtual bool SourceAbort(const std::string& /* id */) { return false; }
  virtual double SourceGetDuration() const { return 0.0; }
  virtual void SourceSetDuration(double /* duration */) {}
  virtual void SourceEndOfStream(EndOfStreamStatus /* status */) {}
  virtual bool SourceSetTimestampOffset(const std::string& /* id */,
                                        double /* offset */) {
    return false;
  }

  virtual SetBoundsCB GetSetBoundsCB() { return SetBoundsCB(); }

  // Instruct WebMediaPlayer to enter/exit fullscreen.
  virtual void EnterFullscreen() {}
  virtual void ExitFullscreen() {}
  // Returns true if the player can enter fullscreen.
  virtual bool CanEnterFullscreen() const { return false; }

  // Returns the address and size of a chunk of memory to be included in a
  // debug report. May not be supported on all platforms. The returned address
  // should remain valid as long as the WebMediaPlayer instance is alive.
  virtual bool GetDebugReportDataAddress(void** /*out_address*/,
                                         size_t* /*out_size*/) {
    return false;
  }

  // Sets the DRM system which must be initialized with the data passed
  // in |WebMediaPlayerClient::EncryptedMediaInitData|.
  //
  // |drm_system| must not be NULL. The method can only be called once.
  virtual void SetDrmSystem(DrmSystem* drm_system) = 0;
};

// TODO: Add prefix "On" to all methods that handle events, such
//       as |NetworkStateChanged|, |SourceOpened|, |EncryptedMediaInitData|.
class WebMediaPlayerClient {
 public:
  virtual void NetworkStateChanged() = 0;
  virtual void NetworkError(const std::string&) = 0;
  virtual void ReadyStateChanged() = 0;
  virtual void TimeChanged(bool eos_played) = 0;
  virtual void DurationChanged() = 0;
  virtual void OutputModeChanged() = 0;
  virtual void ContentSizeChanged() = 0;
  virtual void PlaybackStateChanged() = 0;
  // TODO: Revisit the necessity of the following function.
  virtual void SetOpaque(bool /* opaque */) {}
  virtual void SawUnsupportedTracks() = 0;
  virtual float Volume() const = 0;
  virtual void SourceOpened(ChunkDemuxer* chunk_demuxer) = 0;
  virtual std::string SourceURL() const = 0;
  // Clients should implement this in order to indicate a preference for whether
  // a video should be decoded to a texture or through a punch out system.  If
  // the preferred output mode is not supported, the player will fallback to
  // one that is.  This can be used to indicate that, say, for spherical video
  // playback, we would like a decode-to-texture output mode.
  virtual bool PreferDecodeToTexture() { return false; }
  // Notifies the client that a video is encrypted. Client is supposed to call
  // |WebMediaPlayer::SetDrmSystem| as soon as possible to avoid stalling
  // playback.
  virtual void EncryptedMediaInitDataEncountered(EmeInitDataType init_data_type,
                                                 const unsigned char* init_data,
                                                 unsigned init_data_length) = 0;
  // TODO: Revisit the necessity of the following functions.
  virtual void CloseHelperPlugin() { NOTREACHED(); }
  virtual void DisableAcceleratedCompositing() {}

 protected:
  ~WebMediaPlayerClient() {}
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_H_
