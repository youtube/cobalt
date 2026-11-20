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

import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.view.Surface;
import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;
import org.chromium.base.ContextUtils;

/**
 * Keeps one video MediaCodec alive after its player is destroyed, so that the next player can use
 * it instead of creating a new one. See the "Media.ReuseVideoCodec" setting.
 *
 * <p><strong>What is parked:</strong> only a codec that decodes clear, SDR video to the shared
 * video surface without tunnel mode, see {@link CodecConfig#ineligibleReason()}. It is parked
 * flushed and not restarted: in this state it holds all its buffers and delivers no callbacks, so
 * nothing of the previous playback can reach the next one. A codec that reported an error, or whose
 * flush failed or took longer than {@link #MAX_PARK_FLUSH_MS}, is released instead.
 *
 * <p><strong>When it is reused:</strong> only by a request whose parameters are all identical to
 * the ones the parked codec was created with, see {@link CodecConfig#mismatchReason(CodecConfig)}.
 * A fresh codec would then be configured exactly as the parked one is.
 *
 * <p><strong>When it is released:</strong> a parked codec holds a hardware decoder and its graphic
 * buffers, so it is released as soon as any of the following happens:
 *
 * <ul>
 *   <li>any other video codec is requested, so that there is never a parked codec next to a newly
 *       created one;
 *   <li>the codec reports an error, e.g. because the system reclaimed it;
 *   <li>the video surface is destroyed or the Activity is stopped, see {@link #stopCaching};
 *   <li>the system is running low on memory;
 *   <li>it has been parked for {@link #IDLE_TIMEOUT_MS};
 *   <li>another codec is parked.
 * </ul>
 *
 * <p>When the process dies, the system releases the codec as it does for any other codec.
 *
 * <p><strong>Video surface:</strong> a parked codec stays connected to the video surface, so its
 * last frame can't be cleared. MediaCodecVideoDecoder hides the surface on teardown instead, and
 * shows it again with the first frame of the next playback.
 *
 * <p><strong>Threading:</strong> all methods can be called from any thread. The slot is guarded by
 * a lock, which is never held while calling into MediaCodec. A codec is parked only after it has
 * been detached from its player, and is removed from the slot before it is handed to a player or
 * released, so it is never shared.
 */
public class MediaCodecCache {
  /** A parked codec that nobody asked for is released after this long. */
  @VisibleForTesting static final long IDLE_TIMEOUT_MS = 10_000;

  /**
   * A codec whose last flush took longer than this is not parked. A slow flush indicates a decoder
   * in trouble, which must not be handed to the next player.
   */
  @VisibleForTesting static final long MAX_PARK_FLUSH_MS = 500;

  private static final String LOG_PREFIX = "ReuseVideoCodec: ";

  /** The parameters a video codec was requested with. */
  static class CodecConfig {
    final String mMime;
    final String mDecoderName;
    final Surface mSurface;
    final int mWidthHint;
    final int mHeightHint;
    final int mFps;
    final int mMaxWidth;
    final int mMaxHeight;
    final int mMaxVideoInputSize;
    final boolean mEnableFrameRendererListener;
    final boolean mIsTunneling;
    final boolean mHasCrypto;
    final boolean mIsHdr;

    CodecConfig(
        String mime,
        String decoderName,
        Surface surface,
        int widthHint,
        int heightHint,
        int fps,
        int maxWidth,
        int maxHeight,
        int maxVideoInputSize,
        boolean enableFrameRendererListener,
        boolean isTunneling,
        boolean hasCrypto,
        boolean isHdr) {
      mMime = mime;
      mDecoderName = decoderName;
      mSurface = surface;
      mWidthHint = widthHint;
      mHeightHint = heightHint;
      mFps = fps;
      mMaxWidth = maxWidth;
      mMaxHeight = maxHeight;
      mMaxVideoInputSize = maxVideoInputSize;
      mEnableFrameRendererListener = enableFrameRendererListener;
      mIsTunneling = isTunneling;
      mHasCrypto = hasCrypto;
      mIsHdr = isHdr;
    }

