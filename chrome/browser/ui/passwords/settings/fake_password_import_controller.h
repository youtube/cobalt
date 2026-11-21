// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_FAKE_PASSWORD_IMPORT_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_FAKE_PASSWORD_IMPORT_CONTROLLER_H_

#include <vector>

#include "chrome/browser/ui/passwords/settings/password_import_controller_interface.h"
#include "components/password_manager/core/browser/import/import_results.h"

namespace content {
class WebContents;
}

class FakePasswordImportController : public PasswordImportControllerInterface {
 public:
  FakePasswordImportController();

  FakePasswordImportController(const FakePasswordImportController&) = delete;
  FakePasswordImportController& operator=(const FakePasswordImportController&) =
      delete;

  ~FakePasswordImportController() override;

  // PasswordImportControllerInterface:
  void Import(content::WebContents* web_contents,
              password_manager::PasswordForm::Store to_store,
              ImportResultsCallback results_callback) override;
  void ContinueImport(const std::vector<int>& selected_ids,
                      ImportResultsCallback results_callback) override;
  void ResetImporter(bool delete_file) override;

  void set_import_result_status(
      password_manager::ImportResults::Status status) {
    import_results_status_ = status;
  }

 private:
  password_manager::ImportResults::Status import_results_status_ =
      password_manager::ImportResults::Status::SUCCESS;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_FAKE_PASSWORD_IMPORT_CONTROLLER_H_
