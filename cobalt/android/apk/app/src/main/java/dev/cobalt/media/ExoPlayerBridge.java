// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
import android.view.Surface;
import androidx.annotation.NonNull;
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
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Facilitates communication between the native ExoPlayerBridge and the Java ExoPlayer */
@JNINamespace("starboard")
@UnstableApi
public class ExoPlayerBridge {
    private ExoPlayer player;
    private ExoPlayerMediaSource audioMediaSource;
    private ExoPlayerMediaSource videoMediaSource;
    private final long mNativeExoPlayerBridge;
    private Handler exoplayerHandler;
    private long lastPlaybackPosUsec = 0;
    private volatile float playbackRate = 0.0f;
    private final ExoPlayerListener playerListener;
    private final DroppedFramesListener droppedFramesListener;

    private static final long PLAYER_RELEASE_TIMEOUT_MS = 300;
    private static final int MAX_BUFFER_DURATION_MS = 30000; // 30 seconds
    private static final int MIN_BUFFER_DURATION_MS = 450;

    private class ExoPlayerListener implements Player.Listener {
        @Override
        public void onPlaybackStateChanged(@Player.State int playbackState) {
            switch (playbackState) {
                case Player.STATE_BUFFERING:
                case Player.STATE_IDLE:
                    return;
                case Player.STATE_READY:
                    ExoPlayerBridgeJni.get().onReady(mNativeExoPlayerBridge);
                    play();
                    break;
                case Player.STATE_ENDED:
                    ExoPlayerBridgeJni.get().onEnded(mNativeExoPlayerBridge);
                    break;
            }
        }

        @Override
        public void onTracksChanged(Tracks tracks) {
            ExoPlayerBridgeJni.get().onInitialized(mNativeExoPlayerBridge);
        }

        @Override
        public synchronized void onIsPlayingChanged(boolean isPlaying) {
            ExoPlayerBridgeJni.get().onIsPlayingChanged(mNativeExoPlayerBridge, isPlaying);
        }

        @Override
        public void onPlayerError(@NonNull PlaybackException error) {
            String errorMessage = String.format("ExoPlayer playback error %s, code: %s, cause: %s",
                    error.getMessage(), error.getErrorCodeName(),
                    error.getCause() != null ? error.getCause().getMessage() : "none");
            Log.e(TAG, errorMessage);
            ExoPlayerBridgeJni.get().onError(mNativeExoPlayerBridge, errorMessage);
        }
    }

    private final class DroppedFramesListener implements AnalyticsListener {
        @Override
        public void onDroppedVideoFrames(
                @NonNull EventTime eventTime, int droppedFrames, long elapsedMs) {
            ExoPlayerBridgeJni.get().onDroppedVideoFrames(mNativeExoPlayerBridge, droppedFrames);
        }
    }

    public static ExoPlayerBridge createExoPlayerBridge(long nativeExoPlayerBridge, Context context,
            ExoPlayerMediaSource audioSource, ExoPlayerMediaSource videoSource, Surface surface,
            boolean enableTunnelMode) {
        if (videoSource != null && (surface == null || !surface.isValid())) {
            Log.e(TAG, "Cannot initialize ExoPlayer with NULL surface.");
            return null;
        }

        return new ExoPlayerBridge(nativeExoPlayerBridge, context, audioSource, videoSource,
                surface, enableTunnelMode);
    }

    private ExoPlayerBridge(long nativeExoPlayerBridge, Context context,
            ExoPlayerMediaSource audioSource, ExoPlayerMediaSource videoSource, Surface surface,
            boolean enableTunnelMode) {
        this.exoplayerHandler = new Handler(Looper.getMainLooper());
        mNativeExoPlayerBridge = nativeExoPlayerBridge;

        if (enableTunnelMode) {
            Log.i(TAG, "Tunnel mode is enabled for this playback.");
        }

        DefaultTrackSelector trackSelector = new DefaultTrackSelector(context);
        trackSelector.setParameters(new DefaultTrackSelector.ParametersBuilder(context)
                                            .setTunnelingEnabled(enableTunnelMode)
                                            .build());

        DefaultRenderersFactory renderersFactory =
                new DefaultRenderersFactory(context).setMediaCodecSelector(
                        new ExoPlayerCodecUtil.FilteringMediaCodecSelector());

        if (audioSource != null) {
            audioMediaSource = audioSource;
        }

        if (videoSource != null) {
            videoMediaSource = videoSource;
        }

        MediaSource playbackMediaSource;
        if (audioMediaSource != null && videoMediaSource != null) {
            playbackMediaSource = new MergingMediaSource(true, audioMediaSource, videoMediaSource);
        } else {
            playbackMediaSource = audioMediaSource != null ? audioMediaSource : videoMediaSource;
        }

        player = new ExoPlayer.Builder(context)
                         .setRenderersFactory(renderersFactory)
                         .setLoadControl(
                                 new DefaultLoadControl.Builder()
                                         .setBufferDurationsMs(MIN_BUFFER_DURATION_MS,
                                                 MAX_BUFFER_DURATION_MS, MIN_BUFFER_DURATION_MS,
                                                 MIN_BUFFER_DURATION_MS)
                                         .build())
                         .setLooper(exoplayerHandler.getLooper())
                         .setTrackSelector(trackSelector)
                         .setReleaseTimeoutMs(PLAYER_RELEASE_TIMEOUT_MS)
                         .build();

        playerListener = new ExoPlayerListener();
        droppedFramesListener = new DroppedFramesListener();
        exoplayerHandler.post(() -> {
            player.addListener(playerListener);
            player.addAnalyticsListener(droppedFramesListener);
            player.setMediaSource(playbackMediaSource);
            player.setVideoSurface(surface);
            player.prepare();
            updatePlaybackPos();
        });
    }