    /** Returns why a codec with this config can't be parked, or null if it can. */
    @Nullable
    String ineligibleReason() {
      if (mIsTunneling) {
        return "tunnel";
      }
      if (mHasCrypto) {
        return "secure";
      }
      if (mIsHdr) {
        return "hdr";
      }
      if (mMime == null || mDecoderName == null || mSurface == null) {
        return "incomplete_config";
      }
      if (!mSurface.isValid()) {
        return "surface_invalid";
      }
      return null;
    }

    /**
     * Returns the first parameter in which {@code request} differs from this config, or null if
     * they are identical, i.e. a codec created for {@code request} would be configured exactly like
     * the one created with this config.
     */
    @Nullable
    String mismatchReason(CodecConfig request) {
      if (request.mIsTunneling != mIsTunneling) {
        return "tunnel";
      }
      if (request.mHasCrypto != mHasCrypto) {
        return "secure";
      }
      if (request.mIsHdr != mIsHdr) {
        return "hdr";
      }
      if (!mMime.equals(request.mMime)) {
        return "mime";
      }
      if (!mDecoderName.equals(request.mDecoderName)) {
        return "decoder";
      }
      if (mSurface != request.mSurface) {
        return "surface";
      }
      if (!mSurface.isValid()) {
        return "surface_invalid";
      }
      if (mWidthHint != request.mWidthHint || mHeightHint != request.mHeightHint) {
        return "size";
      }
      if (mMaxWidth != request.mMaxWidth || mMaxHeight != request.mMaxHeight) {
        return "max_size";
      }
      if (mFps != request.mFps) {
        return "fps";
      }
      if (mMaxVideoInputSize != request.mMaxVideoInputSize) {
        return "max_input_size";
      }
      if (mEnableFrameRendererListener != request.mEnableFrameRendererListener) {
        return "frame_rendered_listener";
      }
      return null;
    }

    @Override
    public String toString() {
      return mMime + "/" + mDecoderName + "/" + mWidthHint + "x" + mHeightHint;
    }
  }

  private static class ParkedCodec {
    final MediaCodecBridge mBridge;
    final CodecConfig mConfig;
    final long mParkedAtMs = SystemClock.elapsedRealtime();

    ParkedCodec(MediaCodecBridge bridge, CodecConfig config) {
      mBridge = bridge;
      mConfig = config;
    }
  }

  private static final Object sLock = new Object();

  @GuardedBy("sLock")
  private static ParkedCodec sParkedCodec;

  // Set by stopCaching() and cleared by allowCaching(). While set, codecs are released instead of
  // parked, so that a codec never outlives the video surface it renders to.
  @GuardedBy("sLock")
  private static boolean sCachingStopped;

  // Runs the idle timeout and the low memory eviction, so that releasing a codec, which can take a
  // while, doesn't block the main thread.
  @GuardedBy("sLock")
  private static Handler sEvictionHandler;

  @GuardedBy("sLock")
  private static boolean sMemoryCallbacksRegistered;

  // True while the last frame rendered by a codec that could have been parked may still be on the
  // video surface: with codec reuse, the decoder hides the surface instead of clearing it.
  @GuardedBy("sLock")
  private static boolean sVideoSurfaceNeedsClear;

  // The number of codecs that have been taken out of the slot and are being released or handed to
  // a player.
  @GuardedBy("sLock")
  private static int sCodecsInTransit;

  // When the last parked codec was released, or -1.
  @GuardedBy("sLock")
  private static long sLastEvictedAtMs = -1;

  private static final Runnable sIdleTimeoutRunnable = () -> discard("idle_timeout");

  private MediaCodecCache() {}

