// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PLAYER_WEB_MEDIA_PLAYER_H_
#define MEDIA_PLAYER_WEB_MEDIA_PLAYER_H_

// The temporary home for WebMediaPlayer and WebMediaPlayerClient. They are the
// interface between the HTMLMediaElement and the media stack.

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "media/base/ranges.h"
#include "media/base/shell_video_frame_provider.h"
#include "media/base/video_frame.h"
#include "media/player/buffered_data_source.h"
#include "ui/gfx/size.h"

// Disable `unreferenced formal parameter` as we have many stub functions in
// this file and we want to keep their parameters.
MSVC_PUSH_DISABLE_WARNING(4100)

namespace media {

class WebMediaPlayer {
 public:
  enum NetworkState {
    kNetworkStateEmpty,
    kNetworkStateIdle,
    kNetworkStateLoading,
    kNetworkStateLoaded,
    kNetworkStateFormatError,
    kNetworkStateNetworkError,
    kNetworkStateDecodeError,
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

  virtual void LoadMediaSource() = 0;
  virtual void LoadProgressive(
      const GURL& url,
      const scoped_refptr<BufferedDataSource>& data_source,
      CORSMode cors_mode) = 0;
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
  virtual const Ranges<base::TimeDelta>& Buffered() = 0;
  virtual float MaxTimeSeekable() const = 0;

  // True if the loaded media has a playable video/audio track.
  virtual bool HasVideo() const = 0;
  virtual bool HasAudio() const = 0;

  // Dimension of the video.
  virtual gfx::Size NaturalSize() const = 0;

  // Getters of playback state.
  virtual bool Paused() const = 0;
  virtual bool Seeking() const = 0;
  virtual float Duration() const = 0;
  virtual float CurrentTime() const = 0;

  // Get rate of loading the resource.
  virtual int DataRate() const = 0;

  // Internal states of loading and network.
  virtual NetworkState GetNetworkState() const = 0;
  virtual ReadyState GetReadyState() const = 0;

  virtual bool DidLoadingProgress() const = 0;
  virtual unsigned long long TotalBytes() const = 0;

  virtual bool HasSingleSecurityOrigin() const = 0;
  virtual bool DidPassCORSAccessCheck() const = 0;

  virtual float MediaTimeForTimeValue(float timeValue) const = 0;

  virtual unsigned DecodedFrameCount() const = 0;
  virtual unsigned DroppedFrameCount() const = 0;
  virtual unsigned AudioDecodedByteCount() const = 0;
  virtual unsigned VideoDecodedByteCount() const = 0;

  virtual ShellVideoFrameProvider* GetVideoFrameProvider() { return NULL; }
  virtual scoped_refptr<VideoFrame> GetCurrentFrame() { return 0; }
  // We no longer need PutCurrentFrame as the the video frame returned from
  // GetCurrentFrame() is now a scoped_refptr.
  virtual void PutCurrentFrame(
      const scoped_refptr<VideoFrame>& /* video_frame */) {}

