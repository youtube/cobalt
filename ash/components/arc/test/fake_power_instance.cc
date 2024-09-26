// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_power_instance.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"

namespace arc {

FakePowerInstance::FakePowerInstance() = default;

FakePowerInstance::~FakePowerInstance() = default;

FakePowerInstance::SuspendCallback FakePowerInstance::GetSuspendCallback() {
  return std::move(suspend_callback_);
}

void FakePowerInstance::Init(mojo::PendingRemote<mojom::PowerHost> host_remote,
                             InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

void FakePowerInstance::SetInteractive(bool enabled) {
  interactive_ = enabled;
}

void FakePowerInstance::Suspend(SuspendCallback callback) {
  num_suspend_++;
  suspend_callback_ = std::move(callback);
}

void FakePowerInstance::Resume() {
  num_resume_++;
}

void FakePowerInstance::UpdateScreenBrightnessSettings(double percent) {
  screen_brightness_ = percent;
}

void FakePowerInstance::PowerSupplyInfoChanged() {
  num_power_supply_info_++;
}

void FakePowerInstance::GetWakefulnessMode(
    GetWakefulnessModeCallback callback) {
  std::move(callback).Run(mojom::WakefulnessMode::AWAKE);
}

void FakePowerInstance::OnCpuRestrictionChanged(
    mojom::CpuRestrictionState cpu_restriction_state) {
  last_cpu_restriction_state_ = cpu_restriction_state;
  ++cpu_restriction_state_count_;
}

}  // namespace arc
