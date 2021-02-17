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

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import androidx.annotation.RequiresApi;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Creates and destroys AudioTrackBridge and handles the volume change. */
public class AudioOutputManager implements CobaltMediaSession.UpdateVolumeListener {
  private List<AudioTrackBridge> audioTrackBridgeList;
  private Context context;

  public AudioOutputManager(Context context) {
    this.context = context;
    audioTrackBridgeList = new ArrayList<AudioTrackBridge>();
  }

  @Override
  public void onUpdateVolume(float gain) {
    for (AudioTrackBridge audioTrackBridge : audioTrackBridgeList) {
      audioTrackBridge.setVolume(gain);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  AudioTrackBridge createAudioTrackBridge(
      int sampleType,
      int sampleRate,
      int channelCount,
      int preferredBufferSizeInBytes,
      boolean enableAudioRouting,
      int tunnelModeAudioSessionId) {
    AudioTrackBridge audioTrackBridge =
        new AudioTrackBridge(
            sampleType,
            sampleRate,
            channelCount,
            preferredBufferSizeInBytes,
            enableAudioRouting,
            tunnelModeAudioSessionId);
    if (!audioTrackBridge.isAudioTrackValid()) {
      Log.e(TAG, "AudioTrackBridge has invalid audio track");
      return null;
    }
    audioTrackBridgeList.add(audioTrackBridge);
    return audioTrackBridge;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  void destroyAudioTrackBridge(AudioTrackBridge audioTrackBridge) {
    audioTrackBridge.release();
    audioTrackBridgeList.remove(audioTrackBridge);
  }

  /** Returns the maximum number of HDMI channels. */
  @SuppressWarnings("unused")
  @UsedByNative
  int getMaxChannels() {
    // The aac audio decoder on this platform will switch its output from 5.1
    // to stereo right before providing the first output buffer when
    // attempting to decode 5.1 input.  Since this heavily violates invariants
    // of the shared starboard player framework, disable 5.1 on this platform.
    // It is expected that we will be able to resolve this issue with Xiaomi
    // by Android P, so only do this workaround for SDK versions < 27.
    if (android.os.Build.MODEL.equals("MIBOX3") && android.os.Build.VERSION.SDK_INT < 27) {
      return 2;
    }

    if (Build.VERSION.SDK_INT >= 23) {
      return getMaxChannelsV23();
    }
    return 2;
  }

  /** Returns the maximum number of HDMI channels for API 23 and above. */
  @RequiresApi(23)
  private int getMaxChannelsV23() {
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    AudioDeviceInfo[] deviceInfos = audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
    int maxChannels = 2;
    for (AudioDeviceInfo info : deviceInfos) {
      int type = info.getType();
      if (type == AudioDeviceInfo.TYPE_HDMI || type == AudioDeviceInfo.TYPE_HDMI_ARC) {
        int[] channelCounts = info.getChannelCounts();
        if (channelCounts.length == 0) {
          // An empty array indicates that the device supports arbitrary channel masks.
          return 8;
        }
        for (int count : channelCounts) {
          maxChannels = Math.max(maxChannels, count);
        }
      }
    }
    return maxChannels;
  }

  /** Convert AudioDeviceInfo.TYPE_* to name in String */
  @RequiresApi(23)
  private static String getDeviceTypeName(int device_type) {
    switch (device_type) {
      case AudioDeviceInfo.TYPE_AUX_LINE:
        return "TYPE_AUX_LINE";
      case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
        return "TYPE_BLUETOOTH_A2DP";
      case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
        return "TYPE_BLUETOOTH_SCO";
      case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE:
        return "TYPE_BUILTIN_EARPIECE";
      case AudioDeviceInfo.TYPE_BUILTIN_MIC:
        return "TYPE_BUILTIN_MIC";
      case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
        return "TYPE_BUILTIN_SPEAKER";
      case AudioDeviceInfo.TYPE_BUS:
        return "TYPE_BUS";
      case AudioDeviceInfo.TYPE_DOCK:
        return "TYPE_DOCK";
      case AudioDeviceInfo.TYPE_FM:
        return "TYPE_FM";
      case AudioDeviceInfo.TYPE_FM_TUNER:
        return "TYPE_FM_TUNER";
      case AudioDeviceInfo.TYPE_HDMI:
        return "TYPE_HDMI";
      case AudioDeviceInfo.TYPE_HDMI_ARC:
        return "TYPE_HDMI_ARC";
      case AudioDeviceInfo.TYPE_IP:
        return "TYPE_IP";
      case AudioDeviceInfo.TYPE_LINE_ANALOG:
        return "TYPE_LINE_ANALOG";
      case AudioDeviceInfo.TYPE_LINE_DIGITAL:
        return "TYPE_LINE_DIGITAL";
      case AudioDeviceInfo.TYPE_TELEPHONY:
        return "TYPE_TELEPHONY";
      case AudioDeviceInfo.TYPE_TV_TUNER:
        return "TYPE_TV_TUNER";
      case AudioDeviceInfo.TYPE_UNKNOWN:
        return "TYPE_UNKNOWN";
      case AudioDeviceInfo.TYPE_USB_ACCESSORY:
        return "TYPE_USB_ACCESSORY";
      case AudioDeviceInfo.TYPE_USB_DEVICE:
        return "TYPE_USB_DEVICE";
      case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
        return "TYPE_WIRED_HEADPHONES";
      case AudioDeviceInfo.TYPE_WIRED_HEADSET:
        return "TYPE_WIRED_HEADSET";
      default:
        // This may include constants introduced after API 23.
        return String.format("TYPE_UNKNOWN (%d)", device_type);
    }
  }

  /** Convert audio encodings in int[] to common separated values in String */
  @RequiresApi(23)
  private static String getEncodingNames(final int[] encodings) {
    StringBuffer encodings_in_string = new StringBuffer("[");
    for (int i = 0; i < encodings.length; ++i) {
      switch (encodings[i]) {
        case AudioFormat.ENCODING_DEFAULT:
          encodings_in_string.append("DEFAULT");
          break;
        case AudioFormat.ENCODING_PCM_8BIT:
          encodings_in_string.append("PCM_8BIT");
          break;
        case AudioFormat.ENCODING_PCM_16BIT:
          encodings_in_string.append("PCM_16BIT");
          break;
        case AudioFormat.ENCODING_PCM_FLOAT:
          encodings_in_string.append("PCM_FLOAT");
          break;
        case AudioFormat.ENCODING_DTS:
          encodings_in_string.append("DTS");
          break;
        case AudioFormat.ENCODING_DTS_HD:
          encodings_in_string.append("DTS_HD");
          break;
        case AudioFormat.ENCODING_AC3:
          encodings_in_string.append("AC3");
          break;
        case AudioFormat.ENCODING_E_AC3:
          encodings_in_string.append("E_AC3");
          break;
        case AudioFormat.ENCODING_IEC61937:
          encodings_in_string.append("IEC61937");
          break;
        case AudioFormat.ENCODING_INVALID:
          encodings_in_string.append("INVALID");
          break;
        default:
          // This may include constants introduced after API 23.
          encodings_in_string.append(String.format("UNKNOWN (%d)", encodings[i]));
          break;
      }
      if (i != encodings.length - 1) {
        encodings_in_string.append(", ");
      }
    }
    encodings_in_string.append(']');
    return encodings_in_string.toString();
  }

  /** Dump all audio output devices. */
  public void dumpAllOutputDevices() {
    if (Build.VERSION.SDK_INT < 23) {
      Log.i(TAG, "dumpAllOutputDevices() is only supported in API level 23 or above.");
      return;
    }

    Log.i(TAG, "Dumping all audio output devices:");

    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    AudioDeviceInfo[] deviceInfos = audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);

    for (AudioDeviceInfo info : deviceInfos) {
      Log.i(
          TAG,
          String.format(
              "  Audio Device: %s, channels: %s, sample rates: %s, encodings: %s",
              getDeviceTypeName(info.getType()),
              Arrays.toString(info.getChannelCounts()),
              Arrays.toString(info.getSampleRates()),
              getEncodingNames(info.getEncodings())));
    }
  }

  /** Returns the minimum buffer size of AudioTrack. */
  @SuppressWarnings("unused")
  @UsedByNative
  int getMinBufferSize(int sampleType, int sampleRate, int channelCount) {
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
    return AudioTrack.getMinBufferSize(sampleRate, channelConfig, sampleType);
  }

  /** Generate audio session id used by tunneled playback. */
  @SuppressWarnings("unused")
  @UsedByNative
  int generateTunnelModeAudioSessionId(int numberOfChannels) {
    // Android 9.0 (Build.VERSION.SDK_INT >= 28) support v2 sync header that
    // aligns sync header with audio frame size. V1 sync header has alignment
    // issues for multi-channel audio.
    if (Build.VERSION.SDK_INT < 28) {
      // Currently we only support int16 under tunnel mode.
      final int sampleSizeInBytes = 2;
      final int frameSizeInBytes = sampleSizeInBytes * numberOfChannels;
      if (AudioTrackBridge.AV_SYNC_HEADER_V1_SIZE % frameSizeInBytes != 0) {
        Log.w(
            TAG,
            String.format(
                "Disable tunnel mode due to sampleSizeInBytes (%d) * numberOfChannels (%d) isn't"
                    + " aligned to AV_SYNC_HEADER_V1_SIZE (%d).",
                sampleSizeInBytes, numberOfChannels, AudioTrackBridge.AV_SYNC_HEADER_V1_SIZE));
        return -1;
      }
    }
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    return audioManager.generateAudioSessionId();
  }
}