  virtual AddIdStatus SourceAddId(
      const std::string& /* id */,
      const std::string& /* type */,
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

  // Returns whether keySystem is supported. If true, the result will be
  // reported by an event.
  virtual MediaKeyException GenerateKeyRequest(
      const std::string& /* key_system */,
      const unsigned char* /* init_data */,
      unsigned /* init_data_length */) {
    return kMediaKeyExceptionKeySystemNotSupported;
  }
  virtual MediaKeyException AddKey(const std::string& /* key_system */,
                                   const unsigned char* /* key */,
                                   unsigned /* key_length */,
                                   const unsigned char* /* init_data */,
                                   unsigned /* init_data_length */,
                                   const std::string& /* session_id */) {
    return kMediaKeyExceptionKeySystemNotSupported;
  }
  virtual MediaKeyException CancelKeyRequest(
      const std::string& /* key_system */,
      const std::string& /* session_id */) {
    return kMediaKeyExceptionKeySystemNotSupported;
  }

  // Instruct WebMediaPlayer to enter/exit fullscreen.
  virtual void EnterFullscreen() {}
  virtual void ExitFullscreen() {}
  // Returns true if the player can enter fullscreen.
  virtual bool CanEnterFullscreen() const { return false; }
};

class WebMediaPlayerClient {
 public:
  enum MediaKeyErrorCode {
    kMediaKeyErrorCodeUnknown = 1,
    kMediaKeyErrorCodeClient,
    kMediaKeyErrorCodeService,
    kMediaKeyErrorCodeOutput,
    kMediaKeyErrorCodeHardwareChange,
    kMediaKeyErrorCodeDomain,
    kUnknownError = kMediaKeyErrorCodeUnknown,
    kClientError = kMediaKeyErrorCodeClient,
    kServiceError = kMediaKeyErrorCodeService,
    kOutputError = kMediaKeyErrorCodeOutput,
    kHardwareChangeError = kMediaKeyErrorCodeHardwareChange,
    kDomainError = kMediaKeyErrorCodeDomain,
  };

  virtual void NetworkStateChanged() = 0;
  virtual void ReadyStateChanged() = 0;
  virtual void TimeChanged() = 0;
  virtual void DurationChanged() = 0;
  virtual void RateChanged() = 0;
  virtual void SizeChanged() = 0;
  virtual void PlaybackStateChanged() = 0;
  virtual void Repaint() = 0;
  // TODO(***REMOVED***) : Revisit the necessity of the following function.
  virtual void SetOpaque(bool /* opaque */) {}
  virtual void SawUnsupportedTracks() = 0;
  virtual float Volume() const = 0;
  virtual void SourceOpened() = 0;
  virtual std::string SourceURL() const = 0;
  // TODO(***REMOVED***) : Make the EME related functions pure virtual again once
  // we have proper EME implementation. Currently empty implementation are
  // provided to make media temporarily work.
  virtual void KeyAdded(const std::string& /* key_system */,
                        const std::string& /* session_id */) {
    NOTIMPLEMENTED();
  }
  virtual void KeyError(const std::string& /* key_system */,
                        const std::string& /* session_id */,
                        MediaKeyErrorCode,
                        unsigned short /* system_code */) {
    NOTIMPLEMENTED();
  }
  virtual void KeyMessage(const std::string& /* key_system */,
                          const std::string& /* session_id */,
                          const unsigned char* /* message */,
                          unsigned /* message_length */,
                          const std::string& /* default_url */) {
    NOTIMPLEMENTED();
  }
  virtual void KeyNeeded(const std::string& /* key_system */,
                         const std::string& /* session_id */,
                         const unsigned char* /* init_data */,
                         unsigned /* init_data_length */) {
    NOTIMPLEMENTED();
  }
  // TODO(***REMOVED***) : Revisit the necessity of the following functions.
  virtual void CloseHelperPlugin() { NOTREACHED(); }
  virtual void DisableAcceleratedCompositing() {}

 protected:
  ~WebMediaPlayerClient() {}
};

// An interface to allow a WebMediaPlayerImpl to communicate changes of state
// to objects that need to know.
class WebMediaPlayerDelegate : base::SupportsWeakPtr<WebMediaPlayerDelegate> {
 public:
  WebMediaPlayerDelegate() {}

  // The specified player started playing media.
  virtual void DidPlay(WebMediaPlayer* /* player */) {}

  // The specified player stopped playing media.
  virtual void DidPause(WebMediaPlayer* /* player */) {}

  // The specified player was destroyed. Do not call any methods on it.
  virtual void PlayerGone(WebMediaPlayer* /* player */) {}

 protected:
  ~WebMediaPlayerDelegate() {}
};

}  // namespace media

MSVC_POP_WARNING()

#endif  // MEDIA_PLAYER_WEB_MEDIA_PLAYER_H_
