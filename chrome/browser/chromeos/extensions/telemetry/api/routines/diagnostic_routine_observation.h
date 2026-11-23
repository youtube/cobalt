// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_OBSERVATION_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_OBSERVATION_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_info.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class DiagnosticRoutineObservation
    : public ash::cros_healthd::mojom::RoutineObserver {
 public:
  using OnRoutineFinished = base::OnceCallback<void(DiagnosticRoutineInfo)>;

  explicit DiagnosticRoutineObservation(
      DiagnosticRoutineInfo info,
      OnRoutineFinished on_routine_finished,
      mojo::PendingReceiver<ash::cros_healthd::mojom::RoutineObserver>
          pending_receiver);

  DiagnosticRoutineObservation(const DiagnosticRoutineObservation&) = delete;
  DiagnosticRoutineObservation& operator=(const DiagnosticRoutineObservation&) =
      delete;

  ~DiagnosticRoutineObservation() override;

  // `TelemetryDiagnosticRoutineObserver`:
  void OnRoutineStateChange(
      ash::cros_healthd::mojom::RoutineStatePtr state) override;

 private:
  DiagnosticRoutineInfo info_;
  OnRoutineFinished on_routine_finished_;
  mojo::Receiver<ash::cros_healthd::mojom::RoutineObserver> receiver_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_OBSERVATION_H_
