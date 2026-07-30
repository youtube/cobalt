// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/webui_url_constants.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog_browsertest.h"
#include "content/public/test/browser_test.h"

using MoveConfirmationPageTest = NonManagedUserWebUIBrowserTest;

IN_PROC_BROWSER_TEST_F(MoveConfirmationPageTest, All) {
  set_test_loader_host(ash::kChromeUICloudUploadHost);
  RunTest("chromeos/cloud_upload/move_confirmation_page_test.js",
          "mocha.run()");
}
