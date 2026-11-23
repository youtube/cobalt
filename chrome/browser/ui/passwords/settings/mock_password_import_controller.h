// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_MOCK_PASSWORD_IMPORT_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_MOCK_PASSWORD_IMPORT_CONTROLLER_H_

#include <vector>

#include "chrome/browser/ui/passwords/settings/password_import_controller_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}

class MockPasswordImportController : public PasswordImportControllerInterface {
 public:
  MockPasswordImportController();

  MockPasswordImportController(const MockPasswordImportController&) = delete;
  MockPasswordImportController& operator=(const MockPasswordImportController&) =
      delete;

  ~MockPasswordImportController() override;

  // PasswordImportControllerInterface:
  MOCK_METHOD(void,
              Import,
              (content::WebContents * web_contents,
               password_manager::PasswordForm::Store to_store,
               ImportResultsCallback results_callback),
              (override));
  MOCK_METHOD(void,
              ContinueImport,
              (const std::vector<int>& selected_ids,
               ImportResultsCallback results_callback),
              (override));
  MOCK_METHOD(void, ResetImporter, (bool delete_file), (override));
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_MOCK_PASSWORD_IMPORT_CONTROLLER_H_
