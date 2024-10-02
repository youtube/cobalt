// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_service.h"

#include <memory>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_service_factory.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

constexpr int kNonComplianceReasonAppNotInstalled = 5;

}  // namespace

// Timeout maintenance session after 30 minutes.
constexpr base::TimeDelta kArcKioskMaintenanceSessionTimeout =
    base::Minutes(30);

// static
ArcKioskAppService* ArcKioskAppService::Create(Profile* profile) {
  return new ArcKioskAppService(profile);
}

// static
ArcKioskAppService* ArcKioskAppService::Get(content::BrowserContext* context) {
  return ArcKioskAppServiceFactory::GetForBrowserContext(context);
}

void ArcKioskAppService::SetNetworkDelegate(NetworkDelegate* network_delegate) {
  delegate_ = network_delegate;
}

void ArcKioskAppService::AddObserver(KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcKioskAppService::RemoveObserver(KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcKioskAppService::Shutdown() {
  ArcAppListPrefs::Get(profile_)->RemoveObserver(this);
  // It can be null unittests.
  if (arc::ArcSessionManager::Get()) {
    arc::ArcSessionManager::Get()->RemoveObserver(this);
  }
  app_manager_->RemoveObserver(this);
  arc::ArcPolicyBridge::GetForBrowserContext(profile_)->RemoveObserver(this);
}

void ArcKioskAppService::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_id_.empty() && app_id != app_id_) {
    return;
  }
  PreconditionsChanged();
}

void ArcKioskAppService::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_id_.empty() && app_id != app_id_) {
    return;
  }
  PreconditionsChanged();
}

void ArcKioskAppService::OnPackageListInitialRefreshed() {
  // The app could already be registered.
  PreconditionsChanged();
}

void ArcKioskAppService::OnKioskAppsSettingsChanged() {
  PreconditionsChanged();
}

void ArcKioskAppService::OnTaskCreated(int32_t task_id,
                                       const std::string& package_name,
                                       const std::string& activity,
                                       const std::string& intent,
                                       int32_t session_id) {
  // Store task id of the app to stop it later when needed.
  if (app_info_ && package_name == app_info_->package_name &&
      activity == app_info_->activity) {
    task_id_ = task_id;
    observers_.NotifyAppLaunched();
  }
}

void ArcKioskAppService::OnTaskDestroyed(int32_t task_id) {
  if (task_id == task_id_) {
    ResetAppLauncher();
    // Trying to restart app if it was somehow closed or crashed
    // as kiosk app should always be running during the session.
    PreconditionsChanged();
  }
}

void ArcKioskAppService::OnMaintenanceSessionCreated() {
  maintenance_session_running_ = true;
  PreconditionsChanged();
  // Safe to bind |this| as timer is auto-cancelled on destruction.
  maintenance_timeout_timer_.Start(
      FROM_HERE, kArcKioskMaintenanceSessionTimeout,
      base::BindOnce(&ArcKioskAppService::OnMaintenanceSessionFinished,
                     base::Unretained(this)));
}

void ArcKioskAppService::OnMaintenanceSessionFinished() {
  if (!maintenance_timeout_timer_.IsRunning()) {
    VLOG(1) << "Maintenance session timeout";
  }
  maintenance_timeout_timer_.Stop();
  maintenance_session_running_ = false;
  PreconditionsChanged();
}

void ArcKioskAppService::OnAppWindowLaunched() {
  observers_.NotifyAppWindowCreated();
}

void ArcKioskAppService::OnIconUpdated(ArcAppIcon* icon) {
  DCHECK_EQ(icon, app_icon_.get());
  if (icon->image_skia().isNull()) {
    app_icon_.release();
    return;
  }
  AccountId account_id = multi_user_util::GetAccountIdFromProfile(profile_);
  app_manager_->UpdateNameAndIcon(account_id, app_info_->name,
                                  app_icon_->image_skia());
  observers_.NotifyAppDataUpdated();
}

void ArcKioskAppService::OnArcSessionRestarting() {
  // Reset state as the app is for sure not running.
  VLOG(2) << "Clearing ARC Kiosk state on restart";
  ResetAppLauncher();
}

void ArcKioskAppService::OnArcSessionStopped(arc::ArcStopReason reason) {
  // Reset state as the app is for sure not running.
  VLOG(2) << "Clearing ARC Kiosk state on stop";
  ResetAppLauncher();
}

