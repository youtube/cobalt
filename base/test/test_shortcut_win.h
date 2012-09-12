// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_SHORTCUT_WIN_H_
#define BASE_TEST_TEST_SHORTCUT_WIN_H_

#include "base/file_path.h"
#include "base/win/shortcut.h"

// Windows shortcut functions used only by tests.

namespace base {
namespace win {

enum VerifyShortcutStatus {
  VERIFY_SHORTCUT_SUCCESS = 0,
  VERIFY_SHORTCUT_FAILURE_UNEXPECTED,
  VERIFY_SHORTCUT_FAILURE_FILE_NOT_FOUND,
  VERIFY_SHORTCUT_FAILURE_TARGET,
  VERIFY_SHORTCUT_FAILURE_WORKING_DIR,
  VERIFY_SHORTCUT_FAILURE_ARGUMENTS,
  VERIFY_SHORTCUT_FAILURE_DESCRIPTION,
  VERIFY_SHORTCUT_FAILURE_ICON,
  VERIFY_SHORTCUT_FAILURE_APP_ID,
  VERIFY_SHORTCUT_FAILURE_DUAL_MODE,
};

// Verify that a shortcut exists at |shortcut_path| with the expected
// |properties|.
VerifyShortcutStatus VerifyShortcut(const FilePath& shortcut_path,
                                    const ShortcutProperties& properties);


}  // namespace win
}  // namespace base

#endif  // BASE_TEST_TEST_SHORTCUT_WIN_H_
