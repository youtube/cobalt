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

// A wrapper of the android AudioTrack class.
// Android AudioTrack would not start playing until the buffer is fully
// filled once.
@UsedByNative
public class AudioTrackBridge {
  private AudioTrack audioTrack;
  private AudioTimestamp audioTimestamp = new AudioTimestamp();
  private long maxFramePositionSoFar = 0;

  private ByteBuffer avSyncHeader;
  private int bytesUntilNextAvSync;
  private int tunnelingAudioSessionId = -1;
  private static final int AV_SYNC_HEADER_V1_SIZE = 16;

  public static int getBytesPerSample(int audioFormat) {
    switch (audioFormat) {
      case AudioFormat.ENCODING_PCM_16BIT:
        return 2;
      case AudioFormat.ENCODING_PCM_FLOAT:
        return 4;
      case AudioFormat.ENCODING_INVALID:
      default:
        // FIXME: if starboard support more formats, it
        // should change accordingly
        throw new RuntimeException("Unsupport audio format " + audioFormat);
    }
  }

  public AudioTrackBridge(int sampleType, int sampleRate, int channelCount,
          int preferredBufferSizeInBytes, int tunnelingAudioSessionIdVal) {
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
    tunnelingAudioSessionId = tunnelingAudioSessionIdVal;

    AudioAttributes attributes;
    if (tunnelingAudioSessionIdVal == -1) {
      attributes = new AudioAttributes.Builder()
        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
        .setUsage(AudioAttributes.USAGE_MEDIA)
        .build();
    } else {
      // Android 9.0 support v2 sync header that align sync header and audio data
      // V1 sync header has the align problem for multichannel audio.
      if (Build.VERSION.SDK_INT < 28) {
        int frameSize = getBytesPerSample(sampleType) * channelCount;
        if (AV_SYNC_HEADER_V1_SIZE % frameSize != 0) {
            audioTrack = null;
            Log.w(
                TAG,
                String.format(
                  "Unsupport tunneled mode due to unaligned bytes problem " +
                  "sampleType:%d channel:%d sync header size %d",
                  sampleType, channelCount, AV_SYNC_HEADER_V1_SIZE));
            return;
        }
      }
      attributes = new AudioAttributes.Builder()
        .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
        .setFlags(AudioAttributes.FLAG_HW_AV_SYNC)
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
    // FIXME: it is not necessary to loop until 0 since there is new
    // implementation based on AudioTrack getMinBufferSize. Especially
    // for tunneled mode, it would fail if audio HAL do not support and then
    // it is not helpful to retry.
    while (audioTrackBufferSize > 0) {
      try {
        audioTrack = new AudioTrack(attributes, format, audioTrackBufferSize,
            AudioTrack.MODE_STREAM,
            (tunnelingAudioSessionIdVal != -1) ? tunnelingAudioSessionIdVal
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
            "AudioTrack created with buffer size %d (prefered: %d).  The minimum buffer size is"
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
    bytesUntilNextAvSync = 0;
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
  private void flush() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to flush with NULL audio track.");
      return;
    }
    audioTrack.flush();
    avSyncHeader = null;
    bytesUntilNextAvSync = 0;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int write(byte[] audioData, int sizeInBytes, long presentationTimeNs) {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to write with NULL audio track.");
      return 0;
    }

    if (tunnelingAudioSessionId != -1) {
      return writeWithAvSync(audioData, sizeInBytes, presentationTimeNs);
    }

    if (Build.VERSION.SDK_INT >= 23) {
      return audioTrack.write(audioData, 0, sizeInBytes, AudioTrack.WRITE_NON_BLOCKING);
    } else {
      ByteBuffer byteBuffer = ByteBuffer.wrap(audioData);
      return audioTrack.write(byteBuffer, sizeInBytes, AudioTrack.WRITE_NON_BLOCKING);
    }
  }

  @SuppressWarnings("unused")
  private int writeWithAvSync(
      byte[] audioData, int sizeInBytes, long presentationTimeNs) {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to write with NULL audio track.");
      return 0;
    }

    if (tunnelingAudioSessionId == -1) {
      return write(audioData, sizeInBytes, presentationTimeNs);
    }

    // Android support tunneled mode from 5.0 (API version 21). However
    // below API start support on API version 23
    // Unit Test for avSyncHeader writing by itself via change below to
    // if (Build.VERSION.SDK_INT >= 100) {
    if (Build.VERSION.SDK_INT >= 23) {
      ByteBuffer byteBuffer = ByteBuffer.wrap(audioData);
      return audioTrack.write(
          byteBuffer, sizeInBytes, AudioTrack.WRITE_NON_BLOCKING, presentationTimeNs);
    }

    if (avSyncHeader == null) {
      avSyncHeader = ByteBuffer.allocate(AV_SYNC_HEADER_V1_SIZE);
      avSyncHeader.order(ByteOrder.BIG_ENDIAN);
      avSyncHeader.putInt(0x55550001);
    }
    if (bytesUntilNextAvSync == 0) {
      avSyncHeader.putInt(4, sizeInBytes);
      avSyncHeader.putLong(8, presentationTimeNs);
      avSyncHeader.position(0);
      bytesUntilNextAvSync = sizeInBytes;
    }
    int avSyncHeaderBytesRemaining = avSyncHeader.remaining();
    if (avSyncHeaderBytesRemaining > 0) {
      int result = audioTrack.write(
          avSyncHeader, avSyncHeaderBytesRemaining, AudioTrack.WRITE_NON_BLOCKING);
      if (result < 0) {
        bytesUntilNextAvSync = 0;
        return result;
      }
      if (result < avSyncHeaderBytesRemaining) {
        return 0;
      }
    }

    int sizeToWrite = Math.min(bytesUntilNextAvSync, sizeInBytes);
    ByteBuffer byteBuffer = ByteBuffer.wrap(audioData);
    int result = audioTrack.write(byteBuffer, sizeToWrite, AudioTrack.WRITE_NON_BLOCKING);
    if (result < 0) {
      bytesUntilNextAvSync = 0;
      return result;
    }
    bytesUntilNextAvSync -= result;
    return result;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int write(float[] audioData, int sizeInFloats) {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to write with NULL audio track.");
      return 0;
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

    // TODO: This is required for correctness of the audio sink, because
    // otherwise we would be going back in time. Investigate the impact it has
    // on playback.  All empirical measurements so far suggest that it should
    // be negligible.
    if (audioTimestamp.framePosition < maxFramePositionSoFar) {
      audioTimestamp.framePosition = maxFramePositionSoFar;
    }
    maxFramePositionSoFar = audioTimestamp.framePosition;

    return audioTimestamp;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int getUnderrunCount() {
    if (Build.VERSION.SDK_INT >= 24) {
      return getUnderrunCountV24();
    }
    // The funtion getUnderrunCount() is added in API level 24.
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
