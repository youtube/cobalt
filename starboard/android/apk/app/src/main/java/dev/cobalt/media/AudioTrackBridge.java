// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.media.PlaybackParams;
import android.os.Build;
import androidx.annotation.RequiresApi;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Locale;

/**
 * A wrapper of the android AudioTrack class. Android AudioTrack would not start playing until the
 * buffer is fully filled once.
 */
@UsedByNative
public class AudioTrackBridge {
  // Also used by AudioOutputManager.
  static final int AV_SYNC_HEADER_V1_SIZE = 16;

  private AudioTrack audioTrack;
  private AudioTimestamp audioTimestamp = new AudioTimestamp();
  private long maxFramePositionSoFar = 0;
  private int defaultBufferSize = 0;

// Max/min supported playback rates for fast/slow audio. Audio at very low or very
// high playback rates are muted to preserve quality.
  private static final double minPlaybackRate = .25;
  private static final double maxPlaybackRate = 4.0;

  private final boolean tunnelModeEnabled;
  // The following variables are used only when |tunnelModeEnabled| is true.
  private ByteBuffer avSyncHeader;
  private int avSyncPacketBytesRemaining;

  private static int getBytesPerSample(int audioFormat) {
    switch (audioFormat) {
      case AudioFormat.ENCODING_PCM_16BIT:
        return 2;
      case AudioFormat.ENCODING_PCM_FLOAT:
        return 4;
      case AudioFormat.ENCODING_INVALID:
      default:
        throw new RuntimeException("Unsupported audio format " + audioFormat);
    }
  }

