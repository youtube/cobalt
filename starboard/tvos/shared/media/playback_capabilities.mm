// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/media/playback_capabilities.h"

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#import <VideoToolbox/VideoToolbox.h>

#include <mutex>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/system.h"
#include "starboard/tvos/shared/observer_registry.h"
#include "starboard/tvos/shared/uikit_media_session_client.h"

namespace starboard {

namespace {

const CMVideoCodecType kCMVideoCodecType_VP9 = 'vp09';

bool IsAppleTVHDPriv() {
  char model_name[128];
  bool succeeded = SbSystemGetProperty(kSbSystemPropertyModelName, model_name,
                                       sizeof(model_name));
  SB_DCHECK(succeeded);
  return succeeded && strcmp(model_name, "AppleTV5-3") == 0;
}

bool IsAppleTV4KPriv() {
  char model_name[128];
  bool succeeded = SbSystemGetProperty(kSbSystemPropertyModelName, model_name,
                                       sizeof(model_name));
  SB_DCHECK(succeeded);
  return succeeded && strcmp(model_name, "AppleTV6-2") == 0;
}

SbMediaAudioConnector GetConnectorFromAVAudioSessionPort(
    AVAudioSessionPort port) {
  // Output ports
  if ([port isEqual:AVAudioSessionPortAirPlay]) {
    return kSbMediaAudioConnectorRemoteOther;
  }
  if ([port isEqual:AVAudioSessionPortBluetoothA2DP] ||
      [port isEqual:AVAudioSessionPortBluetoothLE]) {
    return kSbMediaAudioConnectorBluetooth;
  }
  if ([port isEqual:AVAudioSessionPortBuiltInReceiver] ||
      [port isEqual:AVAudioSessionPortBuiltInSpeaker]) {
    return kSbMediaAudioConnectorBuiltIn;
  }
  if ([port isEqual:AVAudioSessionPortHDMI]) {
    return kSbMediaAudioConnectorHdmi;
  }
  if ([port isEqual:AVAudioSessionPortHeadphones]) {
    return kSbMediaAudioConnectorAnalog;
  }
  if ([port isEqual:AVAudioSessionPortLineOut]) {
    return kSbMediaAudioConnectorUnknown;
  }

  // For other non output ports, we set the connector to unknown.
  // Input ports:
  // AVAudioSessionPortBuiltInMic
  // AVAudioSessionPortContinuityMicrophone
  // AVAudioSessionPortHeadsetMic
  // AVAudioSessionPortLineIn
  // I/O ports:
  // AVAudioSessionPortAVB
  // AVAudioSessionPortPCI
  // AVAudioSessionPortBluetoothHFP
  // AVAudioSessionPortCarAudio
  // AVAudioSessionPortDisplayPort
  // AVAudioSessionPortFireWire
  // AVAudioSessionPortThunderbolt
  // AVAudioSessionPortUSBAudio
  // AVAudioSessionPortVirtual
  return kSbMediaAudioConnectorUnknown;
}

class PlaybackCapabilitiesImpl {
 public:
  PlaybackCapabilitiesImpl()
      : is_hw_vp9_supported_(
            VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9)),
        is_apple_tv_hd_(IsAppleTVHDPriv()),
        is_apple_tv_4k_(IsAppleTV4KPriv()) {
    ObserverRegistry::RegisterObserver(&observer_);
    @autoreleasepool {
      route_change_observer_ = [[NSNotificationCenter defaultCenter]
          addObserverForName:AVAudioSessionRouteChangeNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    this->OnAudioRouteChanged(notification);
                  }];

      // Changing airplay output (like HomePod) won't trigger route change
      // notification. We use UIApplicationWillResignActiveNotification as a
      // workaround. There're two ways to change airplay output. One way is from
      // system setting, and the other is from control center. Either way would
      // make our app become inactive.
      app_status_observer_ = [[NSNotificationCenter defaultCenter]
          addObserverForName:UIApplicationWillResignActiveNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    this->OnAudioRouteChanged(notification);
                  }];
    }
  }

  ~PlaybackCapabilitiesImpl() {
    @autoreleasepool {
      [[NSNotificationCenter defaultCenter]
          removeObserver:route_change_observer_];
      [[NSNotificationCenter defaultCenter]
          removeObserver:app_status_observer_];
    }
    ObserverRegistry::UnregisterObserver(&observer_);
  }

  bool IsHwVp9Supported() const { return is_hw_vp9_supported_; }

  bool IsAppleTVHD() const { return is_apple_tv_hd_; }

  bool IsAppleTV4K() const { return is_apple_tv_4k_; }

  bool GetAudioConfiguration(size_t index,
                             SbMediaAudioConfiguration* configuration) {
    std::lock_guard scoped_lock(mutex_);
    if (is_audio_configurations_dirty_) {
      LoadAudioConfigurations_Locked();
    }

    if (index >= audio_configurations_.size()) {
      return false;
    }
    *configuration = audio_configurations_[index];
    return true;
  }

  void ReloadAudioConfigurations() { is_audio_configurations_dirty_ = true; }

 private:
  PlaybackCapabilitiesImpl(const PlaybackCapabilitiesImpl&) = delete;
  PlaybackCapabilitiesImpl& operator=(const PlaybackCapabilitiesImpl&) = delete;

  void LoadAudioConfigurations_Locked() {
    audio_configurations_.clear();
    @autoreleasepool {
      NSArray<AVAudioSessionPortDescription*>* audio_outputs =
          [[[AVAudioSession sharedInstance] currentRoute] outputs];
      // The AVAudioSessionPortDescription.channels represents the current in
      // used channels, which is not the max supported channels.
      // [AVAudioSession sharedInstance].maximumOutputNumberOfChannels returns
      // the max supported channels. If there're multiple output devices
      // connected, the platform would mix up/down the audio for us and make
      // the sound right to all audio outputs.
      auto output_channels = std::max(
          2,
          static_cast<int>(
              [AVAudioSession sharedInstance].maximumOutputNumberOfChannels));

      if (audio_outputs.count == 0) {
        // If the platform doesn't return any route, we should still have a
        // default configuration. Otherwise, all audio codecs would be rejected.
        SbMediaAudioConfiguration configuration;
        configuration.connector = kSbMediaAudioConnectorHdmi;
        configuration.latency = 0;
        configuration.coding_type = kSbMediaAudioCodingTypePcm;
        configuration.number_of_channels = output_channels;
        audio_configurations_.push_back(configuration);
      } else {
        for (AVAudioSessionPortDescription* audio_output : audio_outputs) {
          SbMediaAudioConfiguration configuration;
          configuration.connector =
              GetConnectorFromAVAudioSessionPort(audio_output.portType);
          configuration.latency = 0;
          configuration.coding_type = kSbMediaAudioCodingTypePcm;
          configuration.number_of_channels = output_channels;
          audio_configurations_.push_back(configuration);
        }
      }
    }
    is_audio_configurations_dirty_ = false;
  }

  void OnAudioRouteChanged(NSNotification* notification) {
    int32_t lock_slot = ObserverRegistry::LockObserver(&observer_);
    if (lock_slot < 0) {
      // The observer was already unregistered.
      return;
    }

    SB_LOG(INFO) << "AVAudioSessionRoute has changed.";
    ReloadAudioConfigurations();
    ObserverRegistry::UnlockObserver(lock_slot);
  }

  const bool is_hw_vp9_supported_;
  const bool is_apple_tv_hd_;
  const bool is_apple_tv_4k_;

  std::mutex mutex_;
  std::atomic_bool is_audio_configurations_dirty_ = {true};
  ObserverRegistry::Observer observer_;
  NSObject* route_change_observer_ = nullptr;
  NSObject* app_status_observer_ = nullptr;
  std::vector<SbMediaAudioConfiguration> audio_configurations_;
};

}  // namespace

SB_ONCE_INITIALIZE_FUNCTION(PlaybackCapabilitiesImpl, GetInstance)

// static
void PlaybackCapabilities::InitializeInBackground() {
  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
        GetInstance();
      });
}

// static
bool PlaybackCapabilities::IsHwVp9Supported() {
  return GetInstance()->IsHwVp9Supported();
}

// static
bool PlaybackCapabilities::IsAppleTVHD() {
  return GetInstance()->IsAppleTVHD();
}

// static
bool PlaybackCapabilities::IsAppleTV4K() {
  return GetInstance()->IsAppleTV4K();
}

// static
bool PlaybackCapabilities::GetAudioConfiguration(
    size_t index,
    SbMediaAudioConfiguration* configuration) {
  return GetInstance()->GetAudioConfiguration(index, configuration);
}

// static
void PlaybackCapabilities::ReloadAudioConfigurations() {
  return GetInstance()->ReloadAudioConfigurations();
}

}  // namespace starboard
