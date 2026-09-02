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
 *
 * <p><strong>Lifetime and Ownership:</strong> This is a process-wide singleton cache whose lifetime
 * is bound to the application process. It holds a single cached codec instance which is owned by
 * the cache until it is either acquired for a new playback session or discarded. A codec is only
 * added to the cache after it has been fully detached from its player, so a cached codec is never
 * shared with an active player. While caching is stopped (see {@link #stopCaching()}), codecs that
 * players let go of are released instead of cached.
 *
 * <p><strong>Threading Model:</strong> This class is thread-safe. All public methods can be called
 * from any thread (e.g., player thread, main thread), and internal state is protected by a static
 * lock.
 */
public class MediaCodecCache {

  /** Configuration of a video MediaCodec instance. */
  public static class CodecConfig {
    final String mMime;
    final String mDecoderName;
    final Surface mSurface;
    final int mConfiguredWidth;
    final int mConfiguredHeight;
    final int mMaxWidth;
    final int mMaxHeight;
    final boolean mIsTunneling;
    final boolean mHasCrypto;
    final boolean mIsHdr;

    public CodecConfig(
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
      mMime = mime;
      mDecoderName = decoderName;
      mSurface = surface;
      mConfiguredWidth = configuredWidth;
      mConfiguredHeight = configuredHeight;
      mMaxWidth = maxWidth;
      mMaxHeight = maxHeight;
      mIsTunneling = isTunneling;
      mHasCrypto = hasCrypto;
      mIsHdr = isHdr;
    }

    /** Returns true if this config is eligible for reuse upon teardown. */
    public boolean isEligibleForReuse() {
      return mMime != null
          && mSurface != null
          && mSurface.isValid()
          && !mIsTunneling
          && !mHasCrypto
          && !mIsHdr;
    }

    /**
     * Determines whether the cached codec is compatible with the incoming video configuration.
     *
     * <p>Requires an exact resolution match (Option A) to avoid hardware buffer exhaustion,
     * pipeline stalls, and frame rate conversion lag when transitioning across distinct aspect
     * ratios (e.g., Shorts vs VoD).
     */
    public boolean isCompatible(CodecConfig req) {
      if (req == null) {
        return false;
      }
      if (!mMime.equals(req.mMime)) {
        return false;
      }
      if (!mDecoderName.equals(req.mDecoderName)) {
        return false;
      }
      if (mSurface != req.mSurface
          || mSurface == null
          || !mSurface.isValid()
          || !req.mSurface.isValid()) {
        return false;
      }
      if (mIsTunneling != req.mIsTunneling) {
        return false;
      }
      if (mHasCrypto != req.mHasCrypto) {
        return false;
      }
      if (mIsHdr != req.mIsHdr) {
        return false;
      }
      if (req.mMaxWidth > mMaxWidth || req.mMaxHeight > mMaxHeight) {
        return false;
      }
      if (mConfiguredWidth != req.mConfiguredWidth || mConfiguredHeight != req.mConfiguredHeight) {
        return false;
      }
      return true;
    }

    @Override
    public String toString() {
      return mMime + "/" + mDecoderName + "/" + mConfiguredWidth + "x" + mConfiguredHeight;
    }
  }

  /** Metadata and reference for a cached MediaCodecBridge instance awaiting reuse. */
  public static class CachedCodec {
    final MediaCodecBridge mBridge;
    final CodecConfig mConfig;

    public CachedCodec(MediaCodecBridge bridge, CodecConfig config) {
      mBridge = bridge;
      mConfig = config;
    }
  }

  private static final Object sCacheLock = new Object();

  @GuardedBy("sCacheLock")
  private static CachedCodec sCachedCodec = null;

  // Set by stopCaching() and cleared by allowCaching(). While set, codecs that players let go of
  // are released instead of cached, so that a codec never outlives the video surface it renders to.
  @GuardedBy("sCacheLock")
  private static boolean sDoNotCache = false;

  /** Shows and hides the video surface. Replaceable for testing. */
  interface SurfaceVisibility {
    /** Applies the visibility change immediately. */
    void setVisible(boolean visible);

    /**
     * Applies the visibility change together with the next frame rendered to the surface.
     *
     * @return {@code false} if this is not supported, in which case nothing is changed.
     */
    boolean setVisibleWithNextFrame(boolean visible);
  }

  private static final SurfaceVisibility DEFAULT_SURFACE_VISIBILITY =
      new SurfaceVisibility() {
        @Override
        public void setVisible(boolean visible) {
          VideoSurfaceView.setVideoSurfaceVisible(visible);
        }

        @Override
        public boolean setVisibleWithNextFrame(boolean visible) {
          return VideoSurfaceView.setVideoSurfaceVisibleWithNextFrame(visible);
        }
      };

  private static volatile SurfaceVisibility sSurfaceVisibility = DEFAULT_SURFACE_VISIBILITY;

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
    CodecConfig reqConfig =
        new CodecConfig(
            mime,
            decoderName,
            surface,
            widthHint,
            heightHint,
            maxWidth,
            maxHeight,
            tunnelModeAudioSessionId != TunnelModeAudioSessionId.NONE,
            crypto != null,
            isHdr);

    MediaCodecBridge bridge = null;
    MediaCodecBridge bridgeToRelease = null;
    CodecConfig cachedConfig = null;

    synchronized (sCacheLock) {
      if (sCachedCodec != null) {
        cachedConfig = sCachedCodec.mConfig;
        if (cachedConfig.isCompatible(reqConfig)) {
          bridge = sCachedCodec.mBridge;
        } else {
          bridgeToRelease = sCachedCodec.mBridge;
        }
        sCachedCodec = null;
      }
    }

    if (bridgeToRelease != null) {
      Log.i(
          TAG,
          "Cached MediaCodec is incompatible (cached="
              + cachedConfig
              + ", requested="
              + reqConfig
              + "), evicting.");
      bridgeToRelease.doRelease();
      // Note: Do NOT set VideoSurfaceView visibility to true here. The surface
      // must remain hidden during transition to prevent stale previous frames
      // (e.g., Shorts -> VoD) from flashing on screen. The incoming decoder shows
      // the surface together with its first frame, see onFirstFrameReleasing().
    }

    if (bridge == null) {
      return null;
    }
    Log.i(TAG, "Reusing cached MediaCodec for " + decoderName);

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
  }

  /** Evaluates if a MediaCodecBridge instance is eligible to be cached for reuse upon teardown. */
  public static boolean isEligibleForReuse(MediaCodecBridge bridge) {
    if (bridge == null
        || !bridge.isReuseEnabled()
        || bridge.hasEncounteredError()
        || bridge.getConfig() == null) {
      return false;
    }
    return bridge.getConfig().isEligibleForReuse();
  }

  /**
   * Called when a MediaCodecBridge is being released. If eligible, caches the bridge and defers its
   * release.
   *
   * <p>While caching is stopped (see {@link #stopCaching()}), the bridge is not cached. Its player
   * has already let go of it, so it is safe to release it immediately.
   *
   * @return {@code true} if the bridge was cached for reuse, {@code false} if it should be released
   *     immediately.
   */
  public static boolean maybeCacheOnRelease(MediaCodecBridge bridge) {
    if (!isEligibleForReuse(bridge)) {
      return false;
    }
    if (isCachingStopped()) {
      Log.i(TAG, "MediaCodec caching is stopped, releasing: " + bridge.getCodecName());
      return false;
    }

    // MediaCodec.flush() is a blocking binder call; perform it outside the cache lock.
    if (bridge.flush() != MediaCodecStatus.OK) {
      Log.w(TAG, "Failed to flush MediaCodec on release, not caching.");
      return false;
    }
    bridge.detachNativeBridge();
    MediaCodecOutputTracker.get().unregister(bridge);
    String codecName = bridge.getCodecName();
    // Hide the surface before storing the bridge: once it is in the cache, it can be acquired or
    // discarded on another thread at any time.
    sSurfaceVisibility.setVisible(false);

    boolean cached = false;
    CachedCodec oldCodecToRelease = null;
    synchronized (sCacheLock) {
      // Checked under the lock, so stopCaching() either runs first and the codec is not cached, or
      // runs afterwards and releases it.
      if (!sDoNotCache) {
        oldCodecToRelease = sCachedCodec;
        sCachedCodec = new CachedCodec(bridge, bridge.getConfig());
        cached = true;
      }
    }

    if (oldCodecToRelease != null) {
      Log.i(TAG, "Discarding cached MediaCodec: " + oldCodecToRelease.mConfig.mDecoderName);
      oldCodecToRelease.mBridge.doRelease();
    }

    if (!cached) {
      Log.i(TAG, "MediaCodec caching was stopped while caching, releasing: " + codecName);
      sSurfaceVisibility.setVisible(true);
      return false;
    }
    Log.i(TAG, "MediaCodec cached for potential reuse across playbacks: " + codecName);
    return true;
  }

  /**
   * Releases the cached codec, if any, and stops caching until {@link #allowCaching()} is called.
   *
   * <p>Called when the Activity is stopped and before the video surface is destroyed. A codec that
   * is still used by a player is not released here; it is released, instead of cached, when the
   * player lets go of it.
   */
  public static void stopCaching() {
    synchronized (sCacheLock) {
      sDoNotCache = true;
    }
    discard();
  }

  /** Allows codecs to be cached again, e.g., once a new video surface is available. */
  public static void allowCaching() {
    synchronized (sCacheLock) {
      sDoNotCache = false;
    }
  }

  private static boolean isCachingStopped() {
    synchronized (sCacheLock) {
      return sDoNotCache;
    }
  }

  /** Discards any currently cached MediaCodec and releases its hardware resources. */
  public static void discard() {
    CachedCodec codecToRelease;
    synchronized (sCacheLock) {
      codecToRelease = sCachedCodec;
      sCachedCodec = null;
    }
    if (codecToRelease != null) {
      Log.i(TAG, "Discarding cached MediaCodec: " + codecToRelease.mConfig.mDecoderName);
      codecToRelease.mBridge.doRelease();
      sSurfaceVisibility.setVisible(true);
    }
  }

  /**
   * Notified right before the first frame of a playback is released for rendering.
   *
   * <p>Shows the video surface together with that frame, so that neither a stale frame of the
   * previous playback nor a hidden first frame is displayed. If the change cannot be attached to
   * the frame, the surface is shown immediately.
   */
  public static void onFirstFrameReleasing(MediaCodecBridge bridge) {
    if (bridge == null || !bridge.isAwaitingFirstFrameAfterReuse()) {
      return;
    }
    bridge.setAwaitingFirstFrameAfterReuse(false);
    if (!sSurfaceVisibility.setVisibleWithNextFrame(true)) {
      sSurfaceVisibility.setVisible(true);
    }
  }

  /** Notified when an error occurs on the MediaCodec. */
  public static void onError(MediaCodecBridge bridge) {
    if (bridge == null || !bridge.isAwaitingFirstFrameAfterReuse()) {
      return;
    }
    bridge.setAwaitingFirstFrameAfterReuse(false);
    sSurfaceVisibility.setVisible(true);
  }

  /** Notified when a bridge is released without presenting frames. */
  public static void onBridgeReleased(MediaCodecBridge bridge) {
    if (bridge == null || !bridge.isAwaitingFirstFrameAfterReuse()) {
      return;
    }
    bridge.setAwaitingFirstFrameAfterReuse(false);
    sSurfaceVisibility.setVisible(true);
  }

  @VisibleForTesting
  static void setSurfaceVisibilityForTesting(SurfaceVisibility surfaceVisibility) {
    sSurfaceVisibility = surfaceVisibility != null ? surfaceVisibility : DEFAULT_SURFACE_VISIBILITY;
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
      sDoNotCache = false;
    }
    sSurfaceVisibility = DEFAULT_SURFACE_VISIBILITY;
  }
}
