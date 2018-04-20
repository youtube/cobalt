// Copyright 2017 Google Inc. All Rights Reserved.
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

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;
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
      int sampleType, int sampleRate, int channelCount, int framesPerChannel) {
    AudioTrackBridge audioTrackBridge =
        new AudioTrackBridge(sampleType, sampleRate, channelCount, framesPerChannel);
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
  @TargetApi(23)
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
}
