// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/browser_management_service.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_status_provider.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image.h"

namespace policy {

namespace {

std::vector<std::unique_ptr<ManagementStatusProvider>>
GetManagementStatusProviders(Profile* profile) {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(
      std::make_unique<BrowserCloudManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalDomainBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<ProfileCloudManagementStatusProvider>(profile));
  providers.emplace_back(
      std::make_unique<LocalTestPolicyUserManagementProvider>(profile));
  providers.emplace_back(
      std::make_unique<LocalTestPolicyBrowserManagementProvider>(profile));
#if BUILDFLAG(IS_CHROMEOS)
  providers.emplace_back(std::make_unique<DeviceManagementStatusProvider>());
#endif
  return providers;
}

}  // namespace

BrowserManagementService::BrowserManagementService(Profile* profile)
    : ManagementService(GetManagementStatusProviders(profile)) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserManagementService::UpdateManagementIconForProfile,
                     weak_ptr_factory_.GetWeakPtr(), profile));
  UpdateEnterpriseLabelForProfile(profile);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kEnterpriseLogoUrlForProfile,
      base::BindRepeating(
          &BrowserManagementService::UpdateManagementIconForProfile,
          weak_ptr_factory_.GetWeakPtr(), profile));
  pref_change_registrar_.Add(
      prefs::kEnterpriseCustomLabelForProfile,
      base::BindRepeating(
          &BrowserManagementService::UpdateEnterpriseLabelForProfile,
          weak_ptr_factory_.GetWeakPtr(), profile));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

ui::ImageModel* BrowserManagementService::GetManagementIconForProfile() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return management_icon_for_profile_.IsEmpty() ? nullptr
                                                : &management_icon_for_profile_;
#else
  return nullptr;
#endif
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void BrowserManagementService::UpdateManagementIconForProfile(
    Profile* profile) {
  enterprise_util::GetManagementIcon(
      GURL(profile->GetPrefs()->GetString(prefs::kEnterpriseLogoUrlForProfile)),
      profile,
      base::BindOnce(&BrowserManagementService::SetManagementIconForProfile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserManagementService::UpdateEnterpriseLabelForProfile(
    Profile* profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // profile_manager might be null in testing environments.
  if (!profile_manager) {
    return;
  }

  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    return;
  }

  entry->SetEnterpriseProfileLabel(
      enterprise_util::GetEnterpriseLabel(profile));
}

void BrowserManagementService::SetManagementIconForProfile(
    const gfx::Image& management_icon) {
  management_icon_for_profile_ = ui::ImageModel::FromImage(management_icon);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BrowserManagementService::~BrowserManagementService() = default;

}  // namespace policy