void ArcKioskAppService::OnComplianceReportReceived(
    const base::Value* compliance_report) {
  VLOG(2) << "Compliance report received";
  compliance_report_received_ = true;
  pending_policy_app_installs_.clear();
  const base::Value::List* const details =
      compliance_report->GetDict().FindList("nonComplianceDetails");
  if (!details) {
    PreconditionsChanged();
    return;
  }

  for (const auto& detail : *details) {
    const base::Value::Dict& detail_dict = detail.GetDict();
    absl::optional<int> reason = detail_dict.FindInt("nonComplianceReason");
    if (!reason || *reason != kNonComplianceReasonAppNotInstalled) {
      continue;
    }
    const std::string* const app_name = detail_dict.FindString("packageName");
    if (!app_name || app_name->empty()) {
      continue;
    }
    pending_policy_app_installs_.insert(*app_name);
  }
  PreconditionsChanged();
}

ArcKioskAppService::ArcKioskAppService(Profile* profile) : profile_(profile) {
  ArcAppListPrefs::Get(profile_)->AddObserver(this);
  arc::ArcSessionManager::Get()->AddObserver(this);
  app_manager_ = ArcKioskAppManager::Get();
  DCHECK(app_manager_);
  app_manager_->AddObserver(this);
  arc::ArcPolicyBridge::GetForBrowserContext(profile_)->AddObserver(this);
  PreconditionsChanged();
}

ArcKioskAppService::~ArcKioskAppService() {
  maintenance_timeout_timer_.Stop();
}

void ArcKioskAppService::RequestNameAndIconUpdate() {
  // Request only once when app_icon_ is not initialized.
  if (!app_info_ || !app_info_->ready || app_icon_) {
    return;
  }
  app_icon_ = std::make_unique<ArcAppIcon>(
      profile_, app_id_,
      SharedAppListConfig::instance().default_grid_icon_dimension(), this);
  app_icon_->image_skia().GetRepresentation(ui::GetSupportedResourceScaleFactor(
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor()));
  // Apply default image now and in case icon is updated then OnIconUpdated()
  // will be called additionally.
  OnIconUpdated(app_icon_.get());
}

void ArcKioskAppService::PreconditionsChanged() {
  VLOG(2) << "Preconditions for kiosk app changed";
  app_id_ = GetAppId();
  if (app_id_.empty()) {
    VLOG(2) << "Kiosk app is not available";
    return;
  }
  app_info_ = ArcAppListPrefs::Get(profile_)->GetApp(app_id_);
  VLOG_IF(2, app_info_ && app_info_->ready) << "Kiosk app is ready";
  VLOG(2) << "Maintenance session is "
          << (maintenance_session_running_ ? "running" : "not running");
  VLOG(2) << "Policy compliance is "
          << (compliance_report_received_ ? "reported" : "not yet reported");
  VLOG(2) << "Kiosk app with id: " << app_id_ << " is "
          << (app_launcher_ ? "already launched" : "not yet launched");
  VLOG(2) << "Kiosk app is policy "
          << (pending_policy_app_installs_.count(app_info_->package_name)
                  ? "non-compliant"
                  : "compliant");
  RequestNameAndIconUpdate();
  if (app_info_ && app_info_->ready && !maintenance_session_running_ &&
      compliance_report_received_ &&
      pending_policy_app_installs_.count(app_info_->package_name) == 0) {
    if (!app_launcher_) {
      VLOG(2) << "Starting kiosk app";
      app_launcher_ = std::make_unique<ArcKioskAppLauncher>(
          profile_, ArcAppListPrefs::Get(profile_), app_id_, this);
      observers_.NotifyAppPrepared();
    }
  } else if (task_id_ != -1) {
    VLOG(2) << "Kiosk app should be closed";
    arc::CloseTask(task_id_);
  }
}

std::string ArcKioskAppService::GetAppId() {
  AccountId account_id = multi_user_util::GetAccountIdFromProfile(profile_);
  const ArcKioskAppData* app = app_manager_->GetAppByAccountId(account_id);
  if (!app) {
    return std::string();
  }
  std::unordered_set<std::string> app_ids =
      ArcAppListPrefs::Get(profile_)->GetAppsForPackage(app->package_name());
  if (app_ids.empty()) {
    return std::string();
  }
  // If |activity| and |intent| are not specified, return any app from the
  // package.
  if (app->activity().empty() && app->intent().empty()) {
    return *app_ids.begin();
  }
  // Check that the app is registered for given package.
  return app_ids.count(app->app_id()) ? app->app_id() : std::string();
}

void ArcKioskAppService::ResetAppLauncher() {
  app_launcher_.reset();
  task_id_ = -1;
}

// ArcKioskAppService manages his own state by itself.
void ArcKioskAppService::Initialize() {}
void ArcKioskAppService::ContinueWithNetworkReady() {}
void ArcKioskAppService::LaunchApp() {}

}  // namespace ash
