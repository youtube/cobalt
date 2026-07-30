// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_REMOTE_DIAGNOSTIC_ROUTINES_SERVICE_STRATEGY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_REMOTE_DIAGNOSTIC_ROUTINES_SERVICE_STRATEGY_H_

#include <memory>

namespace ash {
class TelemetryDiagnosticsRoutineServiceAsh;
}  // namespace ash

namespace chromeos {

// An interface for accessing a routines service mojo remote. Allows for
// multiple implementations depending on whether this is running in Ash or
// Lacros.
class RemoteDiagnosticRoutineServiceStrategy {
 public:
  static std::unique_ptr<RemoteDiagnosticRoutineServiceStrategy> Create();

  RemoteDiagnosticRoutineServiceStrategy(
      const RemoteDiagnosticRoutineServiceStrategy&) = delete;
  RemoteDiagnosticRoutineServiceStrategy& operator=(
      const RemoteDiagnosticRoutineServiceStrategy&) = delete;
  virtual ~RemoteDiagnosticRoutineServiceStrategy();

  virtual ash::TelemetryDiagnosticsRoutineServiceAsh& GetService() = 0;

 protected:
  RemoteDiagnosticRoutineServiceStrategy();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_REMOTE_DIAGNOSTIC_ROUTINES_SERVICE_STRATEGY_H_
