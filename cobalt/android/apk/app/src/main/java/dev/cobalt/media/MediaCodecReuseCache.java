// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.media.MediaCrypto;
import android.view.Surface;
import androidx.annotation.GuardedBy;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;

/**
 * Dedicated cache layer for managing MediaCodec reuse across consecutive video playbacks.
 *
 * <p>When enabled, compatible hardware video decoders are preserved across video transitions
 * instead of being torn down and recreated, reducing video startup latency.
 */
public class MediaCodecReuseCache {

  /** Metadata and reference for a cached MediaCodecBridge instance awaiting reuse. */
  public static class CachedCodec {
    public final MediaCodecBridge bridge;
    public final String mime;
    public final String decoderName;
    public final Surface surface;
    public final int configuredWidth;
    public final int configuredHeight;
    public final int maxWidth;
    public final int maxHeight;
    public final boolean isTunneling;
    public final boolean hasCrypto;
    public final boolean isHdr;

    public CachedCodec(
        MediaCodecBridge bridge,
        String mime,
        String decoderName,
        Surface surface,
        int configuredWidth,
        int configuredHeight,
        int maxWidth,
        int maxHeight,
        boolean isTunneling,
        boolean hasCrypto,
        boolean isHdr) {
      this.bridge = bridge;
      this.mime = mime;
      this.decoderName = decoderName;
      this.surface = surface;
      this.configuredWidth = configuredWidth;
      this.configuredHeight = configuredHeight;
      this.maxWidth = maxWidth;
      this.maxHeight = maxHeight;
      this.isTunneling = isTunneling;
      this.hasCrypto = hasCrypto;
      this.isHdr = isHdr;
    }

    /**
     * Determines whether the cached codec is compatible with the incoming video configuration.
     *
     * <p>Requires an exact resolution match (Option A) to avoid hardware buffer exhaustion,
     * pipeline stalls, and frame rate conversion lag when transitioning across distinct aspect
     * ratios (e.g., Shorts vs VoD).
     */
    public boolean isCompatible(
        String reqMime,
        String reqDecoderName,
        int reqWidth,
        int reqHeight,
        Surface reqSurface,
        int reqMaxWidth,
        int reqMaxHeight,
        boolean reqTunneling,
        boolean reqCrypto,
        boolean reqHdr) {
      if (!mime.equals(reqMime)) {
        return false;
      }
      if (!decoderName.equals(reqDecoderName)) {
        return false;
      }
      if (surface != reqSurface || surface == null || !surface.isValid() || !reqSurface.isValid()) {
        return false;
      }
      if (isTunneling != reqTunneling) {
        return false;
      }
      if (hasCrypto != reqCrypto) {
        return false;
      }
      if (isHdr != reqHdr) {
        return false;
      }
      if (reqMaxWidth > maxWidth || reqMaxHeight > maxHeight) {
        return false;
      }
      if (configuredWidth != reqWidth || configuredHeight != reqHeight) {
        return false;
      }
      return true;
    }
  }

  private static final Object sCacheLock = new Object();

  @GuardedBy("sCacheLock")
  private static CachedCodec sCachedCodec = null;

  /**
   * Attempts to acquire a compatible cached MediaCodecBridge for the requested playback session.
   *
   * @return A reconfigured, started MediaCodecBridge if reuse succeeded, or {@code null} if no
   *     compatible codec was available or restart failed.
   */
  public static MediaCodecBridge acquire(
      long nativeMediaCodecBridge,
      String mime,
      String decoderName,
      int widthHint,
      int heightHint,
      int fps,
      int maxWidth,
      int maxHeight,
      Surface surface,
      MediaCrypto crypto,
      boolean isHdr,
      int tunnelModeAudioSessionId,
      boolean skipVideoFramesOver60Fps) {
    synchronized (sCacheLock) {
      if (sCachedCodec == null) {
        return null;
      }

      boolean reqTunneling = tunnelModeAudioSessionId != TunnelModeAudioSessionId.NONE;
      boolean reqCrypto = crypto != null;

      if (sCachedCodec.isCompatible(
          mime,
          decoderName,
          widthHint,
          heightHint,
          surface,
          maxWidth,
          maxHeight,
          reqTunneling,
          reqCrypto,
          isHdr)) {
        Log.i(TAG, "Reusing cached MediaCodec for " + decoderName);
        MediaCodecBridge bridge = sCachedCodec.bridge;
        sCachedCodec = null;

        bridge.prepareForReuse(nativeMediaCodecBridge, skipVideoFramesOver60Fps, fps);
        MediaCodecOutputTracker.get().register(bridge);

        try {
          bridge.startMediaCodec();
          bridge.setAwaitingFirstFrameAfterReuse(true);
          return bridge;
        } catch (Exception e) {
          Log.e(TAG, "Failed to restart cached MediaCodec, falling back to new instance: ", e);
          bridge.doRelease();
          return null;
        }
      } else {
        Log.i(
            TAG,
            "Cached MediaCodec is incompatible (cached="
                + sCachedCodec.mime
                + "/"
                + sCachedCodec.decoderName
                + "/"
                + sCachedCodec.configuredWidth
                + "x"
                + sCachedCodec.configuredHeight
                + ", requested="
                + mime
                + "/"
                + decoderName
                + "/"
                + widthHint
                + "x"
                + heightHint
                + "), evicting.");
        discardInternal();
        return null;
      }
    }
  }

