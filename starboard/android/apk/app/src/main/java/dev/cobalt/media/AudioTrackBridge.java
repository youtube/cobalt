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
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRouting;
import android.media.AudioRouting.OnRoutingChangedListener;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.os.Build;
import androidx.annotation.RequiresApi;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;

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

  private AtomicBoolean newAudioDeviceAdded = new AtomicBoolean(false);
  private AudioDeviceInfo currentRoutedDevice;
  private OnRoutingChangedListener onRoutingChangedListener;

  private static int getBytesPerSample(int audioFormat) {
    switch (audioFormat) {
      case AudioFormat.ENCODING_PCM_16BIT:
        return 2;
      case AudioFormat.ENCODING_PCM_FLOAT:
        return 4;
      case AudioFormat.ENCODING_INVALID:
      default:
        throw new RuntimeException("Unsupport audio format " + audioFormat);
    }
  }

  public AudioTrackBridge(
      int sampleType,
      int sampleRate,
      int channelCount,
      int preferredBufferSizeInBytes,
      boolean enableAudioRouting,
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
      // TODO: Investigate if we can use |CONTENT_TYPE_MOVIE| for AudioTrack
      //       used by video playback.
      attributes =
          new AudioAttributes.Builder()
              .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
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
    if (audioTrack != null && enableAudioRouting && Build.VERSION.SDK_INT >= 24) {
      currentRoutedDevice = audioTrack.getRoutedDevice();
      onRoutingChangedListener =
          new AudioRouting.OnRoutingChangedListener() {
            @Override
            public void onRoutingChanged(AudioRouting router) {
              AudioDeviceInfo newRoutedDevice = router.getRoutedDevice();
              if (currentRoutedDevice == null && newRoutedDevice != null) {
                if (!areAudioDevicesEqual(currentRoutedDevice, newRoutedDevice)) {
                  Log.v(
                      TAG,
                      String.format(
                          "New audio device %s added to AudioTrackAudioSink.",
                          newRoutedDevice.getProductName()));
                  newAudioDeviceAdded.set(true);
                }
              }
              currentRoutedDevice = newRoutedDevice;
            }
          };
      audioTrack.addOnRoutingChangedListener(onRoutingChangedListener, null);
    }
  }

  public Boolean isAudioTrackValid() {
    return audioTrack != null;
  }

  public void release() {
    if (audioTrack != null) {
      if (Build.VERSION.SDK_INT >= 24) {
        audioTrack.removeOnRoutingChangedListener(onRoutingChangedListener);
      }
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
  private void flush() {
    if (audioTrack == null) {
      Log.e(TAG, "Unable to flush with NULL audio track.");
      return;
    }
    audioTrack.flush();
    avSyncHeader = null;
    avSyncPacketBytesRemaining = 0;
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

  @RequiresApi(23)
  private boolean areAudioDevicesEqual(AudioDeviceInfo device1, AudioDeviceInfo device2) {
    if (device1.getId() == device2.getId()
        && device1.getType() == device2.getType()
        && device1.getType() == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER) {
      // This is a workaround for the emulator, which triggers an error callback when switching
      // between devices with different hash codes that are otherwise identical.
      return Arrays.equals(device1.getChannelCounts(), device2.getChannelCounts())
          && Arrays.equals(device1.getChannelIndexMasks(), device2.getChannelIndexMasks())
          && Arrays.equals(device1.getChannelMasks(), device2.getChannelMasks())
          && Arrays.equals(device1.getEncodings(), device2.getEncodings())
          && Arrays.equals(device1.getSampleRates(), device2.getSampleRates())
          && device1.getProductName().equals(device2.getProductName());
    }
    return device1.equals(device2);
  }

  @UsedByNative
  private boolean getAndResetHasNewAudioDeviceAdded() {
    return newAudioDeviceAdded.getAndSet(false);
  }
}
