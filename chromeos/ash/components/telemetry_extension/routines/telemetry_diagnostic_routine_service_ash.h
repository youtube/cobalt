// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/telemetry_extension/common/self_owned_mojo_proxy.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

// Implementation of the `TelemetryDiagnosticRoutinesService`, allows for
// creating new routines on the platform as well as interaction with existing
// routines and requesting information about the `SupportStatus` of routines.
class TelemetryDiagnosticsRoutineServiceAsh {
 public:
  TelemetryDiagnosticsRoutineServiceAsh();
  TelemetryDiagnosticsRoutineServiceAsh(
      const TelemetryDiagnosticsRoutineServiceAsh&) = delete;
  TelemetryDiagnosticsRoutineServiceAsh& operator=(
      const TelemetryDiagnosticsRoutineServiceAsh&) = delete;
  ~TelemetryDiagnosticsRoutineServiceAsh();

  // Requests that a routine using the RoutineControl API is created on the
  // platform. This function creates a different routine based on the
  // RoutineArgument supplied.
  //
  // Error Handling:
  // This method will result in the creation of the routine on the device, which
  // might require allocation of additional resources and checking preconditions
  // for the routine, e.g. available hardware, correct arguments, etc.
  //
  // All exceptions that occur (either during initialization or while executing
  // the routine) will close the connection to the provided
  // TelemetryDiagnosticRoutineControl with a specific reason (see
  // crosapi.mojom.TelemetryExtensionException) and a string message containing
  // human readable information about the exception.
  // For that reason it is necessary to always setup a disconnect handler on the
  // TelemetryDiagnosticRoutineControl remote to be informed about potential
  // exceptions.
  //
  // Please note exceptions are different from a routine reporting `has_passed
  // == false` (in case it failed, see TelemetryDiagnosticRoutineStateFinished).
  // Exception are something not intended to happen. The details of the reasons
  // for Exceptions can be found in crosapi.mojom.TelemetryExtensionException
  // type and the corresponding reason enum.
  //
  // To know if an exception occurs during the initialization, callers can wait
  // for the routine being initialized (get via `GetState` or
  // TelemetryDiagnosticRoutineObserver) on the
  // TelemetryDiagnosticRoutineControl remote, before calling the `Start`
  // method.
  //
  // The request:
  // * |routine_argument| - a RoutineArgument type that provides all the
  //                        necessary parameters and configs to create a
  //                        particular type of routine.
  // * |routine_receiver| - a receiver that will be bound to the actual routine
  //                        control implementation, where the remote will be
  //                        held by the client for starting the routine.
  //
  // * |routine_observer| - an optional observer to receive status updates about
  //                        changing routine states.
  void CreateRoutine(
      crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutineControl>
          routine_receiver,
      mojo::PendingRemote<ash::cros_healthd::mojom::RoutineObserver> observer);

 private:
  // Called when a routine controller connection is closed. This removes the
  // controller from our list.
  void OnConnectionClosed(
      base::WeakPtr<SelfOwnedMojoProxyInterface> closed_connection);

  // The routine controls created for each running routine.
  std::vector<base::WeakPtr<SelfOwnedMojoProxyInterface>> routine_controls_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_