  /**
   * Returns the parked codec, restarted and attached to {@code nativeMediaCodecBridge}, if it was
   * created with exactly the parameters in {@code request}. Otherwise releases the parked codec, if
   * any, and returns null; the caller then creates a new codec.
   */
  @Nullable
  static MediaCodecBridge acquire(
      long nativeMediaCodecBridge, CodecConfig request, boolean skipVideoFramesOver60Fps) {
    ParkedCodec parked = takeParkedCodec();
    if (parked == null) {
      Log.i(TAG, LOG_PREFIX + "not_reused reason=nothing_parked request=" + request);
      return null;
    }

    String reason =
        parked.mBridge.hasEncounteredError() ? "error" : parked.mConfig.mismatchReason(request);
    if (reason != null) {
      release(parked, "mismatch_" + reason + " request=" + request);
      return null;
    }

    long startedAtMs = SystemClock.elapsedRealtime();
    MediaCodecBridge bridge = parked.mBridge;
    try {
      bridge.attachForReuse(nativeMediaCodecBridge, skipVideoFramesOver60Fps);
    } catch (Exception e) {
      Log.e(TAG, LOG_PREFIX + "failed to restart the parked codec", e);
      release(parked, "restart_failed");
      return null;
    }
    finishTransit();
    MediaCodecOutputTracker.get().register(bridge);
    long nowMs = SystemClock.elapsedRealtime();
    Log.i(
        TAG,
        LOG_PREFIX
            + "reused codec="
            + bridge.getCodecName()
            + " config="
            + parked.mConfig
            + " parked_ms="
            + (startedAtMs - parked.mParkedAtMs)
            + " restart_ms="
            + (nowMs - startedAtMs));
    return bridge;
  }

  /**
   * Called by a MediaCodecBridge that is being released. Parks it if it is eligible, in which case
   * the caller must not release it.
   *
   * @return true if the bridge was parked.
   */
  static boolean maybeParkOnRelease(MediaCodecBridge bridge) {
    if (!bridge.isReuseEnabled()) {
      return false;
    }
    if (bridge.hasRenderedFrames()) {
      // Whether the codec is parked or released, nothing clears its last frame from the surface:
      // the decoder hides the surface instead.
      synchronized (sLock) {
        sVideoSurfaceNeedsClear = true;
      }
    }
    CodecConfig config = bridge.getReuseConfig();
    String reason;
    if (config == null) {
      reason = "incomplete_config";
    } else if (bridge.hasEncounteredError()) {
      reason = "error";
    } else if (isCachingStopped()) {
      reason = "caching_stopped";
    } else if (bridge.hasOutputSizeChanged()) {
      reason = "output_size_changed";
    } else {
      reason = config.ineligibleReason();
    }
    if (reason != null) {
      logNotParked(bridge, reason);
      return false;
    }

    // Detaches the bridge from its player and makes sure that the codec is flushed. These are
    // blocking calls into the codec, so they are made without holding the lock.
    long flushMs = bridge.detachForParking();
    if (flushMs < 0) {
      logNotParked(bridge, "flush_failed");
      return false;
    }
    if (flushMs > MAX_PARK_FLUSH_MS) {
      logNotParked(bridge, "slow_flush flush_ms=" + flushMs);
      return false;
    }
    MediaCodecOutputTracker.get().unregister(bridge);

    ParkedCodec replaced = null;
    String reasonUnderLock = null;
    synchronized (sLock) {
      // Checked under the lock, so stopCaching() and onCodecError() either run first and the codec
      // is not parked, or run afterwards and release it.
      if (sCachingStopped) {
        reasonUnderLock = "caching_stopped";
      } else if (bridge.hasEncounteredError()) {
        reasonUnderLock = "error";
      } else {
        replaced = takeParkedCodecLocked();
        sParkedCodec = new ParkedCodec(bridge, config);
        Handler handler = getEvictionHandlerLocked();
        handler.removeCallbacks(sIdleTimeoutRunnable);
        handler.postDelayed(sIdleTimeoutRunnable, IDLE_TIMEOUT_MS);
      }
    }
    if (reasonUnderLock != null) {
      logNotParked(bridge, reasonUnderLock);
      return false;
    }
    if (replaced != null) {
      release(replaced, "replaced");
    }
    registerMemoryCallbacks();
    Log.i(
        TAG,
        LOG_PREFIX
            + "parked codec="
            + bridge.getCodecName()
            + " config="
            + config
            + " flush_ms="
            + flushMs
            + " rendered="
            + (bridge.hasRenderedFrames() ? 1 : 0));
    return true;
  }

