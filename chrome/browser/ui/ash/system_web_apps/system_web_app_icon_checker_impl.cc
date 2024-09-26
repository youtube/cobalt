// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_web_apps/system_web_app_icon_checker_impl.h"

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_icon_checker.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/diagnostics/web_app_icon_diagnostic.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
// static
std::unique_ptr<SystemWebAppIconChecker> SystemWebAppIconChecker::Create(
    Profile* profile) {
  return std::make_unique<SystemWebAppIconCheckerImpl>(profile);
}

SystemWebAppIconCheckerImpl::SystemWebAppIconCheckerImpl(Profile* profile)
    : profile_(profile) {}

SystemWebAppIconCheckerImpl::~SystemWebAppIconCheckerImpl() = default;

void SystemWebAppIconCheckerImpl::StartCheck(
    const std::vector<web_app::AppId>& app_ids,
    base::OnceCallback<void(SystemWebAppIconChecker::IconState)> callback) {
  DCHECK(checkers_.empty()) << "StartCheck() shouldn't be called again before "
                               "the current check completes.";
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(SystemWebAppManager::Get(profile_));
  DCHECK(
      SystemWebAppManager::Get(profile_)->on_apps_synchronized().is_signaled())
      << "SystemWebAppIconChecker should only be called after system web apps "
         "finish installing.";

  auto barrier = base::BarrierCallback<
      absl::optional<web_app::WebAppIconDiagnostic::Result>>(
      app_ids.size(),
      base::BindOnce(&SystemWebAppIconCheckerImpl::OnChecksDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  checkers_.reserve(app_ids.size());

  for (const auto& app_id : app_ids) {
    DCHECK(GetSystemWebAppTypeForAppId(profile_, app_id));

    checkers_
        .emplace_back(
            std::make_unique<web_app::WebAppIconDiagnostic>(profile_, app_id))
        ->Run(barrier);
  }
}

void SystemWebAppIconCheckerImpl::OnChecksDone(
    base::OnceCallback<void(IconState)> callback,
    std::vector<absl::optional<web_app::WebAppIconDiagnostic::Result>>
        results) {
  checkers_.clear();

  if (results.size() == 0) {
    std::move(callback).Run(IconState::kNoAppInstalled);
    return;
  }

  for (const auto& result : results) {
    if (result->IsAnyFallbackUsed()) {
      std::move(callback).Run(IconState::kBroken);
      return;
    }
  }

  std::move(callback).Run(IconState::kOk);
}

void SystemWebAppIconCheckerImpl::StopCheck() {
  checkers_.clear();
}

}  // namespace ash