  /** Evaluates if a MediaCodecBridge instance is eligible to be cached for reuse upon teardown. */
  public static boolean isEligibleForReuse(MediaCodecBridge bridge) {
    if (bridge == null) {
      return false;
    }
    if (!bridge.isReuseEnabled()) {
      return false;
    }
    if (bridge.getSurface() == null || !bridge.getSurface().isValid()) {
      return false;
    }
    if (bridge.isTunnelingPlayback()) {
      return false;
    }
    if (bridge.hasCrypto()) {
      return false;
    }
    if (bridge.isHdr()) {
      return false;
    }
    if (bridge.getMime() == null) {
      return false;
    }
    if (bridge.hasEncounteredError() || !bridge.isMediaCodecSet()) {
      return false;
    }
    return true;
  }

  /**
   * Called when a MediaCodecBridge is being released. If eligible, caches the bridge and defers its
   * release.
   *
   * @return {@code true} if the bridge was cached for reuse, {@code false} if it should be released
   *     immediately.
   */
  public static boolean maybeCacheOnRelease(MediaCodecBridge bridge) {
    if (!isEligibleForReuse(bridge)) {
      return false;
    }

    synchronized (sCacheLock) {
      discardInternal();

      bridge.flush();
      bridge.detachNativeBridge();
      MediaCodecOutputTracker.get().unregister(bridge);

      sCachedCodec =
          new CachedCodec(
              bridge,
              bridge.getMime(),
              bridge.getCodecName(),
              bridge.getSurface(),
              bridge.getConfiguredWidth(),
              bridge.getConfiguredHeight(),
              bridge.getMaxWidth(),
              bridge.getMaxHeight(),
              bridge.isTunnelingPlayback(),
              bridge.hasCrypto(),
              bridge.isHdr());

      VideoSurfaceView.setVideoSurfaceVisible(false);
      Log.i(
          TAG, "MediaCodec cached for potential reuse across playbacks: " + bridge.getCodecName());
      return true;
    }
  }

  /** Discards any currently cached MediaCodec and releases its hardware resources. */
  public static void discard() {
    synchronized (sCacheLock) {
      discardInternal();
    }
  }

  @GuardedBy("sCacheLock")
  private static void discardInternal() {
    if (sCachedCodec != null) {
      Log.i(TAG, "Discarding cached MediaCodec: " + sCachedCodec.decoderName);
      MediaCodecBridge oldBridge = sCachedCodec.bridge;
      sCachedCodec = null;
      oldBridge.doRelease();
    }
  }

  /** Notified when a frame is rendered to the surface by the decoder. */
  public static void onFrameRendered(MediaCodecBridge bridge) {
    if (bridge != null && bridge.isAwaitingFirstFrameAfterReuse()) {
      if (!bridge.hasReleasedOutputBufferForRender()) {
        return;
      }
      bridge.setAwaitingFirstFrameAfterReuse(false);
      VideoSurfaceView.setVideoSurfaceVisible(true);
    }
  }

  /** Notified when an error occurs on the MediaCodec. */
  public static void onError(MediaCodecBridge bridge) {
    if (bridge != null && bridge.isAwaitingFirstFrameAfterReuse()) {
      bridge.setAwaitingFirstFrameAfterReuse(false);
      VideoSurfaceView.setVideoSurfaceVisible(true);
    }
  }

  /**
   * Notified when an output buffer is released for rendering (fallback when frame renderer listener
   * is absent).
   */
  public static void onOutputBufferReleased(MediaCodecBridge bridge) {
    if (bridge != null
        && bridge.isAwaitingFirstFrameAfterReuse()
        && !bridge.isFrameRendererListenerEnabled()) {
      bridge.setAwaitingFirstFrameAfterReuse(false);
      VideoSurfaceView.setVideoSurfaceVisible(true);
    }
  }

  /** Notified when a bridge is released without presenting frames. */
  public static void onBridgeReleased(MediaCodecBridge bridge) {
    if (bridge != null && bridge.isAwaitingFirstFrameAfterReuse()) {
      bridge.setAwaitingFirstFrameAfterReuse(false);
      VideoSurfaceView.setVideoSurfaceVisible(true);
    }
  }

  @VisibleForTesting
  public static void setCachedCodecForTesting(CachedCodec cachedCodec) {
    synchronized (sCacheLock) {
      sCachedCodec = cachedCodec;
    }
  }

  @VisibleForTesting
  public static CachedCodec getCachedCodecForTesting() {
    synchronized (sCacheLock) {
      return sCachedCodec;
    }
  }

  @VisibleForTesting
  public static void resetCacheForTesting() {
    synchronized (sCacheLock) {
      sCachedCodec = null;
    }
  }
}