  /** Called by a MediaCodecBridge when its codec reports an error. */
  static void onCodecError(MediaCodecBridge bridge) {
    ParkedCodec parked;
    synchronized (sLock) {
      if (sParkedCodec == null || sParkedCodec.mBridge != bridge) {
        return;
      }
      parked = takeParkedCodecLocked();
    }
    release(parked, "error");
  }

  /**
   * Releases the parked codec, if any, and stops parking codecs until {@link #allowCaching()} is
   * called. Called when the Activity is stopped and before the video surface is destroyed. A codec
   * that is still used by a player is released, instead of parked, when the player lets go of it.
   */
  public static void stopCaching(String reason) {
    synchronized (sLock) {
      sCachingStopped = true;
    }
    discard(reason);
  }

  /** Called when the video surface is destroyed, after stopCaching(). */
  public static void onVideoSurfaceDestroyed() {
    synchronized (sLock) {
      sVideoSurfaceNeedsClear = false;
    }
  }

  /**
   * Returns true, once, if the last frame of a released codec may still be on the video surface,
   * see sVideoSurfaceNeedsClear. Returns false while a codec is parked, as it is still connected to
   * the surface. To be called after acquire() returned null.
   */
  static boolean takeVideoSurfaceNeedsClear() {
    synchronized (sLock) {
      waitForCodecsInTransitLocked();
      boolean needsClear = sVideoSurfaceNeedsClear && sParkedCodec == null;
      if (needsClear) {
        sVideoSurfaceNeedsClear = false;
      }
      return needsClear;
    }
  }

  /** Logs that a new codec is about to be created, and how long ago a parked one was released. */
  static void logCreatingCodec(String decoderName) {
    long lastEvictedAtMs;
    synchronized (sLock) {
      lastEvictedAtMs = sLastEvictedAtMs;
    }
    Log.i(
        TAG,
        LOG_PREFIX
            + "creating codec="
            + decoderName
            + " since_evicted_ms="
            + (lastEvictedAtMs < 0 ? -1 : SystemClock.elapsedRealtime() - lastEvictedAtMs));
  }

  /** Allows codecs to be parked again, e.g. once a new video surface is available. */
  public static void allowCaching() {
    synchronized (sLock) {
      sCachingStopped = false;
    }
  }

  /** Releases the parked codec, if any. */
  public static void discard(String reason) {
    ParkedCodec parked = takeParkedCodec();
    if (parked != null) {
      release(parked, reason);
    }
  }

  private static boolean isCachingStopped() {
    synchronized (sLock) {
      return sCachingStopped;
    }
  }

  /**
   * Takes the parked codec out of the slot, after waiting for any codec another thread has taken
   * out to be released or handed to a player. A codec that is being released is still connected to
   * the video surface, so the caller must not go on to use the surface before that is over.
   */
  @Nullable
  private static ParkedCodec takeParkedCodec() {
    synchronized (sLock) {
      waitForCodecsInTransitLocked();
      return takeParkedCodecLocked();
    }
  }

