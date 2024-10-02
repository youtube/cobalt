// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_

#include <string>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {
struct AXActionData;
}  // namespace ui

namespace extensions {

struct AutomationInfo;

// Implementation of the chrome.automation API.
class AutomationInternalEnableTabFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableTab",
                             AUTOMATIONINTERNAL_ENABLETAB)
 protected:
  ~AutomationInternalEnableTabFunction() override = default;

  ExtensionFunction::ResponseAction Run() override;
};

class AutomationInternalPerformActionFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.performAction",
                             AUTOMATIONINTERNAL_PERFORMACTION)

 public:
  struct Result {
    Result();
    Result(const Result&);
    ~Result();
    // If there is a validation error then |automation_error| should be ignored.
    bool validation_success = false;
    // Assuming validation was successful, then a value of absl::nullopt
    // implies success. Otherwise, the failure is described in the contained
    // string.
    absl::optional<std::string> automation_error;
  };

  // Exposed to allow crosapi to reuse the implementation. |extension_id| can be
  // the empty string. |extension| and |automation_info| can be nullptr.
  static Result PerformAction(const ui::AXActionData& data,
                              const Extension* extension,
                              const AutomationInfo* automation_info);

 protected:
  ~AutomationInternalPerformActionFunction() override = default;

  ExtensionFunction::ResponseAction Run() override;

 private:
};

class AutomationInternalEnableTreeFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableTree",
                             AUTOMATIONINTERNAL_ENABLETREE)

 public:
  // Returns an error message or absl::nullopt on success. Exposed to allow
  // crosapi to reuse the implementation. |extension_id| can be the empty
  // string.
  static absl::optional<std::string> EnableTree(
      const ui::AXTreeID& ax_tree_id,
      const ExtensionId& extension_id);

 protected:
  ~AutomationInternalEnableTreeFunction() override = default;

  ExtensionFunction::ResponseAction Run() override;
};

class AutomationInternalEnableDesktopFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableDesktop",
                             AUTOMATIONINTERNAL_ENABLEDESKTOP)
 protected:
  ~AutomationInternalEnableDesktopFunction() override = default;

  ResponseAction Run() override;
};

class AutomationInternalDisableDesktopFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.disableDesktop",
                             AUTOMATIONINTERNAL_DISABLEDESKTOP)
 protected:
  ~AutomationInternalDisableDesktopFunction() override = default;

  ResponseAction Run() override;
};

class AutomationInternalQuerySelectorFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.querySelector",
                             AUTOMATIONINTERNAL_QUERYSELECTOR)

 public:
  using Callback =
      base::OnceCallback<void(const std::string& error, int result_acc_obj_id)>;

 protected:
  ~AutomationInternalQuerySelectorFunction() override = default;

  ResponseAction Run() override;

 private:
  void OnResponse(const std::string& error, int result_acc_obj_id);

  // Used for assigning a unique ID to each request so that the response can be
  // routed appropriately.
  static int query_request_id_counter_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_
