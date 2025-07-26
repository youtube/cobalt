// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_STUB_LOG_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_STUB_LOG_MANAGER_H_

#include "components/autofill/core/browser/logging/log_manager.h"

namespace autofill {

// Use this in tests only, to provide a no-op implementation of LogManager.
class StubLogManager : public RoutingLogManager {
 public:
  StubLogManager() = default;

  StubLogManager(const StubLogManager&) = delete;
  StubLogManager& operator=(const StubLogManager&) = delete;

  ~StubLogManager() override = default;

 private:
  // LogManager
  void OnLogRouterAvailabilityChanged(bool router_can_be_used) override;
  void SetSuspended(bool suspended) override;
  bool IsLoggingActive() const override;
  LogBufferSubmitter Log() override;
  void ProcessLog(base::Value::Dict node,
                  base::PassKey<LogBufferSubmitter>) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_STUB_LOG_MANAGER_H_
