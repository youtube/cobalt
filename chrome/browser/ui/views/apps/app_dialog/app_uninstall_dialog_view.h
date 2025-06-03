// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace views {
class Checkbox;
}  // namespace views

namespace gfx {
class ImageSkia;
}

struct SubApp;

// This class generates the unified uninstall dialog based on the app type. Once
// the user has confirmed the uninstall, this class calls the parent class
// apps::UninstallDialog::UiBase to notify AppService, which transfers control
// to the publisher to uninstall the app.
class AppUninstallDialogView : public ProfileObserver,
                               public apps::UninstallDialog::UiBase,
                               public AppDialogView {
  using GetSubAppsCallback = base::OnceCallback<void(std::vector<SubApp>)>;
  using UninstallDialogReadyCallback =
      base::OnceCallback<void(AppUninstallDialogView*)>;

 public:
  AppUninstallDialogView(Profile* profile,
                         apps::AppType app_type,
                         const std::string& app_id,
                         const std::string& app_name,
                         gfx::ImageSkia image,
                         apps::UninstallDialog* uninstall_dialog,
                         UninstallDialogReadyCallback callback);

  AppUninstallDialogView(const AppUninstallDialogView&) = delete;
  AppUninstallDialogView& operator=(const AppUninstallDialogView&) = delete;

  ~AppUninstallDialogView() override;

  static AppUninstallDialogView* GetActiveViewForTesting();

  // IDs that identify a view within the app uninstall dialog view.
  // Used to validate views in browsertests.
  enum class DialogViewID : int {
    VIEW_ID_NONE = 0,
    SUB_APP_LABEL,
  };

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void InitializeView(Profile* profile,
                      apps::AppType app_type,
                      const std::string& app_id);

  void InitializeCheckbox(const GURL& app_start_url);
  void InitializeSubAppList(const std::string& short_app_name,
                            const std::vector<SubApp>& sub_apps);

  void InitializeViewForExtension(Profile* profile, const std::string& app_id);
  void InitializeViewForWebApp(const std::string& app_id,
                               std::vector<SubApp> sub_apps);

#if BUILDFLAG(IS_CHROMEOS)
  void LoadSubAppIds(const std::string& parent_app_id,
                     GetSubAppsCallback callback);
  void GetSubAppsInfo(GetSubAppsCallback callback,
                      const std::vector<std::string>& sub_app_ids);
  void CheckForSubAppsThenInitializeViewForWebApp(const std::string& app_id);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InitializeViewForArcApp(Profile* profile, const std::string& app_id);
  void InitializeViewWithMessage(const std::u16string& message);
#endif

  void OnDialogCancelled();
  void OnDialogAccepted();

  void OnWidgetInitialized() override;

  bool async_ = false;

  UninstallDialogReadyCallback uninstall_dialog_ready_callback_;

  raw_ptr<Profile> profile_;

  raw_ptr<views::Checkbox> report_abuse_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> clear_site_data_checkbox_ = nullptr;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<AppUninstallDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_
