// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/remote_diagnostic_routines_service_strategy.h"

#include <memory>

#include "base/notreached.h"
#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"

namespace chromeos {

namespace {

class RemoteDiagnosticRoutineServiceStrategyAsh
    : public RemoteDiagnosticRoutineServiceStrategy {
 public:
  RemoteDiagnosticRoutineServiceStrategyAsh() = default;
  ~RemoteDiagnosticRoutineServiceStrategyAsh() override = default;

  // `RemoteDiagnosticRoutineServiceStrategy`:
  ash::TelemetryDiagnosticsRoutineServiceAsh& GetService() override {
    return diagnostic_routines_service_;
  }

 private:
  ash::TelemetryDiagnosticsRoutineServiceAsh diagnostic_routines_service_;
};

}  // namespace

// static
std::unique_ptr<RemoteDiagnosticRoutineServiceStrategy>
RemoteDiagnosticRoutineServiceStrategy::Create() {
  return std::make_unique<RemoteDiagnosticRoutineServiceStrategyAsh>();
}

RemoteDiagnosticRoutineServiceStrategy::
    RemoteDiagnosticRoutineServiceStrategy() = default;
RemoteDiagnosticRoutineServiceStrategy::
    ~RemoteDiagnosticRoutineServiceStrategy() = default;

}  // namespace chromeos