  // TODO: Pass error details to caller.
  public AudioTrackBridge(
      int sampleType,
      int sampleRate,
      int channelCount,
      int preferredBufferSizeInBytes,
      int tunnelModeAudioSessionId,
      boolean isWebAudio) {

    tunnelModeEnabled = tunnelModeAudioSessionId != -1;
    int channelConfig;
    switch (channelCount) {
      case 1:
        channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        break;
      case 2:
        channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        break;
      case 6:
        channelConfig = AudioFormat.CHANNEL_OUT_5POINT1;
        break;
      default:
        throw new RuntimeException("Unsupported channel count: " + channelCount);
    }

    AudioAttributes attributes;
    if (tunnelModeEnabled) {
      // Android 9.0 (Build.VERSION.SDK_INT >= 28) support v2 sync header that aligns sync header
      // with audio frame size. V1 sync header has alignment issues for multi-channel audio.
      if (Build.VERSION.SDK_INT < 28) {
        int frameSize = getBytesPerSample(sampleType) * channelCount;
        // This shouldn't happen as it should have been checked in
        // AudioOutputManager.generateTunnelModeAudioSessionId().
        if (AV_SYNC_HEADER_V1_SIZE % frameSize != 0) {
          audioTrack = null;
          String errorMessage =
              String.format(
                  Locale.US,
                  "Enable tunnel mode when frame size is unaligned, "
                      + "sampleType: %d, channel: %d, sync header size: %d.",
                  sampleType,
                  channelCount,
                  AV_SYNC_HEADER_V1_SIZE);
          Log.e(TAG, errorMessage);
          throw new RuntimeException(errorMessage);
        }
      }
      attributes =
          new AudioAttributes.Builder()
              .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
              .setFlags(AudioAttributes.FLAG_HW_AV_SYNC)
              .setUsage(AudioAttributes.USAGE_MEDIA)
              .build();
    } else {
      final int usage =
          isWebAudio ? AudioAttributes.USAGE_NOTIFICATION : AudioAttributes.USAGE_MEDIA;
      // TODO: Support ENCODING_E_AC3_JOC for api level 28 or later.
      final boolean isSurround =
          sampleType == AudioFormat.ENCODING_AC3 || sampleType == AudioFormat.ENCODING_E_AC3;
      final boolean useContentTypeMovie = isSurround || !isWebAudio;
      attributes =
          new AudioAttributes.Builder()
              .setContentType(
                  useContentTypeMovie
                      ? AudioAttributes.CONTENT_TYPE_MOVIE
                      : AudioAttributes.CONTENT_TYPE_MUSIC)
              .setUsage(usage)
              .build();
    }
    AudioFormat format =
        new AudioFormat.Builder()
            .setEncoding(sampleType)
            .setSampleRate(sampleRate)
            .setChannelMask(channelConfig)
            .build();

    int audioTrackBufferSize = preferredBufferSizeInBytes;
    // TODO: Investigate if this implementation could be refined.
    // It is not necessary to loop until 0 since there is new implementation based on
    // AudioTrack.getMinBufferSize(). Especially for tunnel mode, it would fail if audio HAL does
    // not support tunnel mode and then it is not helpful to retry.
    while (audioTrackBufferSize > 0) {
      try {
        audioTrack =
            new AudioTrack(
                attributes,
                format,
                audioTrackBufferSize * 2, /* Multiplied by 2 due to setPlaybackParams needing more buffer size for faster playbacks */
                AudioTrack.MODE_STREAM,
                tunnelModeEnabled
                    ? tunnelModeAudioSessionId
                    : AudioManager.AUDIO_SESSION_ID_GENERATE);
      } catch (Exception e) {
        audioTrack = null;
      }
      // AudioTrack ctor can fail in multiple, platform specific ways, so do a thorough check
      // before proceed.
      if (audioTrack != null) {
        if (audioTrack.getState() == AudioTrack.STATE_INITIALIZED) {
          break;
        }
        audioTrack = null;
      }
      audioTrackBufferSize /= 2;
    }

    // set the |defaultBufferSize| to half of what was set in the creation of the
    // AudioTrack. This will be used as a base when we have to dynamically change the buffer size
    // based on the selected playback rate.
    defaultBufferSize = (int) (audioTrack.getBufferSizeInFrames() / 2);

    String sampleTypeString = "ENCODING_INVALID";
    if (isAudioTrackValid()) {
      // If the AudioTrack encoding indicates compressed data,
      // e.g. AudioFormat.ENCODING_AC3, then the frame count returned
      // is the size of the AudioTrack buffer in bytes.
      // In such cases, audioTrackBufferSize does not have to be
      // multiplied with bytes per sample and channel count below.
      audioTrackBufferSize = audioTrack.getBufferSizeInFrames();
      switch (sampleType) {
        case AudioFormat.ENCODING_PCM_16BIT:
          sampleTypeString = "ENCODING_PCM_16BIT";
          audioTrackBufferSize *= getBytesPerSample(sampleType) * channelCount;
          break;
        case AudioFormat.ENCODING_PCM_FLOAT:
          sampleTypeString = "ENCODING_PCM_FLOAT";
          audioTrackBufferSize *= getBytesPerSample(sampleType) * channelCount;
          break;
        case AudioFormat.ENCODING_AC3:
          sampleTypeString = "ENCODING_AC3";
          break;
        case AudioFormat.ENCODING_E_AC3:
          sampleTypeString = "ENCODING_E_AC3";
          break;
        default:
          Log.i(TAG, String.format(Locale.US, "Unknown AudioFormat %d.", sampleType));
          break;
      }
    }
    Log.i(
        TAG,
        "AudioTrack created with AudioFormat %s and buffer size %d (preferred: %d)."
            + " The minimum buffer size is %d.",
        sampleTypeString,
        audioTrackBufferSize,
        preferredBufferSizeInBytes,
        AudioTrack.getMinBufferSize(sampleRate, channelConfig, sampleType));
  }

  public Boolean isAudioTrackValid() {
    return audioTrack != null;
  }

