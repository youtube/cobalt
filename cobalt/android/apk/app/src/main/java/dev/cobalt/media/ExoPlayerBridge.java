// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Surface;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.PlaybackParameters;
import androidx.media3.common.Player;
import androidx.media3.common.Tracks;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.DefaultLoadControl;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.exoplayer.analytics.AnalyticsListener;
import androidx.media3.exoplayer.source.MediaSource;
import androidx.media3.exoplayer.source.MergingMediaSource;
import androidx.media3.exoplayer.trackselection.DefaultTrackSelector;
import dev.cobalt.util.Log;
import java.nio.ByteBuffer;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Facilitates communication between the native ExoPlayerBridge and the Java ExoPlayer */
@JNINamespace("starboard")
@UnstableApi
public class ExoPlayerBridge {
    private ExoPlayer mPlayer;
    private ExoPlayerMediaSource mAudioMediaSource;
    private ExoPlayerMediaSource mVideoMediaSource;
    private final long mNativeExoPlayerBridge;
    private final Handler mExoplayerHandler;
    // The following variables are accessed on both the Handler and native threads
    private volatile long mLastPlaybackPosUsec = 0;
    private volatile long mPlaybackPosLastUpdatedMsec = 0;
    private volatile float mPlaybackRate = 0.0f;
    private volatile boolean mIsProgressing = false;
    private volatile boolean mIsReleasing = false;

    private long mSeekTimeUsec = 0;
    private final ExoPlayerListener mPlayerListener;
    private final DroppedFramesListener mDroppedFramesListener;

    // Duration after which an error is raised if the player doesn't release its resources.
    private static final long PLAYER_RELEASE_TIMEOUT_MS = 2000; // 2 seconds.
    private static final int MIN_BUFFER_DURATION_MS = 1000; // 1 second.
    private static final int MAX_BUFFER_DURATION_MS = 5000; // 5 seconds

    private class ExoPlayerListener implements Player.Listener {
        @Override
        public void onPlaybackStateChanged(@Player.State int playbackState) {
            if (mIsReleasing) {
                return;
            }

            switch (playbackState) {
                case Player.STATE_BUFFERING:
                case Player.STATE_IDLE:
                    return;
                case Player.STATE_READY:
                    ExoPlayerBridgeJni.get().onReady(mNativeExoPlayerBridge);
                    break;
                case Player.STATE_ENDED:
                    ExoPlayerBridgeJni.get().onEnded(mNativeExoPlayerBridge);
                    break;
            }
        }

        @Override
        public void onTracksChanged(Tracks tracks) {
            if (mIsReleasing) {
                return;
            }

            ExoPlayerBridgeJni.get().onInitialized(mNativeExoPlayerBridge);
        }

        @Override
        public synchronized void onIsPlayingChanged(boolean isPlaying) {
            if (mIsReleasing) {
                return;
            }

            mIsProgressing = isPlaying;
            ExoPlayerBridgeJni.get().onIsPlayingChanged(mNativeExoPlayerBridge, isPlaying);
        }

        @Override
        public void onPlayerError(@NonNull PlaybackException error) {
            String errorMessage = String.format("ExoPlayer playback error %s, code: %s, cause: %s",
                    error.getMessage(), error.getErrorCodeName(),
                    error.getCause() != null ? error.getCause().getMessage() : "none");
            reportError(errorMessage);
        }
    }

    private final class DroppedFramesListener implements AnalyticsListener {
        @Override
        public void onDroppedVideoFrames(
                @NonNull EventTime eventTime, int droppedFrames, long elapsedMs) {
            if (mIsReleasing) {
                return;
            }

            ExoPlayerBridgeJni.get().onDroppedVideoFrames(mNativeExoPlayerBridge, droppedFrames);
        }
    }

