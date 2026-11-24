/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;
import org.jni_zero.NativeMethods;

/** Java wrapper for a C++ MediaStreamTrackInterface. */
public class MediaStreamTrack {
  public static final String AUDIO_TRACK_KIND = "audio";
  public static final String VIDEO_TRACK_KIND = "video";

  /** Tracks MediaStreamTrackInterface.TrackState */
  public enum State {
    LIVE,
    ENDED;

    @CalledByNative
    static State fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  // Must be kept in sync with webrtc::MediaType.
  public enum MediaType {
    MEDIA_TYPE_AUDIO(0),
    MEDIA_TYPE_VIDEO(1);

    private final int nativeIndex;

    private MediaType(int nativeIndex) {
      this.nativeIndex = nativeIndex;
    }

    @CalledByNative
    int getNative() {
      return nativeIndex;
    }

    @CalledByNative
    static MediaType fromNativeIndex(int nativeIndex) {
      for (MediaType type : MediaType.values()) {
        if (type.getNative() == nativeIndex) {
          return type;
        }
      }
      throw new IllegalArgumentException("Unknown native media type: " + nativeIndex);
    }
  }

  /** Factory method to create an AudioTrack or VideoTrack subclass. */
  static @Nullable MediaStreamTrack createMediaStreamTrack(long nativeTrack) {
    if (nativeTrack == 0) {
      return null;
    }
    String trackKind = MediaStreamTrackJni.get().getKind(nativeTrack);
    if (trackKind.equals(AUDIO_TRACK_KIND)) {
      return new AudioTrack(nativeTrack);
    } else if (trackKind.equals(VIDEO_TRACK_KIND)) {
      return new VideoTrack(nativeTrack);
    } else {
      return null;
    }
  }

  private long nativeTrack;

  public MediaStreamTrack(long nativeTrack) {
    if (nativeTrack == 0) {
      throw new IllegalArgumentException("nativeTrack may not be null");
    }
    this.nativeTrack = nativeTrack;
  }

  public String id() {
    checkMediaStreamTrackExists();
    return MediaStreamTrackJni.get().getId(nativeTrack);
  }

  public String kind() {
    checkMediaStreamTrackExists();
    return MediaStreamTrackJni.get().getKind(nativeTrack);
  }

  public boolean enabled() {
    checkMediaStreamTrackExists();
    return MediaStreamTrackJni.get().getEnabled(nativeTrack);
  }

  public boolean setEnabled(boolean enable) {
    checkMediaStreamTrackExists();
    return MediaStreamTrackJni.get().setEnabled(nativeTrack, enable);
  }

  public State state() {
    checkMediaStreamTrackExists();
    return MediaStreamTrackJni.get().getState(nativeTrack);
  }

  public void dispose() {
    checkMediaStreamTrackExists();
    JniCommon.nativeReleaseRef(nativeTrack);
    nativeTrack = 0;
  }

  long getNativeMediaStreamTrack() {
    checkMediaStreamTrackExists();
    return nativeTrack;
  }

  private void checkMediaStreamTrackExists() {
    if (nativeTrack == 0) {
      throw new IllegalStateException("MediaStreamTrack has been disposed.");
    }
  }

  @NativeMethods
  interface Natives {
    String getId(long track);

    String getKind(long track);

    boolean getEnabled(long track);

    boolean setEnabled(long track, boolean enabled);

    State getState(long track);
  }
}
