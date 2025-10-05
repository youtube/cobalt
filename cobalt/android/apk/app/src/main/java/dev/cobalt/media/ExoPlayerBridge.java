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
import android.os.HandlerThread;
import android.os.Process;
import android.view.Surface;

import androidx.annotation.NonNull;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.PlaybackParameters;
import androidx.media3.common.Player;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.DefaultLoadControl;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.exoplayer.analytics.AnalyticsListener;
import androidx.media3.exoplayer.mediacodec.MediaCodecSelector;
import androidx.media3.exoplayer.source.MediaSource;
import androidx.media3.exoplayer.source.MergingMediaSource;
import androidx.media3.exoplayer.trackselection.DefaultTrackSelector;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import dev.cobalt.util.Log;

/** Facilitates communication between the native ExoPlayerBridge with the Java ExoPlayer. */
@JNINamespace("starboard::android::shared::exoplayer")
@UnstableApi
public class ExoPlayerBridge {
    private ExoPlayer player;
    private CobaltMediaSource audioMediaSource;
    private CobaltMediaSource videoMediaSource;
    private long mNativeExoPlayerBridge;
    private HandlerThread exoplayerThread;
    private Handler exoplayerHandler;
    private long lastPlaybackPos = 0;
    private volatile boolean stopped = false;
    private boolean prerolled = false;
    private volatile boolean destroying = false;
    private int droppedFramesOnPlayerThread = 0;
    private boolean notifiedEOS = false;
    private boolean notifiedStreamsReady = false;

    private class ExoPlayerListener implements Player.Listener {
        @Override
        public void onPlaybackStateChanged(@Player.State int playbackState) {
            Log.i(TAG, String.format("Media time is %d", lastPlaybackPos));
            switch (playbackState) {
                case Player.STATE_BUFFERING:
                    Log.i(TAG, "ExoPlayer state changed to BUFFERING");
                    break;
                case Player.STATE_IDLE:
                    Log.i(TAG, "ExoPlayer state changed to IDLE");
                    // It's unnecessary to update the state to idle if playback has already stopped.
                    if (!stopped) {
                        ExoPlayerBridgeJni.get().onPlaybackStateChanged(
                                mNativeExoPlayerBridge, playbackState);
                    }
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
                    break;
            }
            ExoPlayerBridgeJni.get().onPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
            if (playbackState == Player.STATE_ENDED) {
                notifiedEOS = true;
            }
        }

        @Override
        public void onIsPlayingChanged(boolean isPlaying) {
            Log.i(TAG, isPlaying ? "Exoplayer is playing." : "Exoplayer is not playing.");
            if (!stopped) {
                ExoPlayerBridgeJni.get().setPlayingStatus(mNativeExoPlayerBridge, isPlaying);
            }
        }

        @Override
        public void onPlayerError(@NonNull PlaybackException error) {
            String errorMessage = String.format("ExoPlayer playback error %s, code: %s, cause: %s",
                    error.getMessage(), error.getErrorCodeName(),
                    error.getCause() != null ? error.getCause().getMessage() : "N/A");
            Log.e(TAG, errorMessage);
            ExoPlayerBridgeJni.get().onError(mNativeExoPlayerBridge, errorMessage);
        }

        @Override
        public void onPlayWhenReadyChanged(
                boolean playWhenReady, @Player.PlayWhenReadyChangeReason int reason) {
            Log.i(TAG,
                    String.format("onPlayWhenReadyChanged() with playWhenReady %b and reason %d",
                            playWhenReady, reason));
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
        this.exoplayerThread = new HandlerThread("ExoPlayerThread", Process.THREAD_PRIORITY_AUDIO);
        exoplayerThread.start();
        this.exoplayerHandler = new Handler(exoplayerThread.getLooper());

        mNativeExoPlayerBridge = nativeExoPlayerBridge;

        if (preferTunnelMode) {
            Log.i(TAG, "Tunnel mode is preferred for this playback.");
        }

        DefaultTrackSelector trackSelector = new DefaultTrackSelector(context);
        trackSelector.setParameters(new DefaultTrackSelector.ParametersBuilder(context)
                                            .setTunnelingEnabled(preferTunnelMode)
                                            .build());

        MediaCodecSelector mediaCodecSelector = new CobaltMediaCodecSelector();
        DefaultRenderersFactory renderersFactory =
                new DefaultRenderersFactory(context).setMediaCodecSelector(mediaCodecSelector);

        player = new ExoPlayer.Builder(context)
                         .setRenderersFactory(renderersFactory)
                         .setLoadControl(createLoadControl())
                         .setLooper(exoplayerThread.getLooper())
                         .setTrackSelector(trackSelector)
                         .setReleaseTimeoutMs(1000)
                         .build();

        exoplayerHandler.post(() -> {
            player.addAnalyticsListener(new DroppedFramesListener());
            player.addListener(new ExoPlayerListener());
        });
    }

