// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_
#define COBALT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace content {

class ShellJavaScriptDialog;

class ShellJavaScriptDialogManager : public JavaScriptDialogManager {
 public:
  ShellJavaScriptDialogManager();

  ShellJavaScriptDialogManager(const ShellJavaScriptDialogManager&) = delete;
  ShellJavaScriptDialogManager& operator=(const ShellJavaScriptDialogManager&) =
      delete;

  ~ShellJavaScriptDialogManager() override;

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;

  void CancelDialogs(WebContents* web_contents, bool reset_state) override;

  // Called by the ShellJavaScriptDialog when it closes.
  void DialogClosed(ShellJavaScriptDialog* dialog);

  // Used for content_browsertests.
  void set_dialog_request_callback(base::OnceClosure callback) {
    dialog_request_callback_ = std::move(callback);
  }
  void set_should_proceed_on_beforeunload(bool proceed, bool success) {
    should_proceed_on_beforeunload_ = proceed;
    beforeunload_success_ = success;
  }

 private:
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if

  base::OnceClosure dialog_request_callback_;

  // Whether to automatically proceed when asked to display a BeforeUnload
  // dialog, and the return value that should be passed (success or failure).
  bool should_proceed_on_beforeunload_;
  bool beforeunload_success_;

  DialogClosedCallback before_unload_callback_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_JAVASCRIPT_DIALOG_MANAGER_H_
