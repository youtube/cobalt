// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_KERBEROS_KERBEROS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_KERBEROS_KERBEROS_SECTION_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Kerberos settings. Search tags are
// only added if the feature is enabled by policy.
class KerberosSection : public OsSettingsSection,
                        public KerberosCredentialsManager::Observer {
 public:
  KerberosSection(Profile* profile,
                  SearchTagRegistry* search_tag_registry,
                  KerberosCredentialsManager* kerberos_credentials_manager);
  ~KerberosSection() override;

  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  const char* GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

 private:
  // KerberosCredentialsManager::Observer:
  void OnAccountsChanged() override;
  void OnKerberosEnabledStateChanged() override;

  void UpdateKerberosSearchConcepts();

  raw_ptr<KerberosCredentialsManager, ExperimentalAsh>
      kerberos_credentials_manager_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_KERBEROS_KERBEROS_SECTION_H_