  public void release() {
    if (audioTrack != null) {
      audioTrack.release();
    }
    audioTrack = null;
    avSyncHeader = null;
    avSyncPacketBytesRemaining = 0;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public int setVolume(float gain) {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to setVolume with NULL audio track.");
      return 0;
    }
    return audioTrack.setVolume(gain);
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public int setPlaybackRate(float playbackRate) {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to setPlaybackRate with NULL audio track.");
      return 0;
    }
    try {
      audioTrack.setBufferSizeInFrames((int) (defaultBufferSize * playbackRate));
      PlaybackParams params = audioTrack.getPlaybackParams();
      params.setSpeed(playbackRate);
      audioTrack.setPlaybackParams(params);
      // Mute the audio if the playback is too slow or fast
      if (playbackRate <= minPlaybackRate || playbackRate >= maxPlaybackRate){
        audioTrack.setVolume(0);
      }
      else {
        audioTrack.setVolume(1);
      }
    } catch (IllegalArgumentException e){
      Log.e(TAG, String.format("Unable to set playbackRate, error: %s.", e.toString()));
      return 0;
    } catch (IllegalStateException e) {
      Log.e(TAG, String.format("Unable to set playbackRate, error: %s", e.toString()));
      return 0;
    }
    return 1;
  }

  // TODO (b/262608024): Have this method return a boolean and return false on failure.
  @SuppressWarnings("unused")
  @UsedByNative
  private void play() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to play with NULL audio track.");
      return;
    }
    try {
      audioTrack.play();
    } catch (IllegalStateException e) {
      Log.e(TAG, String.format(Locale.US, "Unable to play audio track, error: %s", e.toString()));
    }
  }

  // TODO (b/262608024): Have this method return a boolean and return false on failure.
  @SuppressWarnings("unused")
  @UsedByNative
  private void pause() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to pause with NULL audio track.");
      return;
    }
    try {
      audioTrack.pause();
    } catch (IllegalStateException e) {
      Log.e(TAG, String.format(Locale.US, "Unable to pause audio track, error: %s", e.toString()));
    }
  }

  // TODO (b/262608024): Have this method return a boolean and return false on failure.
  @SuppressWarnings("unused")
  @UsedByNative
  private void stop() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to stop with NULL audio track.");
      return;
    }
    try {
      audioTrack.stop();
    } catch (IllegalStateException e) {
      Log.e(TAG, String.format(Locale.US, "Unable to stop audio track, error: %s", e.toString()));
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void flush() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to flush with NULL audio track.");
      return;
    }
    audioTrack.flush();
    // Reset the states to allow reuse of |audioTrack| after flush() is called.  This can reduce
    // switch latency for passthrough playbacks.
    avSyncHeader = null;
    avSyncPacketBytesRemaining = 0;
    synchronized (this) {
      maxFramePositionSoFar = 0;
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int write(byte[] audioData, int sizeInBytes, long presentationTimeInMicroseconds) {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to write with NULL audio track.");
      return 0;
    }

    if (tunnelModeEnabled) {
      return writeWithAvSync(audioData, sizeInBytes, presentationTimeInMicroseconds);
    }

    return audioTrack.write(audioData, 0, sizeInBytes, AudioTrack.WRITE_NON_BLOCKING);
  }

  private int writeWithAvSync(
      byte[] audioData, int sizeInBytes, long presentationTimeInMicroseconds) {
    if (audioTrack == null) {
      throw new RuntimeException("writeWithAvSync() is called when audioTrack is null.");
    }

    if (!tunnelModeEnabled) {
      throw new RuntimeException("writeWithAvSync() is called when tunnelModeEnabled is false.");
    }

    long presentationTimeInNanoseconds = presentationTimeInMicroseconds * 1000;

    // Android support tunnel mode from 5.0 (API level 21), but the app has to manually write the
    // sync header before API 23, where the write() function with presentation timestamp is
    // introduced.
    // Set the following constant to |false| to test manual sync header writing in API level 23 or
    // later.  Note that the code to write sync header manually only supports v1 sync header.
    final boolean useAutoSyncHeaderWrite = true;
    if (useAutoSyncHeaderWrite) {
      ByteBuffer byteBuffer = ByteBuffer.wrap(audioData);
      return audioTrack.write(
          byteBuffer, sizeInBytes, AudioTrack.WRITE_NON_BLOCKING, presentationTimeInNanoseconds);
    }

    if (avSyncHeader == null) {
      avSyncHeader = ByteBuffer.allocate(AV_SYNC_HEADER_V1_SIZE);
      avSyncHeader.order(ByteOrder.BIG_ENDIAN);
      avSyncHeader.putInt(0x55550001);
    }

    if (avSyncPacketBytesRemaining == 0) {
      avSyncHeader.putInt(4, sizeInBytes);
      avSyncHeader.putLong(8, presentationTimeInNanoseconds);
      avSyncHeader.position(0);
      avSyncPacketBytesRemaining = sizeInBytes;
    }

    if (avSyncHeader.remaining() > 0) {
      int ret =
          audioTrack.write(avSyncHeader, avSyncHeader.remaining(), AudioTrack.WRITE_NON_BLOCKING);
      if (ret < 0) {
        avSyncPacketBytesRemaining = 0;
        return ret;
      }
      if (avSyncHeader.remaining() > 0) {
        return 0;
      }
    }

    int sizeToWrite = Math.min(avSyncPacketBytesRemaining, sizeInBytes);
    ByteBuffer byteBuffer = ByteBuffer.wrap(audioData);
    int ret = audioTrack.write(byteBuffer, sizeToWrite, AudioTrack.WRITE_NON_BLOCKING);
    if (ret < 0) {
      avSyncPacketBytesRemaining = 0;
      return ret;
    }
    avSyncPacketBytesRemaining -= ret;
    return ret;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int write(float[] audioData, int sizeInFloats) {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to write with NULL audio track.");
      return 0;
    }
    if (tunnelModeEnabled) {
      throw new RuntimeException("Float sample is not supported under tunnel mode.");
    }
    return audioTrack.write(audioData, 0, sizeInFloats, AudioTrack.WRITE_NON_BLOCKING);
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private AudioTimestamp getAudioTimestamp() {
    // TODO: Consider calling with TIMEBASE_MONOTONIC and returning that
    // information to the starboard audio sink.
    if (audioTrack == null) {
      Log.e(TAG, "Unable to getAudioTimestamp with NULL audio track.");
      return audioTimestamp;
    }
    // The `synchronized` is required as `maxFramePositionSoFar` can also be modified in flush().
    // TODO: Consider refactor the code to remove the dependency on `synchronized`.
    synchronized (this) {
      if (audioTrack.getTimestamp(audioTimestamp)) {
        // This conversion is safe, as only the lower bits will be set, since we
        // called |getTimestamp| without a timebase.
        // https://developer.android.com/reference/android/media/AudioTimestamp.html#framePosition
        audioTimestamp.framePosition &= 0x7FFFFFFF;
      } else {
        // Time stamps haven't been updated yet, assume playback hasn't started.
        audioTimestamp.framePosition = 0;
        audioTimestamp.nanoTime = System.nanoTime();
      }

      if (audioTimestamp.framePosition > maxFramePositionSoFar) {
        maxFramePositionSoFar = audioTimestamp.framePosition;
      } else {
        // The returned |audioTimestamp.framePosition| is not monotonically
        // increasing, and a monotonically increastion frame position is
        // required to calculate the playback time correctly, because otherwise
        // we would be going back in time.
        audioTimestamp.framePosition = maxFramePositionSoFar;
      }
    }

    return audioTimestamp;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int getUnderrunCount() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to call getUnderrunCount() with NULL audio track.");
      return 0;
    }
    return audioTrack.getUnderrunCount();
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int getStartThresholdInFrames() {
    if (Build.VERSION.SDK_INT >= 31) {
      return getStartThresholdInFramesV31();
    }
    return 0;
  }

  @RequiresApi(31)
  private int getStartThresholdInFramesV31() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to call getStartThresholdInFrames() with NULL audio track.");
      return 0;
    }
    return audioTrack.getStartThresholdInFrames();
  }
}