    /**
     * Initializes a new ExoPlayerBridge.
     * @param nativeExoPlayerBridge The pointer to the native ExoPlayerBridge.
     * @param context The application context.
     * @param renderersFactory The factory for creating renderers.
     * @param audioSource The audio source.
     * @param videoSource The video source.
     * @param surface The rendering surface.
     * @param enableTunnelMode Whether to enable tunnel mode.
     */
    public ExoPlayerBridge(long nativeExoPlayerBridge, Context context,
            DefaultRenderersFactory renderersFactory, @Nullable ExoPlayerMediaSource audioSource,
            @Nullable ExoPlayerMediaSource videoSource,
            @Nullable Surface surface, boolean enableTunnelMode) {
        this.mExoplayerHandler = new Handler(Looper.getMainLooper());
        mNativeExoPlayerBridge = nativeExoPlayerBridge;

        if (enableTunnelMode) {
            Log.i(TAG, "Tunnel mode is enabled for this playback.");
        }

        DefaultTrackSelector trackSelector = new DefaultTrackSelector(context);
        trackSelector.setParameters(new DefaultTrackSelector.ParametersBuilder(context)
                                            .setTunnelingEnabled(enableTunnelMode)
                                            .build());

        mAudioMediaSource = audioSource;
        mVideoMediaSource = videoSource;

        MediaSource playbackMediaSource;
        if (mAudioMediaSource != null && mVideoMediaSource != null) {
            playbackMediaSource =
                    new MergingMediaSource(true, mAudioMediaSource, mVideoMediaSource);
        } else {
            playbackMediaSource = mAudioMediaSource != null ? mAudioMediaSource : mVideoMediaSource;
        }

        mPlayer = new ExoPlayer.Builder(context)
                          .setRenderersFactory(renderersFactory)
                          .setLoadControl(
                                  new DefaultLoadControl.Builder()
                                          .setBufferDurationsMs(MIN_BUFFER_DURATION_MS,
                                                  MAX_BUFFER_DURATION_MS, MIN_BUFFER_DURATION_MS,
                                                  MIN_BUFFER_DURATION_MS)
                                          .build())
                          .setLooper(mExoplayerHandler.getLooper())
                          .setTrackSelector(trackSelector)
                          .setReleaseTimeoutMs(PLAYER_RELEASE_TIMEOUT_MS)
                          .build();

        mPlayerListener = new ExoPlayerListener();
        mDroppedFramesListener = new DroppedFramesListener();
        mExoplayerHandler.post(() -> {
            mPlayer.addListener(mPlayerListener);
            mPlayer.addAnalyticsListener(mDroppedFramesListener);
            mPlayer.setMediaSource(playbackMediaSource);
            mPlayer.setVideoSurface(surface);
            mPlayer.prepare();
            updatePlaybackPos();
            mPlayer.play();
        });
    }

    private synchronized void updatePlaybackPos() {
        if (mPlayer == null) {
            return;
        }

        // getCurrentPosition() returns milliseconds, convert to microseconds
        mLastPlaybackPosUsec = mPlayer.getCurrentPosition() * 1000;
        mPlaybackPosLastUpdatedMsec = SystemClock.elapsedRealtime();
        mExoplayerHandler.postDelayed(this::updatePlaybackPos, 150);
    }

    @CalledByNative
    public void release() {
        mIsReleasing = true;
        if (mPlayer == null) {
            Log.w(TAG, "Attempted to destroy ExoPlayer after it has already been released.");
            return;
        }

        mExoplayerHandler.post(() -> {
            mPlayer.stop();
            mPlayer.removeListener(mPlayerListener);
            mPlayer.removeAnalyticsListener(mDroppedFramesListener);
            mPlayer.release();
            mPlayer = null;
            mAudioMediaSource = null;
            mVideoMediaSource = null;
        });

        mExoplayerHandler.removeCallbacks(this::updatePlaybackPos);
    }

    /**
     * Seeks to a specified position.
     * @param seekToTimeUsec The position in microseconds to seek to.
     */
    @CalledByNative
    private void seek(long seekToTimeUsec) {
        if (mPlayer == null || mIsReleasing) {
            reportError("Cannot seek with NULL or releasing ExoPlayer.");
            return;
        }
        mExoplayerHandler.post(() -> {
          mPlayer.seekTo(seekToTimeUsec / 1000);
        });
        mSeekTimeUsec = seekToTimeUsec;
    }

    /**
     * Writes a sample to the appropriate media source.
     * @param samples The sample data.
     * @param size The size of the sample data.
     * @param timestamp The timestamp of the sample in microseconds.
     * @param isKeyFrame Whether the sample is a keyframe.
     * @param type The type of the media source (audio or video).
     */
    @CalledByNative
    public void writeSample(
            ByteBuffer samples, int size, long timestamp, boolean isKeyFrame, int type) {
        ExoPlayerMediaSource mediaSource =
                type == ExoPlayerRendererType.AUDIO ? mAudioMediaSource : mVideoMediaSource;
        if (mediaSource == null || mIsReleasing) {
            reportError(
                    String.format("Tried to write %s sample while ExoPlayer is in an invalid state",
                            type == ExoPlayerRendererType.AUDIO ? "audio" : "video"));
        }
        mediaSource.writeSample(samples, size, timestamp, isKeyFrame);
    }

