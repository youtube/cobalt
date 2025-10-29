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
import androidx.media3.common.ColorInfo;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.PlaybackParameters;
import androidx.media3.common.Player;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.DefaultLoadControl;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.exoplayer.analytics.AnalyticsListener;
import androidx.media3.exoplayer.source.MediaSource;
import androidx.media3.exoplayer.source.MergingMediaSource;
import androidx.media3.exoplayer.trackselection.DefaultTrackSelector;
import dev.cobalt.util.Log;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** Facilitates communication between the native ExoPlayerBridge and the Java ExoPlayer */
@JNINamespace("starboard")
@UnstableApi
public class ExoPlayerBridge {
    private ExoPlayer player;
    private ExoPlayerMediaSource audioMediaSource;
    private ExoPlayerMediaSource videoMediaSource;
    private long mNativeExoPlayerBridge;
    private Handler exoplayerHandler;
    private long lastPlaybackPos = 0;
    private volatile boolean stopped = false;
    private boolean prerolled = false;
    private volatile boolean destroying = false;
    private int droppedFramesOnPlayerThread = 0;
    private boolean notifiedEOS = false;
    private boolean notifiedStreamsReady = false;

    private ExoPlayerListener playerListener;

    private DroppedFramesListener droppedFramesListener;

    private class ExoPlayerListener implements Player.Listener {
        @Override
        public void onPlaybackStateChanged(@Player.State int playbackState) {
            if (destroying) {
                return;
            }
            switch (playbackState) {
                case Player.STATE_BUFFERING:
                case Player.STATE_IDLE:
                    return;
                case Player.STATE_READY:
                    Log.i(TAG, "ExoPlayer state changed to READY");
                    if (!prerolled) {
                        ExoPlayerBridgeJni.get().onReady(mNativeExoPlayerBridge);
                        prerolled = true;
                    }
                    break;
                case Player.STATE_ENDED:
                    Log.i(TAG, "ExoPlayer state changed to ENDED");
                    ExoPlayerBridgeJni.get().onPlaybackEnded(mNativeExoPlayerBridge);
                    notifiedEOS = true;
                    break;
            }
        }

        @Override
        public void onIsPlayingChanged(boolean isPlaying) {
            if (destroying) {
                return;
            }
            Log.i(TAG, isPlaying ? "Exoplayer is playing." : "Exoplayer is not playing.");
            if (!stopped) {
                ExoPlayerBridgeJni.get().setPlayingStatus(mNativeExoPlayerBridge, isPlaying);
            }
        }

        @Override
        public void onPlayerError(@NonNull PlaybackException error) {
            if (destroying) {
                return;
            }
            String errorMessage = String.format("ExoPlayer playback error %s, code: %s, cause: %s",
                    error.getMessage(), error.getErrorCodeName(),
                    error.getCause() != null ? error.getCause().getMessage() : "N/A");
            Log.e(TAG, errorMessage);
            Log.i(TAG, "Calling onerror in onplayererror");
            ExoPlayerBridgeJni.get().onError(mNativeExoPlayerBridge, errorMessage);
        }
    }

    private final class DroppedFramesListener implements AnalyticsListener {
        @Override
        public void onDroppedVideoFrames(
                @NonNull EventTime eventTime, int droppedFrames, long elapsedMs) {
            droppedFramesOnPlayerThread += droppedFrames;
        }
    }