    @CalledByNative
    private CobaltMediaSource createAudioMediaSource(String mime, byte[] audioConfigurationData,
            int sampleRate, int channelCount, int bitsPerSample) {
        if (audioMediaSource != null) {
            Log.e(TAG,
                    "Attempted to create an ExoPlayer audio MediaSource while one already exists.");
            return null;
        }

        audioMediaSource = new CobaltMediaSource(this,
                ExoPlayerFormatCreator.createAudioFormat(
                        mime, audioConfigurationData, sampleRate, channelCount, bitsPerSample),
                ExoPlayerRendererType.AUDIO);

        return audioMediaSource;
    }

    @CalledByNative
    private CobaltMediaSource createVideoMediaSource(
            String mime, Surface surface, int width, int height, int fps, int bitrate) {
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

        videoMediaSource = new CobaltMediaSource(this,
                ExoPlayerFormatCreator.createVideoFormat(mime, width, height, (float) fps, bitrate),
                ExoPlayerRendererType.VIDEO);

        exoplayerHandler.post(() -> { player.setVideoSurface(surface); });
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
            Log.i(TAG, "ExoPlayer created and preparing.");
        });
        return true;
    }

    private synchronized void updatePlaybackPos() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to update timestamps with NULL ExoPlayer.");
            return;
        }
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
        Log.i(TAG, "Removing callbacks and preparing to destroy player.");
        exoplayerHandler.removeCallbacksAndMessages(null);
        destroying = true;

        exoplayerHandler.post(() -> {
            stopped = true;
            Log.i(TAG, "Releasing ExoPlayer.");
            player.release();
            player = null;
            // if (player != null) {
            //     player.release();
            //     player = null;
            // } else {
            //     Log.i(TAG, "Player is already null, no release needed.");
            // }
            // exoplayerHandler.getLooper().quitSafely();
            Log.i(TAG, "ExoPlayer released and thread quit safely.");
        });

        // TODO: Find a better way to ensure the thread is joined.
        // Wait for thread to finish
        Log.i(TAG, "Telling exoPlayerThread to quit");
        exoplayerThread.quitSafely();
        exoplayerThread.join();
        if (exoplayerThread.isAlive()) {
            Log.w(TAG, "ExoPlayer thread did not join.");
        }
        exoplayerHandler = null;
        exoplayerThread = null;
        Log.i(TAG, "ExoPlayer thread references cleared.");
        audioMediaSource = null;
        videoMediaSource = null;
        prerolled = false;
    }

    @CalledByNative
    private boolean seek(long seekToTimeUs) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set seek with NULL ExoPlayer.");
            return false;
        }
        exoplayerHandler.post(() -> {
            player.seekTo(seekToTimeUs / 1000);
            Log.i(TAG, String.format("ExoPlayer seeked to %d microseconds.", seekToTimeUs));
            Log.i(TAG,
                    String.format("New playback pos is %d us", player.getCurrentPosition() * 1000));
        });
        droppedFramesOnPlayerThread = 0;
        prerolled = false;
        notifiedStreamsReady = false;
        return true;
    }

    @CalledByNative
    private boolean setVolume(float volume) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set volume with NULL ExoPlayer.");
            return false;
        }
        exoplayerHandler.post(() -> { player.setVolume(volume); });
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
                Log.i(TAG, String.format("Setting ExoPlayer playback rate to %f", playbackRate));
                player.setPlaybackParameters(new PlaybackParameters(playbackRate));
            });
        } else {
            Log.i(TAG, String.format("Playback rate %f is unsupported", playbackRate));
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
            Log.i(TAG, "ExoPlayer is paused.");
        });
        return true;
    }

    @CalledByNative
    private boolean stop() {
        Log.i(TAG, "Called player stop.");
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
    public void writeSample(byte[] data, int sizeInBytes, long timestampUs, int rendererType,
            boolean isKeyFrame, boolean isEndOfStream) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to write samples with a NULL ExoPlayer or during destruction.");
            return;
        }
        if (!stopped) {
            if (rendererType == ExoPlayerRendererType.AUDIO) {
                audioMediaSource.writeSample(
                        data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
            } else if (videoMediaSource != null) {
                videoMediaSource.writeSample(
                        data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
            }
        }
    }

    @CalledByNative
    private synchronized long getCurrentPositionUs() {
        Log.i(TAG, String.format("Playback pos is %d", lastPlaybackPos));
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
        return new DefaultLoadControl.Builder().setBufferDurationsMs(750, 10000, 750, 750).build();
    }

    private boolean isAbleToProcessCommands() {
        return !destroying && player != null && exoplayerThread != null && exoplayerHandler != null;
    }

    @NativeMethods
    interface Natives {
        void onPlaybackStateChanged(long nativeExoPlayerBridge, int playbackState);

        void onInitialized(long nativeExoPlayerBridge);

        void onReady(long nativeExoPlayerBridge);

        void onError(long nativeExoPlayerBridge, String errorMessage);

        void setPlayingStatus(long nativeExoPlayerBridge, boolean isPlaying);
    }
}
