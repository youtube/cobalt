// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://cloud-upload/connect_onedrive.js.
 */

GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

var ConnectOneDriveTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://cloud-upload/test_loader.html?module=chromeos/' +
        'cloud_upload/connect_onedrive_test.js';
  }

  get typedefCppFixture() {
    return 'NonManagedUserWebUIBrowserTest';
  }
};

TEST_F('ConnectOneDriveTest', 'All', () => mocha.run());