  /** The caller must call finishTransit() once the returned codec is released or handed over. */
  @GuardedBy("sLock")
  @Nullable
  private static ParkedCodec takeParkedCodecLocked() {
    ParkedCodec parked = sParkedCodec;
    sParkedCodec = null;
    if (parked != null) {
      ++sCodecsInTransit;
      if (sEvictionHandler != null) {
        sEvictionHandler.removeCallbacks(sIdleTimeoutRunnable);
      }
    }
    return parked;
  }

  @GuardedBy("sLock")
  private static void waitForCodecsInTransitLocked() {
    while (sCodecsInTransit > 0) {
      try {
        sLock.wait();
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
        return;
      }
    }
  }

  private static void finishTransit() {
    synchronized (sLock) {
      --sCodecsInTransit;
      sLock.notifyAll();
    }
  }

  private static void release(ParkedCodec parked, String reason) {
    long startedAtMs = SystemClock.elapsedRealtime();
    long nowMs;
    try {
      parked.mBridge.doRelease();
    } finally {
      nowMs = SystemClock.elapsedRealtime();
      synchronized (sLock) {
        sLastEvictedAtMs = nowMs;
      }
      finishTransit();
    }
    Log.i(
        TAG,
        LOG_PREFIX
            + "evicted reason="
            + reason
            + " codec="
            + parked.mBridge.getCodecName()
            + " config="
            + parked.mConfig
            + " parked_ms="
            + (startedAtMs - parked.mParkedAtMs)
            + " release_ms="
            + (nowMs - startedAtMs)
            + " rendered="
            + (parked.mBridge.hasRenderedFrames() ? 1 : 0));
  }

  private static void logNotParked(MediaCodecBridge bridge, String reason) {
    Log.i(
        TAG,
        LOG_PREFIX
            + "not_parked reason="
            + reason
            + " codec="
            + bridge.getCodecName()
            + " config="
            + bridge.getReuseConfig());
  }

  @GuardedBy("sLock")
  private static Handler getEvictionHandlerLocked() {
    if (sEvictionHandler == null) {
      HandlerThread thread = new HandlerThread("MediaCodecCache");
      thread.start();
      sEvictionHandler = new Handler(thread.getLooper());
    }
    return sEvictionHandler;
  }

  /** Releases the parked codec when the system tells the app that memory is low. */
  private static void registerMemoryCallbacks() {
    Context context = ContextUtils.getApplicationContext();
    synchronized (sLock) {
      if (sMemoryCallbacksRegistered || context == null) {
        return;
      }
      sMemoryCallbacksRegistered = true;
    }
    context.registerComponentCallbacks(
        new ComponentCallbacks2() {
          @Override
          public void onTrimMemory(int level) {
            // Also covers TRIM_MEMORY_UI_HIDDEN and the levels sent to background processes, which
            // are higher.
            if (level >= ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW) {
              postLowMemoryEviction();
            }
          }

          @Override
          public void onLowMemory() {
            postLowMemoryEviction();
          }

          @Override
          public void onConfigurationChanged(Configuration newConfig) {}
        });
  }

  private static void postLowMemoryEviction() {
    final Handler handler;
    synchronized (sLock) {
      handler = getEvictionHandlerLocked();
    }
    handler.post(() -> discard("low_memory"));
  }

  @VisibleForTesting
  static boolean hasParkedCodecForTesting() {
    synchronized (sLock) {
      return sParkedCodec != null;
    }
  }

  /** Forgets the parked codec without releasing it, and makes {@code handler} run evictions. */
  @VisibleForTesting
  static void resetForTesting(Handler handler) {
    synchronized (sLock) {
      if (sEvictionHandler != null) {
        sEvictionHandler.removeCallbacks(sIdleTimeoutRunnable);
      }
      sParkedCodec = null;
      sCachingStopped = false;
      sVideoSurfaceNeedsClear = false;
      sLastEvictedAtMs = -1;
      sCodecsInTransit = 0;
      sMemoryCallbacksRegistered = false;
      sEvictionHandler = handler;
    }
  }
}
