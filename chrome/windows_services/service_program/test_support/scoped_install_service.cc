// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/scoped_install_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_service_work_item.h"

ScopedInstallService::ScopedInstallService(std::wstring_view service_name,
                                           std::wstring_view display_name,
                                           std::wstring_view description,
                                           base::CommandLine service_command,
                                           const CLSID& clsid,
                                           const IID& iid) {
  const std::wstring name(service_name);
  const std::wstring display(display_name);
  const std::wstring description_text(description);
  const std::vector<GUID> clsids{clsid};
  const std::vector<GUID> iids{iid};

  // Delete an old instance if one was left behind by a previous crash.
  installer::InstallServiceWorkItem::DeleteService(name, display, clsids, iids);

  static constexpr const char* kSwitchesToCopy[] = {
      switches::kV,
      switches::kVModule,
  };
  service_command.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                   kSwitchesToCopy);
  auto work_item = std::make_unique<installer::InstallServiceWorkItem>(
      name, display, description_text, SERVICE_DEMAND_START,
      std::move(service_command),
      base::CommandLine(base::CommandLine::NO_PROGRAM),
      install_static::GetClientStateKeyPath(), clsids, iids);
  if (work_item->Do()) {
    work_item_ = std::move(work_item);
  }
}

ScopedInstallService::~ScopedInstallService() {
  if (work_item_) {
    work_item_->Rollback();
  }
}