    private synchronized void updatePlaybackPos() {
        if (!isAbleToProcessCommands()) {
            return;
        }

        // getCurrentPosition() returns milliseconds, convert to microseconds
        lastPlaybackPosUsec = player.getCurrentPosition() * 1000;
        exoplayerHandler.postDelayed(this::updatePlaybackPos, 150);
    }

    public void destroy() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to destroy player with NULL ExoPlayer.");
            return;
        }

        exoplayerHandler.post(() -> {
            player.stop();
            player.removeListener(playerListener);
            player.removeAnalyticsListener(droppedFramesListener);
            player.release();
            player = null;
            exoplayerHandler = null;
            audioMediaSource = null;
        });

        exoplayerHandler.removeCallbacks(this::updatePlaybackPos);
        videoMediaSource = null;
    }

    @CalledByNative
    private void seek(long seekToTimeUs) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set seek with NULL ExoPlayer.");
            return;
        }
        exoplayerHandler.post(() -> {
            player.seekTo(seekToTimeUs / 1000);
        });
    }

    @CalledByNative
    public void writeSample(
            byte[] samples, int size, long timestamp, boolean isKeyFrame, int type) {
        ExoPlayerMediaSource mediaSource =
                type == ExoPlayerRendererType.AUDIO ? audioMediaSource : videoMediaSource;
        mediaSource.writeSample(samples, size, timestamp, isKeyFrame);
    }

    @CalledByNative
    public void writeEndOfStream(int type) {
        ExoPlayerMediaSource mediaSource =
                type == ExoPlayerRendererType.AUDIO ? audioMediaSource : videoMediaSource;
        mediaSource.writeEndOfStream();
    }

    @CalledByNative
    private boolean pause() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to pause with NULL ExoPlayer.");
            return false;
        }
        exoplayerHandler.post(() -> {
            player.pause();
        });
        exoplayerHandler.removeCallbacks(this::updatePlaybackPos);
        return true;
    }

    @CalledByNative
    private boolean play() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to play with NULL ExoPlayer.");
            return false;
        }
        exoplayerHandler.post(() -> {
            if (!player.isPlaying() && playbackRate > 0.0) {
                player.play();
                updatePlaybackPos();
            }
        });
        return true;
    }

    @CalledByNative
    private boolean setPlaybackRate(float playbackRate) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set playback rate with NULL ExoPlayer.");
            return false;
        }

        this.playbackRate = playbackRate;

        if (playbackRate == 0.0f) {
            pause();
        } else if (playbackRate > 0.0f) {
            exoplayerHandler.post(() -> {
                player.setPlaybackParameters(new PlaybackParameters(playbackRate, 1.0f));
            });
            play();
        }
        return true;
    }

    @CalledByNative
    private boolean setVolume(float volume) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set volume with NULL ExoPlayer.");
            return false;
        }

        exoplayerHandler.post(() -> {
            player.setVolume(volume);
        });
        return true;
    }

    @CalledByNative
    private boolean stop() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to stop with NULL ExoPlayer. Assuming stopped successfully.");
            return true;
        }
        exoplayerHandler.removeCallbacks(this::updatePlaybackPos);
        exoplayerHandler.post(() -> {
            player.stop();
        });

        return true;
    }

    @CalledByNative
    public boolean canAcceptMoreData(int type) {
        ExoPlayerMediaSource mediaSource =
                type == ExoPlayerRendererType.AUDIO ? audioMediaSource : videoMediaSource;
        return mediaSource.canAcceptMoreData();
    }

    @CalledByNative
    private synchronized long getCurrentPositionUsec() {
        return lastPlaybackPosUsec;
    }

    private boolean isAbleToProcessCommands() {
        return player != null && exoplayerHandler != null;
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
