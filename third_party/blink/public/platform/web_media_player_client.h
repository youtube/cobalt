/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_MEDIA_PLAYER_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_MEDIA_PLAYER_CLIENT_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/media/display_type.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "ui/gfx/color_space.h"

#include "third_party/blink/public/platform/web_texttrack_metadata.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class Layer;
}

namespace media {
enum class MediaContentType;
enum class VideoCodec;
enum class AudioCodec;
}  // namespace media

namespace blink {

class WebInbandTextTrack;
class WebMediaSource;
class WebRemotePlaybackClient;

class BLINK_PLATFORM_EXPORT WebMediaPlayerClient {
 public:
  enum VideoTrackKind {
    kVideoTrackKindNone,
    kVideoTrackKindAlternative,
    kVideoTrackKindCaptions,
    kVideoTrackKindMain,
    kVideoTrackKindSign,
    kVideoTrackKindSubtitles,
    kVideoTrackKindCommentary
  };

  enum AudioTrackKind {
    kAudioTrackKindNone,
    kAudioTrackKindAlternative,
    kAudioTrackKindDescriptions,
    kAudioTrackKindMain,
    kAudioTrackKindMainDescriptions,
    kAudioTrackKindTranslation,
    kAudioTrackKindCommentary
  };

  // Reason for a PausePlayback call, for better diagnostic messages.
  enum class PauseReason {
    kUnknown,
    kBackgroundVideoOptimization,
    kSuspendedPlayerIdleTimeout,
    kRemotePlayStateChange,
  };

  static const int kMediaRemotingStopNoText = -1;

  virtual void NetworkStateChanged() = 0;
  virtual void ReadyStateChanged() = 0;
  virtual void TimeChanged() = 0;
  virtual void Repaint() = 0;
  virtual void DurationChanged() = 0;
  virtual void SizeChanged() = 0;
  virtual void SetCcLayer(cc::Layer*) = 0;
  virtual WebMediaPlayer::TrackId AddAudioTrack(const WebString& id,
                                                AudioTrackKind,
                                                const WebString& label,
                                                const WebString& language,
                                                bool enabled) = 0;
  virtual void RemoveAudioTrack(WebMediaPlayer::TrackId) = 0;
  virtual WebMediaPlayer::TrackId AddVideoTrack(const WebString& id,
                                                VideoTrackKind,
                                                const WebString& label,
                                                const WebString& language,
                                                bool selected) = 0;
  virtual void RemoveVideoTrack(WebMediaPlayer::TrackId) = 0;
  virtual void AddTextTrack(WebInbandTextTrack*) = 0;
  virtual void RemoveTextTrack(WebInbandTextTrack*) = 0;
  virtual void MediaSourceOpened(WebMediaSource*) = 0;
  virtual void RemotePlaybackCompatibilityChanged(const WebURL&,
                                                  bool is_compatible) = 0;

  // Returns whether the media element has always been muted. This is used to
  // avoid take audio focus for elements that the user is not aware is playing.
  virtual bool WasAlwaysMuted() = 0;

  // Returns if there's a selected video track.
  virtual bool HasSelectedVideoTrack() = 0;

  // Returns the selected video track id (or an empty id if there's none).
  virtual WebMediaPlayer::TrackId GetSelectedVideoTrackId() = 0;

  // Informs that media starts being rendered and played back remotely.
  // |remote_device_friendly_name| will be shown in the remoting UI to indicate
  // which device the content is rendered on. An empty name indicates an unknown
  // remote device. A default message will be shown in this case.
  virtual void MediaRemotingStarted(
      const WebString& remote_device_friendly_name) = 0;

  // Informs that media stops being rendered remotely. |error_code| corresponds
  // to a localized string that explains the reason as user-readable text.
  // |error_code| should be IDS_FOO or kMediaRemotingStopNoText.
  virtual void MediaRemotingStopped(int error_code) = 0;

  // Returns whether the media element has native controls. It does not mean
  // that the controls are currently visible.
  virtual bool HasNativeControls() = 0;