    /**
     * Writes the end of the stream to the appropriate media source.
     * @param type The type of the media source (audio or video).
     */
    @CalledByNative
    public void writeEndOfStream(int type) {
        ExoPlayerMediaSource mediaSource =
                type == ExoPlayerRendererType.AUDIO ? mAudioMediaSource : mVideoMediaSource;
        if (mediaSource == null || mIsReleasing) {
            reportError(String.format(
                    "Tried to write %s EOS sample while ExoPlayer is in an invalid state",
                    type == ExoPlayerRendererType.AUDIO ? "audio" : "video"));
        }
        mediaSource.writeEndOfStream();
    }

    @CalledByNative
    private void pause() {
        if (mPlayer == null || mIsReleasing) {
            reportError("Cannot pause with NULL or releasing ExoPlayer");
            return;
        }
        mExoplayerHandler.removeCallbacks(this::updatePlaybackPos);
        mExoplayerHandler.post(() -> {
            mPlayer.pause();
            mLastPlaybackPosUsec = mPlayer.getCurrentPosition() * 1000;
        });
    }

    @CalledByNative
    private void play() {
        if (mPlayer == null || mIsReleasing) {
            reportError("Cannot play with NULL or releasing ExoPlayer");
            return;
        }
        mExoplayerHandler.post(() -> {
            if (!mPlayer.isPlaying() && mPlaybackRate > 0.0) {
                mPlayer.play();
                updatePlaybackPos();
            }
        });
    }

    @CalledByNative
    private void setPlaybackRate(float playbackRate) {
        if (mPlayer == null || mIsReleasing) {
            reportError("Cannot set playback rate with NULL or releasing ExoPlayer");
            return;
        }

        this.mPlaybackRate = playbackRate;

        if (playbackRate > 0.0f) {
            mExoplayerHandler.post(() -> {
                mPlayer.setPlaybackParameters(new PlaybackParameters(playbackRate, 1.0f));
            });
        }
    }

    @CalledByNative
    private void setVolume(float volume) {
        if (mPlayer == null || mIsReleasing) {
            reportError("Cannot set volume with NULL or releasing ExoPlayer");
            return;
        }

        mExoplayerHandler.post(() -> {
          mPlayer.setVolume(volume);
        });
    }

    @CalledByNative
    private void stop() {
        if (mPlayer == null || mIsReleasing) {
            Log.e(TAG,
                    "Cannot stop with NULL or releasing ExoPlayer. Assuming stopped successfully.");
            return;
        }
        mExoplayerHandler.removeCallbacks(this::updatePlaybackPos);
        mExoplayerHandler.post(() -> {
          mPlayer.stop();
        });
    }

    /**
     * Returns whether the appropriate media source can accept more data.
     * @param type The type of the media source (audio or video).
     * @return Whether the media source can accept more data.
     */
    @CalledByNative
    public boolean canAcceptMoreData(int type) {
        if (mIsReleasing) {
            return false;
        }
        ExoPlayerMediaSource mediaSource =
                type == ExoPlayerRendererType.AUDIO ? mAudioMediaSource : mVideoMediaSource;
        return mediaSource.canAcceptMoreData();
    }

    /**
     * Returns the current playback position in microseconds.
     * @return The current playback position in microseconds.
     */
    @CalledByNative
    private synchronized long getCurrentPositionUsec() {
        if (mIsProgressing) {
            return mLastPlaybackPosUsec
                    + (long) ((SystemClock.elapsedRealtime() - mPlaybackPosLastUpdatedMsec)
                            * mPlaybackRate * 1000);
        }

        return Math.max(mLastPlaybackPosUsec, mSeekTimeUsec);
    }

    private void reportError(String errorMessage) {
        Log.e(TAG, errorMessage);

        if (mIsReleasing) {
            return;
        }

        ExoPlayerBridgeJni.get().onError(mNativeExoPlayerBridge, errorMessage);
    }

    @NativeMethods
    interface Natives {
        void onInitialized(long nativeExoPlayerBridge);

        void onReady(long nativeExoPlayerBridge);

        void onError(long nativeExoPlayerBridge, String errorMessage);

        void onEnded(long nativeExoPlayerBridge);

        void onDroppedVideoFrames(long nativeExoPlayerBridge, int count);

        void onIsPlayingChanged(long nativeExoPlayerBridge, boolean isPlaying);
    }
}