    public ExoPlayerBridge(long nativeExoPlayerBridge, Context context, boolean preferTunnelMode) {
        this.exoplayerHandler = new Handler(Looper.getMainLooper());

        mNativeExoPlayerBridge = nativeExoPlayerBridge;

        if (preferTunnelMode) {
            Log.i(TAG, "Tunnel mode is preferred for this playback.");
        }

        DefaultTrackSelector trackSelector = new DefaultTrackSelector(context);
        trackSelector.setParameters(new DefaultTrackSelector.ParametersBuilder(context)
                                            .setTunnelingEnabled(preferTunnelMode)
                                            .build());

        DefaultRenderersFactory renderersFactory =
                new DefaultRenderersFactory(context).setMediaCodecSelector(
                        new ExoPlayerMediaCodecSelector());

        player = new ExoPlayer.Builder(context)
                         .setRenderersFactory(renderersFactory)
                         .setLoadControl(createLoadControl())
                         .setLooper(exoplayerHandler.getLooper())
                         .setTrackSelector(trackSelector)
                         .setReleaseTimeoutMs(300)
                         .build();

        playerListener = new ExoPlayerListener();
        droppedFramesListener = new DroppedFramesListener();
        exoplayerHandler.post(() -> {
            player.addListener(playerListener);
            player.addAnalyticsListener(droppedFramesListener);
        });
    }

    @CalledByNative
    private ExoPlayerMediaSource createAudioMediaSource(int codec, String mime,
            byte[] audioConfigurationData, int sampleRate, int channelCount, int bitsPerSample) {
        if (audioMediaSource != null) {
            Log.e(TAG,
                    "Attempted to create an ExoPlayer audio MediaSource while one already exists.");
            return null;
        }

        audioMediaSource = new ExoPlayerMediaSource(this,
                ExoPlayerFormatCreator.createAudioFormat(codec, mime, audioConfigurationData,
                        sampleRate, channelCount, bitsPerSample),
                ExoPlayerRendererType.AUDIO);

        return audioMediaSource;
    }

    @CalledByNative
    private ExoPlayerMediaSource createVideoMediaSource(
            String mime, Surface surface, int width, int height, int fps, int bitrate, ColorInfo colorInfo) {
        if (videoMediaSource != null) {
            Log.e(TAG,
                    "Attempted to create an ExoPlayer video MediaSource while one already exists.");
            return null;
        }
        if (player == null) {
            Log.e(TAG, "Cannot set the video surface for an invalid player.");
            return null;
        }
        if (surface != null && !surface.isValid()) {
            Log.w(TAG, "Provided surface is invalid.");
        }

        videoMediaSource = new ExoPlayerMediaSource(this,
                ExoPlayerFormatCreator.createVideoFormat(mime, width, height, (float) fps, bitrate, colorInfo),
                ExoPlayerRendererType.VIDEO);

        exoplayerHandler.post(() -> {
            player.setVideoSurface(surface);
        });
        return videoMediaSource;
    }

    @CalledByNative
    private boolean preparePlayer() {
        if (audioMediaSource == null && videoMediaSource == null) {
            Log.e(TAG, "Cannot prepare ExoPlayer with null media sources");
            return false;
        }

        MediaSource playbackMediaSource;
        if (audioMediaSource != null && videoMediaSource != null) {
            playbackMediaSource = new MergingMediaSource(true, audioMediaSource, videoMediaSource);
        } else if (audioMediaSource != null) {
            playbackMediaSource = audioMediaSource;
        } else {
            playbackMediaSource = videoMediaSource;
        }
        exoplayerHandler.post(() -> {
            player.setMediaSource(playbackMediaSource);
            player.prepare();
        });
        return true;
    }

