// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_H_
#define COBALT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace content {

class ShellJavaScriptDialogManager;

class ShellJavaScriptDialog {
 public:
  ShellJavaScriptDialog(ShellJavaScriptDialogManager* manager,
                        gfx::NativeWindow parent_window,
                        JavaScriptDialogType dialog_type,
                        const std::u16string& message_text,
                        const std::u16string& default_prompt_text,
                        JavaScriptDialogManager::DialogClosedCallback callback);

  ShellJavaScriptDialog(const ShellJavaScriptDialog&) = delete;
  ShellJavaScriptDialog& operator=(const ShellJavaScriptDialog&) = delete;

  ~ShellJavaScriptDialog();

  // Called to cancel a dialog mid-flight.
  void Cancel();

 private:
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_H_
