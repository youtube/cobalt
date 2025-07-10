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
import androidx.annotation.Nullable;
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

import dev.cobalt.media.CobaltMediaCodecSelector;
import dev.cobalt.media.CobaltMediaSource;
import dev.cobalt.media.ExoPlayerFormatCreator;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;

@UsedByNative
@UnstableApi
public class ExoPlayerBridge {
    private ExoPlayer player;
    private final Context context;
    private CobaltMediaSource audioMediaSource;
    private CobaltMediaSource videoMediaSource;
    private long mNativeExoPlayerBridge;
    private boolean isAudioOnly;
    private boolean isVideoOnly;
    private HandlerThread exoplayerThread;
    private Handler exoplayerHandler;
    private long lastPlaybackPos = 0;
    private volatile boolean stopped = false;
    private boolean notifiedPrerolled = false;
    private boolean audioPrerolled = false;
    private boolean videoPrerolled = false;
    private volatile boolean destroying = false;
    private int droppedFramesOnPlayerThread = 0;
    private boolean firedEOS = false;

    public ExoPlayerBridge(Context context) {
        this.context = context;
    }

    private native void nativeOnPlaybackStateChanged(long nativeExoPlayerBridge, int playbackState);
    private native void nativeOnInitialized(long nativeExoPlayerBridge);
    private native void nativeOnError(long nativeExoPlayerBridge);
    private native void nativeSetPlayingStatus(long nativeExoPlayerBridge, boolean isPlaying);

    private class ExoPlayerListener implements Player.Listener {
        @Override
        public void onPlaybackStateChanged(@Player.State int playbackState) {
            Log.i(TAG, String.format("ExoPlayer state changed to %d", playbackState));
            switch (playbackState) {
                case Player.STATE_BUFFERING:
                case Player.STATE_IDLE:
                    if (!stopped) {
                        nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
                    }
                    break;
                case Player.STATE_READY:
                    Log.i(TAG,
                            player.isTunnelingEnabled() ? "TUNNEL MODE ENABLED"
                                                        : "TUNNEL MODE DISABLED");
                    // No native callback for STATE_READY in original, maintaining omission.
                    break;
                case Player.STATE_ENDED:
                    if (!stopped && !firedEOS) {
                        firedEOS = true;
                        nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
                    }
                    break;
            }
        }

        @Override
        public void onIsPlayingChanged(boolean isPlaying) {
            Log.i(TAG, isPlaying ? "Exoplayer is playing." : "Exoplayer is not playing.");
            if (!stopped) {
                nativeSetPlayingStatus(mNativeExoPlayerBridge, isPlaying);
            }
        }

        @Override
        public void onPlayerError(@NonNull PlaybackException error) {
            Log.e(TAG,
                    String.format("ExoPlayer playback error %s, code: %s, cause: %s",
                            error.getMessage(), error.getErrorCodeName(),
                            error.getCause() != null ? error.getCause().getMessage() : "N/A"));
            nativeOnError(mNativeExoPlayerBridge);
        }

        @Override
        public void onPlayWhenReadyChanged(
                boolean playWhenReady, @Player.PlayWhenReadyChangeReason int reason) {
            Log.i(TAG,
                    String.format("onPlayWhenReadyChanged() with playWhenReady %b and reason %d",
                            playWhenReady, reason));
        }

        @Override
        public void onPlaybackSuppressionReasonChanged(
                @Player.PlaybackSuppressionReason int playbackSuppressionReason) {
            Log.i(TAG,
                    String.format("onPlaybackSuppressionReasonChanged() with reason %d",
                            playbackSuppressionReason));
        }
    }

    private final class ExoPlayerEventLogger implements AnalyticsListener {
        @Override
        public void onDroppedVideoFrames(
                @NonNull EventTime eventTime, int droppedFrames, long elapsedMs) {
            Log.i(TAG, String.format("REPORTING %d DROPPED VIDEO FRAMES", droppedFrames));
            droppedFramesOnPlayerThread += droppedFrames;
        }
    }