  // Returns true iff the client represents an HTML <audio> element.
  virtual bool IsAudioElement() = 0;

  // Returns the current display type of the media element.
  virtual DisplayType GetDisplayType() const = 0;

  // Returns the remote playback client associated with the media element, if
  // any.
  virtual WebRemotePlaybackClient* RemotePlaybackClient() { return nullptr; }

  // Returns metadata for out-of-band text tracks declared as <track> elements.
  virtual Vector<TextTrackMetadata> GetTextTrackMetadata() = 0;

  // Returns the color space to render media into if.
  // Rendering media into this color space may avoid some conversions.
  virtual gfx::ColorSpace TargetColorSpace() { return gfx::ColorSpace(); }

  // Returns whether the media element was initiated via autoplay.
  // In this context, autoplay means that it was initiated before any user
  // activation was received on the page and before a user initiated same-domain
  // navigation. In other words, with the unified autoplay policy applied, it
  // should only return `true` when MEI allowed autoplay.
  virtual bool WasAutoplayInitiated() { return false; }

  // Returns true if playback would start if the ready state was at least
  // WebMediaPlayer::kReadyStateHaveFutureData.
  virtual bool CouldPlayIfEnoughData() const = 0;

  // Returns whether the playback is in auto-pip mode which does not have th
  // behavior as regular Picture-in-Picture.
  virtual bool IsInAutoPIP() const = 0;

  // Requests the player to resume playback.
  virtual void ResumePlayback() = 0;

  // Request the player to pause playback.
  virtual void PausePlayback(PauseReason) = 0;

  // Notify the client that the media player started playing content.
  virtual void DidPlayerStartPlaying() = 0;

  // Notify the client that the media player stopped playing content, indicating
  // in |stream_ended| if playback has reached the end of the stream.
  virtual void DidPlayerPaused(bool stream_ended) = 0;

  // Notify the client that the muted status of the media player has changed.
  virtual void DidPlayerMutedStatusChange(bool muted) = 0;

  // Notify the client that the media metadata of the media player has changed.
  virtual void DidMediaMetadataChange(
      bool has_audio,
      bool has_video,
      media::AudioCodec audio_codec,
      media::VideoCodec video_codec,
      media::MediaContentType media_content_type,
      bool is_encrypted_media) = 0;

  // Notify the client that the playback position has changed.
  virtual void DidPlayerMediaPositionStateChange(double playback_rate,
                                                 base::TimeDelta duration,
                                                 base::TimeDelta position,
                                                 bool end_of_media) = 0;

  // Notify the client that the audio sink cannot be changed.
  virtual void DidDisableAudioOutputSinkChanges() = 0;

  // Notify the client that the playback starts/stops to use AudioService.
  virtual void DidUseAudioServiceChange(bool uses_audio_service) = 0;

  // Notify the client that the size of the media player has changed.
  // TODO(crbug.com/1039252): Remove by merging this method into SizeChanged().
  virtual void DidPlayerSizeChange(const gfx::Size& size) = 0;

  virtual void OnFirstFrame(base::TimeTicks first_frame,
                            size_t bytes_to_first_frame) = 0;

  // Notify the client that one of the state used by Picture-in-Picture has
  // changed. The client will then have to poll the states from the associated
  // WebMediaPlayer.
  // The states are:
  //  - Delegate ID;
  //  - Surface ID;
  //  - Natural Size.
  virtual void OnPictureInPictureStateChange() = 0;

  // Called when a video frame has been presented to the compositor, after a
  // request was initiated via WebMediaPlayer::RequestVideoFrameCallback().
  // See https://wicg.github.io/video-rvfc/.
  virtual void OnRequestVideoFrameCallback() {}

  // Notify the client that the RemotePlayback has been disabled/enabled.
  virtual void OnRemotePlaybackDisabled(bool disabled) = 0;

 protected:
  ~WebMediaPlayerClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_MEDIA_PLAYER_CLIENT_H_
