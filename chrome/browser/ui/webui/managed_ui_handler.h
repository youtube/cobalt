// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGED_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGED_UI_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

class Profile;

class ManagedUIHandler : public content::WebUIMessageHandler,
                         public policy::PolicyService::Observer {
 public:
  explicit ManagedUIHandler(Profile* profile);

  ManagedUIHandler(const ManagedUIHandler&) = delete;
  ManagedUIHandler& operator=(const ManagedUIHandler&) = delete;

  ~ManagedUIHandler() override;

  // Sets load-time constants on |source|. This handles a flicker-free initial
  // page load (i.e. loadTimeData.getBoolean('isManaged')). Adds a
  // ManagedUIHandler to |web_ui|.  Continually updates |source| by name if
  // managed state changes. This is so page refreshes will have fresh, correct
  // data. Neither |web_ui| nor |source| may be nullptr.
  static void Initialize(content::WebUI* web_ui,
                         content::WebUIDataSource* source);

 protected:
  // Protected for tests.
  static void InitializeInternal(content::WebUI* web_ui,
                                 content::WebUIDataSource* source,
                                 Profile* profile);

 private:
  // policy::PolicyService::Observer
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // Handles the "observeManagedUI" message. No arguments.
  void HandleObserveManagedUI(const base::Value::List& args);

  // Add/remove observers on the PolicyService.
  void AddObservers();
  void RemoveObservers();

  // Generates a dictionary with "isManaged" and "managedByOrg" i18n keys based
  // on `managed_`. Called initialize and on each change for notifications.
  base::Value::Dict GetDataSourceUpdate() const;

  // Fire a webui listener notification if dark mode actually changed.
  void NotifyIfChanged();

  // To avoid double-removing the observers, which would cause a DCHECK()
  // failure.
  bool has_observers_ = false;

  PrefChangeRegistrar pref_registrar_;

  // Profile to update data sources on. Injected for testing.
  const raw_ptr<Profile> profile_;

  // Whether or not this page is currently showing the managed UI footnote.
  bool managed_;

  // Name of the WebUIDataSource to update.
  std::string source_name_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGED_UI_HANDLER_H_