    @UsedByNative
    public void createExoPlayer(long nativeExoPlayerBridge, @Nullable Surface surface,
            byte[] audioConfigurationData, boolean isAudioOnly, boolean isVideoOnly, int sampleRate,
            int channelCount, int width, int height, int fps, int videoBitrate, String audioMime,
            String videoMime) {
        if (surface != null && !surface.isValid()) {
            Log.w(TAG, "Provided surface is invalid.");
        }

        this.exoplayerThread = new HandlerThread("ExoPlayerThread", Process.THREAD_PRIORITY_AUDIO);
        exoplayerThread.start();
        this.exoplayerHandler = new Handler(exoplayerThread.getLooper());
        this.isAudioOnly = isAudioOnly;
        this.isVideoOnly = isVideoOnly; // Ensure this flag is set
        mNativeExoPlayerBridge = nativeExoPlayerBridge;

        boolean preferTunnelMode =
                false; // Original code always sets to false, can be removed if not dynamic
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
            player.addListener(new ExoPlayerListener());

            if (!isAudioOnly
                    && surface
                            != null) { // Only set surface if not audio only and surface is provided
                Log.i(TAG, "Setting video surface");
                player.setVideoSurface(surface);
            }

            MediaSource playbackMediaSource;
            if (isAudioOnly) {
                Log.i(TAG, "Creating audio only ExoPlayer");
                audioMediaSource = new CobaltMediaSource(true, this, audioConfigurationData,
                        ExoPlayerFormatCreator.createAudioFormat(
                                audioMime, audioConfigurationData, sampleRate, channelCount));
                playbackMediaSource = audioMediaSource;
            } else if (isVideoOnly) {
                Log.i(TAG, "Creating video only ExoPlayer");
                videoMediaSource = new CobaltMediaSource(false, this, null,
                        ExoPlayerFormatCreator.createVideoFormat(
                                videoMime, width, height, (float) fps, videoBitrate));
                playbackMediaSource = videoMediaSource;
            } else { // Audio and Video
                audioMediaSource = new CobaltMediaSource(true, this, audioConfigurationData,
                        ExoPlayerFormatCreator.createAudioFormat(
                                audioMime, audioConfigurationData, sampleRate, channelCount));
                videoMediaSource = new CobaltMediaSource(false, this, null,
                        ExoPlayerFormatCreator.createVideoFormat(
                                videoMime, width, height, (float) fps, videoBitrate));
                playbackMediaSource =
                        new MergingMediaSource(true, audioMediaSource, videoMediaSource);
            }

            player.setMediaSource(playbackMediaSource);
            player.addAnalyticsListener(new ExoPlayerEventLogger());
            Log.i(TAG, "ExoPlayer created and prepared.");
            player.prepare();
            player.play();
        });
    }

    private synchronized void updatePlaybackPos() {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to update timestamps with NULL ExoPlayer.");
            return;
        }
        lastPlaybackPos = player.getCurrentPosition() * 1000;
        if (!stopped) {
            exoplayerHandler.postDelayed(this::updatePlaybackPos, 75);
        } else {
            Log.i(TAG, "Stopped, not posting delayed updatePlaybackPos.");
        }
    }

    @UsedByNative
    public void destroyExoPlayer() throws InterruptedException {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to destroy player with NULL ExoPlayer.");
            return;
        }
        Log.i(TAG, "Removing callbacks and preparing to destroy player.");
        exoplayerHandler.removeCallbacksAndMessages(null);
        destroying = true;

        exoplayerHandler.post(() -> {
            stopped = true; // Signal stop for the updatePlaybackPos loop
            Log.i(TAG, "Releasing ExoPlayer.");
            if (player != null) {
                player.release();
                player = null;
            } else {
                Log.i(TAG, "Player is already null, no release needed.");
            }
            destroying = false;
            stopped = false;
            firedEOS = false; // Reset EOS flag
            notifiedPrerolled = false;
            exoplayerHandler.getLooper().quitSafely();
            Log.i(TAG, "ExoPlayer released and thread quit safely.");
        });

        exoplayerThread.join(10000); // Wait for thread to finish
        if (exoplayerThread.isAlive()) {
            Log.w(TAG, "ExoPlayer thread did not join after 10 seconds.");
        }
        exoplayerHandler = null;
        exoplayerThread = null;
        Log.i(TAG, "ExoPlayer thread references cleared.");
    }

    @UsedByNative
    private boolean seek(long seekToTimeUs) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set seek with NULL ExoPlayer.");
            return false;
        }
        exoplayerHandler.post(() -> {
            player.seekTo(seekToTimeUs / 1000);
            Log.i(TAG, String.format("ExoPlayer seeked to %d microseconds.", seekToTimeUs));
            droppedFramesOnPlayerThread = 0;
        });
        return true;
    }

    @UsedByNative
    private boolean setVolume(float volume) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set volume with NULL ExoPlayer.");
            return false;
        }
        exoplayerHandler.post(() -> {
            player.setVolume(volume);
            Log.i(TAG, String.format("Setting ExoPlayer volume to %f", volume));
        });
        return true;
    }

    @UsedByNative
    private void setPlaybackRate(float playbackRate) {
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Cannot set playback rate with NULL ExoPlayer.");
            return;
        }
        if (playbackRate > 0.0f) {
            exoplayerHandler.post(() -> {
                player.setPlaybackParameters(new PlaybackParameters(playbackRate));
                Log.i(TAG, String.format("Setting ExoPlayer playback rate to %f", playbackRate));
            });
        }
    }

    @UsedByNative
    private boolean play() {
        Log.i(TAG, "Called ExoPlayerBridge.play()");
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to play with NULL ExoPlayer.");
            return false;
        }
        exoplayerHandler.post(() -> {
            if (!player.isPlaying()) { // Only call play() if not already playing
                player.play();
                Log.i(TAG, "Called play() on ExoPlayer.");
            }
        });
        return true;
    }

    @UsedByNative
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

    @UsedByNative
    private boolean stop() {
        Log.i(TAG, "Called player stop.");
        if (!isAbleToProcessCommands()) {
            Log.e(TAG, "Unable to stop with NULL ExoPlayer. Assuming stopped successfully.");
            return true; // Return true as if stopped to prevent native side issues
        }
        if (!stopped) { // Only stop if not already flagged as stopped
            exoplayerHandler.removeCallbacks(this::updatePlaybackPos);
            exoplayerHandler.post(() -> {
                player.stop();
                stopped = true;
                Log.i(TAG, "ExoPlayer is stopped.");
            });
        }
        return true;
    }

    @UsedByNative
    public void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isAudio,
            boolean isKeyFrame, boolean isEndOfStream) {
        if (!isAbleToProcessCommands()) {
            // Log.e(TAG, "Unable to write samples with a NULL ExoPlayer or during destruction.");
            return; // No need to log frequently for sample writes
        }
        if (!stopped) {
            if (isAudio) {
                audioPrerolled |= audioMediaSource.writeSample(
                        data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
            } else if (videoMediaSource
                    != null) { // Check videoMediaSource is not null for video samples
                videoPrerolled |= videoMediaSource.writeSample(
                        data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
            }
        }
    }

    @UsedByNative
    private synchronized long getCurrentPositionUs() {
        return lastPlaybackPos;
    }

    @UsedByNative
    private synchronized int getDroppedFrames() {
        return droppedFramesOnPlayerThread;
    }

    public void onStreamCreated() {
        if (!isAbleToProcessCommands()) {
            Log.i(TAG, "Player is being destroyed or not ready, cannot signal stream creation.");
            return;
        }

        // Determine if all required streams are initialized
        boolean allStreamsInitialized = (isAudioOnly && audioMediaSource.isInitialized())
                || (isVideoOnly && videoMediaSource.isInitialized())
                || (audioMediaSource != null && audioMediaSource.isInitialized()
                        && videoMediaSource != null && videoMediaSource.isInitialized());

        if (allStreamsInitialized) {
            nativeOnInitialized(mNativeExoPlayerBridge);
        }
    }

    private DefaultLoadControl createLoadControl() {
        return new DefaultLoadControl.Builder().setBufferDurationsMs(500, 50000, 500, 500).build();
    }

    public void onPrerolled(boolean isAudio) {
        if (!isAbleToProcessCommands()) {
            Log.i(TAG, "Player is being destroyed or not ready, cannot signal preroll.");
            return;
        }

        if (isAudio) {
            audioPrerolled = true;
        } else {
            videoPrerolled = true;
        }

        boolean allStreamsPrerolled = (isAudioOnly && audioPrerolled)
                || (isVideoOnly && videoPrerolled) || (audioPrerolled && videoPrerolled);

        if (allStreamsPrerolled && !notifiedPrerolled) {
            notifiedPrerolled = true;
            Log.i(TAG,
                    String.format("Media prerolled (audio: %b, video: %b). Ready to play.",
                            audioPrerolled, videoPrerolled));
            nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, Player.STATE_READY);
            exoplayerHandler.post(this::updatePlaybackPos);
        }
    }

    private boolean isAbleToProcessCommands() {
        return !destroying && player != null && exoplayerThread != null && exoplayerHandler != null;
    }

    public void notifyEOS() {
        if (!firedEOS) { // Only fire EOS once
            nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, Player.STATE_ENDED);
            firedEOS = true;
        }
    }
}
