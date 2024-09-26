// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/geolocation/system_geolocation_source_lacros.h"

#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chromeos/lacros/lacros_service.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"

SystemGeolocationSourceLacros::SystemGeolocationSourceLacros()
    : permission_update_callback_(base::DoNothing()) {
  // binding to remote
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(WARNING) << "crosapi: Prefs API not available";
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->AddObserver(
      crosapi::mojom::PrefPath::kGeolocationAllowed,
      receiver_.BindNewPipeAndPassRemoteWithVersion());
  CHECK(receiver_.is_bound());
}

SystemGeolocationSourceLacros::~SystemGeolocationSourceLacros() = default;

// static
std::unique_ptr<device::GeolocationManager>
SystemGeolocationSourceLacros::CreateGeolocationManagerOnLacros() {
  return std::make_unique<device::GeolocationManager>(
      std::make_unique<SystemGeolocationSourceLacros>());
}

void SystemGeolocationSourceLacros::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_update_callback_ = std::move(callback);
  if (current_status_ ==
      device::LocationSystemPermissionStatus::kNotDetermined) {
    // This is here to support older versions of Ash that do not send the system
    // geolocation switch via crosapi.
    // The original behavior before the system wide switch was introduced was to
    // allow, so we keep this as the default behavior when the system doesn't
    // indicate othervise.
    // TODO(272426671): clean this up when we can safely assume that Ash
    // provides the value.
    permission_update_callback_.Run(
        device::LocationSystemPermissionStatus::kAllowed);
    return;
  }
  // If available, pass the (up-to-date) status into the new callback
  permission_update_callback_.Run(current_status_);
}

void SystemGeolocationSourceLacros::OnPrefChanged(base::Value value) {
  const bool value_is_bool = value.is_bool();
  LOG_IF(ERROR, !value_is_bool)
      << "GeolocationSourceLacros received a non-bool value";
  if (value_is_bool) {
    current_status_ = value.GetBool()
                          ? device::LocationSystemPermissionStatus::kAllowed
                          : device::LocationSystemPermissionStatus::kDenied;

    permission_update_callback_.Run(current_status_);
  }
}
