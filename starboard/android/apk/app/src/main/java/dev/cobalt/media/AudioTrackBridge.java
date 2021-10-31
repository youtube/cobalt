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
import android.os.Build;
import androidx.annotation.RequiresApi;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

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
      int tunnelModeAudioSessionId) {

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
                  "Enable tunnel mode when frame size is unaligned, "
                      + "sampleType: %d, channel: %d, sync header size: %d.",
                  sampleType, channelCount, AV_SYNC_HEADER_V1_SIZE);
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
      // TODO: Support ENCODING_E_AC3_JOC for api level 28 or later.
      final boolean is_surround =
          sampleType == AudioFormat.ENCODING_AC3 || sampleType == AudioFormat.ENCODING_E_AC3;
      // TODO: We start to enforce |CONTENT_TYPE_MOVIE| for surround playback, investigate if we
      //       can use |CONTENT_TYPE_MOVIE| for all non-surround AudioTrack used by video
      //       playback.
      attributes =
          new AudioAttributes.Builder()
              .setContentType(
                  is_surround
                      ? AudioAttributes.CONTENT_TYPE_MOVIE
                      : AudioAttributes.CONTENT_TYPE_MUSIC)
              .setUsage(AudioAttributes.USAGE_MEDIA)
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
                audioTrackBufferSize,
                AudioTrack.MODE_STREAM,
                tunnelModeEnabled
                    ? tunnelModeAudioSessionId
                    : AudioManager.AUDIO_SESSION_ID_GENERATE);
      } catch (Exception e) {
        audioTrack = null;
      }
      // AudioTrack ctor can fail in multiple, platform specific ways, so do a thorough check
      // before proceed.
      if (audioTrack != null && audioTrack.getState() == AudioTrack.STATE_INITIALIZED) {
        break;
      }
      audioTrackBufferSize /= 2;
    }
    Log.i(
        TAG,
        String.format(
            "AudioTrack created with buffer size %d (preferred: %d).  The minimum buffer size is"
                + " %d.",
            audioTrackBufferSize,
            preferredBufferSizeInBytes,
            AudioTrack.getMinBufferSize(sampleRate, channelConfig, sampleType)));
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
  private void play() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to play with NULL audio track.");
      return;
    }
    audioTrack.play();
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void pause() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to pause with NULL audio track.");
      return;
    }
    audioTrack.pause();
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void stop() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to stop with NULL audio track.");
      return;
    }
    audioTrack.stop();
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

    if (Build.VERSION.SDK_INT >= 23) {
      return audioTrack.write(audioData, 0, sizeInBytes, AudioTrack.WRITE_NON_BLOCKING);
    } else {
      ByteBuffer byteBuffer = ByteBuffer.wrap(audioData);
      return audioTrack.write(byteBuffer, sizeInBytes, AudioTrack.WRITE_NON_BLOCKING);
    }
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
    if (useAutoSyncHeaderWrite && Build.VERSION.SDK_INT >= 23) {
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
    if (Build.VERSION.SDK_INT >= 24) {
      return getUnderrunCountV24();
    }
    // The function getUnderrunCount() is added in API level 24.
    return 0;
  }

  @RequiresApi(24)
  private int getUnderrunCountV24() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to call getUnderrunCount() with NULL audio track.");
      return 0;
    }
    return audioTrack.getUnderrunCount();
  }
}
