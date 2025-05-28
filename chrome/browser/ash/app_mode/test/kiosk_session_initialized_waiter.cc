// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/test/kiosk_session_initialized_waiter.h"

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"

namespace ash {

KioskSessionInitializedWaiter::KioskSessionInitializedWaiter() {
  scoped_observations_.AddObservation(KioskChromeAppManager::Get());
  scoped_observations_.AddObservation(WebKioskAppManager::Get());
  scoped_observations_.AddObservation(KioskIwaManager::Get());
}

KioskSessionInitializedWaiter::~KioskSessionInitializedWaiter() = default;

bool KioskSessionInitializedWaiter::Wait() {
  if (KioskController::Get().GetKioskSystemSession() != nullptr) {
    // Kiosk session already initialized, nothing to wait for.
    return true;
  }
  return initialized_future_.Wait();
}

void KioskSessionInitializedWaiter::OnKioskSessionInitialized() {
  initialized_future_.SetValue();
}

}  // namespace ash
