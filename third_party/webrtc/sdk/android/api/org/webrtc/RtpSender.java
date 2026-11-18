/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;
import java.util.List;
import org.jni_zero.NativeMethods;

/** Java wrapper for a C++ RtpSenderInterface. */
public class RtpSender {
  private long nativeRtpSender;

  @Nullable private MediaStreamTrack cachedTrack;
  private boolean ownsTrack = true;
  private final @Nullable DtmfSender dtmfSender;

  @CalledByNative
  public RtpSender(long nativeRtpSender) {
    this.nativeRtpSender = nativeRtpSender;
    long nativeTrack = RtpSenderJni.get().getTrack(nativeRtpSender);
    cachedTrack = MediaStreamTrack.createMediaStreamTrack(nativeTrack);

    if (RtpSenderJni.get()
        .getMediaType(nativeRtpSender)
        .equalsIgnoreCase(MediaStreamTrack.AUDIO_TRACK_KIND)) {
      long nativeDtmfSender = RtpSenderJni.get().getDtmfSender(nativeRtpSender);
      dtmfSender = (nativeDtmfSender != 0) ? new DtmfSender(nativeDtmfSender) : null;
    } else {
      dtmfSender = null;
    }
  }

  /**
   * Starts sending a new track, without requiring additional SDP negotiation.
   * <p>
   * Note: This is equivalent to replaceTrack in the official WebRTC API. It
   * was just implemented before the standards group settled on a name.
   *
   * @param takeOwnership If true, the RtpSender takes ownership of the track
   *                      from the caller, and will auto-dispose of it when no
   *                      longer needed. `takeOwnership` should only be used if
   *                      the caller owns the track; it is not appropriate when
   *                      the track is owned by, for example, another RtpSender
   *                      or a MediaStream.
   * @return              true on success and false on failure.
   */
  public boolean setTrack(@Nullable MediaStreamTrack track, boolean takeOwnership) {
    checkRtpSenderExists();
    if (!RtpSenderJni.get()
        .setTrack(nativeRtpSender, (track == null) ? 0 : track.getNativeMediaStreamTrack())) {
      return false;
    }
    if (cachedTrack != null && ownsTrack) {
      cachedTrack.dispose();
    }
    cachedTrack = track;
    ownsTrack = takeOwnership;
    return true;
  }

  @Nullable
  public MediaStreamTrack track() {
    return cachedTrack;
  }

  public void setStreams(List<String> streamIds) {
    checkRtpSenderExists();
    RtpSenderJni.get().setStreams(nativeRtpSender, streamIds);
  }

  public List<String> getStreams() {
    checkRtpSenderExists();
    return RtpSenderJni.get().getStreams(nativeRtpSender);
  }

  public boolean setParameters(RtpParameters parameters) {
    checkRtpSenderExists();
    return RtpSenderJni.get().setParameters(nativeRtpSender, parameters);
  }

  public RtpParameters getParameters() {
    checkRtpSenderExists();
    return RtpSenderJni.get().getParameters(nativeRtpSender);
  }

  public String id() {
    checkRtpSenderExists();
    return RtpSenderJni.get().getId(nativeRtpSender);
  }

  @Nullable
  public DtmfSender dtmf() {
    return dtmfSender;
  }

  public void setFrameEncryptor(FrameEncryptor frameEncryptor) {
    checkRtpSenderExists();
    RtpSenderJni.get().setFrameEncryptor(nativeRtpSender, frameEncryptor.getNativeFrameEncryptor());
  }

  public void dispose() {
    checkRtpSenderExists();
    if (dtmfSender != null) {
      dtmfSender.dispose();
    }
    if (cachedTrack != null && ownsTrack) {
      cachedTrack.dispose();
    }
    JniCommon.nativeReleaseRef(nativeRtpSender);
    nativeRtpSender = 0;
  }

  /** Returns a pointer to webrtc::RtpSenderInterface. */
  long getNativeRtpSender() {
    checkRtpSenderExists();
    return nativeRtpSender;
  }

  private void checkRtpSenderExists() {
    if (nativeRtpSender == 0) {
      throw new IllegalStateException("RtpSender has been disposed.");
    }
  }

  @NativeMethods
  interface Natives {
    boolean setTrack(long rtpSender, long nativeTrack);

    long getTrack(long rtpSender);

    void setStreams(long rtpSender, List<String> streamIds);

    List<String> getStreams(long rtpSender);

    long getDtmfSender(long rtpSender);

    boolean setParameters(long rtpSender, RtpParameters parameters);

    RtpParameters getParameters(long rtpSender);

    String getId(long rtpSender);

    void setFrameEncryptor(long rtpSender, long nativeFrameEncryptor);

    String getMediaType(long rtpSender);
  }
}
