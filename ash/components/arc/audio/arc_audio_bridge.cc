// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/audio/arc_audio_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/public/cpp/system_tray.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/components/audio/audio_device.h"

namespace arc {
namespace {

using ::ash::AudioDevice;
using ::ash::AudioDeviceType;

// Singleton factory for ArcAccessibilityHelperBridge.
class ArcAudioBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAudioBridge,
          ArcAudioBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAudioBridgeFactory";

  static ArcAudioBridgeFactory* GetInstance() {
    return base::Singleton<ArcAudioBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcAudioBridgeFactory>;
  ArcAudioBridgeFactory() = default;
  ~ArcAudioBridgeFactory() override = default;
};

}  // namespace

// static
ArcAudioBridge* ArcAudioBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAudioBridgeFactory::GetForBrowserContext(context);
}

// static
ArcAudioBridge* ArcAudioBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcAudioBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcAudioBridge::ArcAudioBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      cras_audio_handler_(ash::CrasAudioHandler::Get()) {
  arc_bridge_service_->audio()->SetHost(this);
  arc_bridge_service_->audio()->AddObserver(this);
  DCHECK(cras_audio_handler_);
  cras_audio_handler_->AddAudioObserver(this);
}

ArcAudioBridge::~ArcAudioBridge() {
  if (ash::CrasAudioHandler::Get())  // for unittests
    cras_audio_handler_->RemoveAudioObserver(this);
  arc_bridge_service_->audio()->RemoveObserver(this);
  arc_bridge_service_->audio()->SetHost(nullptr);
}

void ArcAudioBridge::OnConnectionReady() {
  // TODO(hidehiko): Replace with ConnectionHolder::IsConnected().
  available_ = true;
}

void ArcAudioBridge::OnConnectionClosed() {
  available_ = false;
}

void ArcAudioBridge::ShowVolumeControls() {
  DVLOG(2) << "ArcAudioBridge::ShowVolumeControls";
  ash::SystemTray::Get()->ShowVolumeSliderBubble();
}

void ArcAudioBridge::OnSystemVolumeUpdateRequest(int32_t percent) {
  if (percent < 0 || percent > 100)
    return;
  cras_audio_handler_->SetOutputVolumePercent(percent);
  bool is_muted =
      percent <= cras_audio_handler_->GetOutputDefaultVolumeMuteThreshold();
  if (cras_audio_handler_->IsOutputMuted() != is_muted)
    cras_audio_handler_->SetOutputMute(is_muted);
}

void ArcAudioBridge::OnAudioNodesChanged() {
  uint64_t output_id = cras_audio_handler_->GetPrimaryActiveOutputNode();
  const ash::AudioDevice* output_device =
      cras_audio_handler_->GetDeviceFromId(output_id);
  bool headphone_inserted =
      (output_device && (output_device->type == AudioDeviceType::kHeadphone ||
                         output_device->type == AudioDeviceType::kUsb ||
                         output_device->type == AudioDeviceType::kLineout));

  uint64_t input_id = cras_audio_handler_->GetPrimaryActiveInputNode();
  const ash::AudioDevice* input_device =
      cras_audio_handler_->GetDeviceFromId(input_id);
  bool microphone_inserted =
      (input_device && (input_device->type == AudioDeviceType::kMic ||
                        input_device->type == AudioDeviceType::kUsb));

  DVLOG(1) << "HEADPHONE " << headphone_inserted << " MICROPHONE "
           << microphone_inserted;
  SendSwitchState(headphone_inserted, microphone_inserted);
}

void ArcAudioBridge::OnOutputNodeVolumeChanged(uint64_t node_id, int volume) {
  DVLOG(1) << "Output node " << node_id << " volume " << volume;
  volume_ = volume;
  SendVolumeState();
}

void ArcAudioBridge::OnOutputMuteChanged(bool mute_on) {
  DVLOG(1) << "Output mute " << mute_on;
  muted_ = mute_on;
  SendVolumeState();
}

void ArcAudioBridge::SendSwitchState(bool headphone_inserted,
                                     bool microphone_inserted) {
  uint32_t switch_state = 0;
  if (headphone_inserted) {
    switch_state |=
        (1 << static_cast<uint32_t>(mojom::AudioSwitch::SW_HEADPHONE_INSERT));
  }
  if (microphone_inserted) {
    switch_state |=
        (1 << static_cast<uint32_t>(mojom::AudioSwitch::SW_MICROPHONE_INSERT));
  }

  DVLOG(1) << "Send switch state " << switch_state;
  if (!available_)
    return;
  mojom::AudioInstance* audio_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->audio(), NotifySwitchState);
  if (audio_instance)
    audio_instance->NotifySwitchState(switch_state);
}

void ArcAudioBridge::SendVolumeState() {
  DVLOG(1) << "Send volume " << volume_ << " muted " << muted_;
  if (!available_)
    return;
  mojom::AudioInstance* audio_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->audio(), NotifyVolumeState);
  if (audio_instance)
    audio_instance->NotifyVolumeState(volume_, muted_);
}

// static
void ArcAudioBridge::EnsureFactoryBuilt() {
  ArcAudioBridgeFactory::GetInstance();
}

}  // namespace arc
