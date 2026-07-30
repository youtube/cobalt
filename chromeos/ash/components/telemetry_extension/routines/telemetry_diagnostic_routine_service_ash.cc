// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/telemetry_extension/common/self_owned_mojo_proxy.h"
#include "chromeos/ash/components/telemetry_extension/common/telemetry_extension_converters.h"
#include "chromeos/ash/components/telemetry_extension/routines/routine_control.h"
#include "chromeos/ash/components/telemetry_extension/routines/routine_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

namespace {

namespace crosapi = crosapi::mojom;
namespace healthd = cros_healthd::mojom;

using RoutineControlProxy =
    SelfOwnedMojoProxy<healthd::RoutineControl,
                       crosapi::TelemetryDiagnosticRoutineControl,
                       CrosHealthdRoutineControl>;

}  // namespace

TelemetryDiagnosticsRoutineServiceAsh::TelemetryDiagnosticsRoutineServiceAsh() =
    default;

TelemetryDiagnosticsRoutineServiceAsh::
    ~TelemetryDiagnosticsRoutineServiceAsh() {
  for (auto&& proxy : routine_controls_) {
    if (proxy) {
      proxy->OnServiceDestroyed();
    }
  }
  routine_controls_.clear();
}

void TelemetryDiagnosticsRoutineServiceAsh::CreateRoutine(
    crosapi::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutineControl>
        routine_receiver,
    mojo::PendingRemote<healthd::RoutineObserver> observer) {
  // Setup the RoutineControl.
  mojo::PendingRemote<healthd::RoutineControl> cros_healthd_remote;
  auto cros_healthd_receiver =
      cros_healthd_remote.InitWithNewPipeAndPassReceiver();

  // SAFETY: We can use `base::Unretained` here since we signal the
  // `SelfOwnedMojoProxy` in the destructor.
  auto control_delete_cb =
      base::BindOnce(&TelemetryDiagnosticsRoutineServiceAsh::OnConnectionClosed,
                     base::Unretained(this));
  auto routine_control = RoutineControlProxy::Create(
      std::move(routine_receiver), std::move(cros_healthd_remote),
      std::move(control_delete_cb));
  routine_controls_.push_back(std::move(routine_control));

  // Register the two objects with cros_healthd.
  cros_healthd::ServiceConnection::GetInstance()
      ->GetRoutinesService()
      ->CreateRoutine(
          converters::ConvertRoutinePtr(std::move(routine_argument)),
          std::move(cros_healthd_receiver), std::move(observer));
}

void TelemetryDiagnosticsRoutineServiceAsh::OnConnectionClosed(
    base::WeakPtr<SelfOwnedMojoProxyInterface> closed_connection) {
  std::erase_if(routine_controls_, [&closed_connection](const auto& ptr) {
    return ptr.get() == closed_connection.get();
  });
}

}  // namespace ash