    private synchronized void updatePlaybackPos() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to update timestamps with NULL ExoPlayer.");
            return;
        }
        // getCurrentPosition() returns milliseconds, convert to microseconds
        lastPlaybackPos = player.getCurrentPosition() * 1000;
        if (!stopped) {
            exoplayerHandler.postDelayed(this::updatePlaybackPos, 75);
        } else if (notifiedEOS) {
            exoplayerHandler.removeCallbacks(this::updatePlaybackPos);
        }
    }

    public void destroy() throws InterruptedException {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to destroy player with NULL ExoPlayer.");
            return;
        }
        destroying = true;
        exoplayerHandler.removeCallbacks(this::updatePlaybackPos);

        final CountDownLatch releaseLatch = new CountDownLatch(1);
        exoplayerHandler.post(() -> {
            try {
                stopped = true;
                Log.i(TAG, "Releasing ExoPlayer.");
                player.stop();
                Log.i(TAG, String.format("Player playback state is %d", player.getPlaybackState()));
                player.removeListener(playerListener);
                player.removeAnalyticsListener(droppedFramesListener);
                player.release();
                player = null;
            } finally {
                Log.i(TAG, "Signaling latch");
                releaseLatch.countDown();
            }
        });

        releaseLatch.await(5, TimeUnit.SECONDS);

        exoplayerHandler = null;
        audioMediaSource = null;
        videoMediaSource = null;
        prerolled = false;
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
        droppedFramesOnPlayerThread = 0;
        prerolled = false;
        notifiedStreamsReady = false;
        notifiedEOS = false;
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
    private boolean setPlaybackRate(float playbackRate) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set playback rate with NULL ExoPlayer.");
            return false;
        }
        if (playbackRate == 0.0f) {
            // Skip, setting the playback rate to 0 is unsupported.
        } else if (playbackRate > 0.0f) {
            exoplayerHandler.post(() -> {
                player.setPlaybackParameters(new PlaybackParameters(playbackRate, 1.0f));
            });
        } else {
            return false;
        }
        return true;
    }

    @CalledByNative
    private boolean play() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to play with NULL ExoPlayer.");
            return false;
        }
        exoplayerHandler.post(() -> {
            if (!player.isPlaying()) {
                player.play();
            }
        });
        exoplayerHandler.post(this::updatePlaybackPos);
        return true;
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
        return true;
    }

    @CalledByNative
    private boolean stop() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to stop with NULL ExoPlayer. Assuming stopped successfully.");
            return true;
        }
        if (!stopped) {
            exoplayerHandler.removeCallbacks(this::updatePlaybackPos);
            exoplayerHandler.post(() -> {
                player.stop();
                stopped = true;
                Log.i(TAG, "ExoPlayer is stopped.");
            });
        }
        return true;
    }

    @CalledByNative
    private synchronized long getCurrentPositionUs() {
        return lastPlaybackPos;
    }

    @CalledByNative
    private synchronized int getDroppedFrames() {
        return droppedFramesOnPlayerThread;
    }

    public void onStreamCreated() {
        if (!isAbleToProcessCommands()) {
            Log.i(TAG, "Player is being destroyed or not ready, cannot signal stream creation.");
            return;
        }
        if (notifiedStreamsReady) {
            Log.e(TAG,
                    "Called onStreamCreated() after the native ExoPlayerBridge had been notified.");
            ExoPlayerBridgeJni.get().onError(mNativeExoPlayerBridge,
                    "Called onStreamCreated() after the native ExoPlayerBridge had been notified");
            return;
        }

        // Determine if all required streams are initialized
        boolean audioIsInitialized = audioMediaSource == null || audioMediaSource.isInitialized();
        boolean videoIsInitialized = videoMediaSource == null || videoMediaSource.isInitialized();

        if (audioIsInitialized && videoIsInitialized) {
            ExoPlayerBridgeJni.get().onInitialized(mNativeExoPlayerBridge);
            notifiedStreamsReady = true;
        }
    }

    private DefaultLoadControl createLoadControl() {
        return new DefaultLoadControl.Builder().setBufferDurationsMs(400, 10000, 400, 400).build();
    }

    private boolean isAbleToProcessCommands() {
        return !destroying && player != null && exoplayerHandler != null;
    }

    @NativeMethods
    interface Natives {
        void onPlaybackEnded(long nativeExoPlayerBridge);

        void onInitialized(long nativeExoPlayerBridge);

        void onReady(long nativeExoPlayerBridge);

        void onError(long nativeExoPlayerBridge, String errorMessage);

        void setPlayingStatus(long nativeExoPlayerBridge, boolean isPlaying);
    }
}
