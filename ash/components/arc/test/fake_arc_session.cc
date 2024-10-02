// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_arc_session.h"

#include <memory>

#include "base/check.h"

namespace arc {

FakeArcSession::FakeArcSession() = default;

FakeArcSession::~FakeArcSession() = default;

void FakeArcSession::StartMiniInstance() {}

void FakeArcSession::RequestUpgrade(UpgradeParams params) {
  upgrade_requested_ = true;
  upgrade_locale_param_ = params.locale;
  if (boot_failure_emulation_enabled_) {
    for (auto& observer : observer_list_)
      observer.OnSessionStopped(boot_failure_reason_, false, true);
  } else if (!boot_suspended_) {
    running_ = true;
  }
}

void FakeArcSession::Stop() {
  stop_requested_ = true;
  StopWithReason(ArcStopReason::SHUTDOWN);
}

bool FakeArcSession::IsStopRequested() {
  return stop_requested_;
}

void FakeArcSession::OnShutdown() {
  StopWithReason(ArcStopReason::SHUTDOWN);
}

void FakeArcSession::SetUserInfo(
    const cryptohome::Identification& cryptohome_id,
    const std::string& hash,
    const std::string& serial_number) {}

void FakeArcSession::SetDemoModeDelegate(
    ArcClientAdapter::DemoModeDelegate* delegate) {}

void FakeArcSession::TrimVmMemory(TrimVmMemoryCallback callback,
                                  int page_limit) {
  ++trim_vm_memory_count_;
  last_trim_vm_page_limit_ = page_limit;
  std::move(callback).Run(trim_success_, trim_fail_reason);
}

void FakeArcSession::SetDefaultDeviceScaleFactor(float scale_factor) {}

void FakeArcSession::SetUseVirtioBlkData(bool use_virtio_blk_data) {}

void FakeArcSession::StopWithReason(ArcStopReason reason) {
  bool was_mojo_connected = running_;
  running_ = false;
  for (auto& observer : observer_list_)
    observer.OnSessionStopped(reason, was_mojo_connected, upgrade_requested_);
}

void FakeArcSession::EnableBootFailureEmulation(ArcStopReason reason) {
  DCHECK(!boot_failure_emulation_enabled_);
  DCHECK(!boot_suspended_);

  boot_failure_emulation_enabled_ = true;
  boot_failure_reason_ = reason;
}

void FakeArcSession::SuspendBoot() {
  DCHECK(!boot_failure_emulation_enabled_);
  DCHECK(!boot_suspended_);

  boot_suspended_ = true;
}

// TODO(cmtm): Change this function so that it emulates both mini-container
// startup and upgrade. With this change, we should also be able to get rid of
// SuspendBoot since RequestUpgrade will no longer notify observers.
void FakeArcSession::EmulateMiniContainerStart() {
  if (boot_failure_emulation_enabled_) {
    for (auto& observer : observer_list_)
      observer.OnSessionStopped(boot_failure_reason_, false,
                                upgrade_requested_);
  }
}

// static
std::unique_ptr<ArcSession> FakeArcSession::Create() {
  return std::make_unique<FakeArcSession>();
}

}  // namespace arc
