// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/shell/browser/shell_javascript_dialog_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell_javascript_dialog.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/common/shell_switches.h"

namespace content {

ShellJavaScriptDialogManager::ShellJavaScriptDialogManager()
    : should_proceed_on_beforeunload_(true), beforeunload_success_(true) {}

ShellJavaScriptDialogManager::~ShellJavaScriptDialogManager() = default;

void ShellJavaScriptDialogManager::RunJavaScriptDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  if (dialog_request_callback_) {
    std::move(dialog_request_callback_).Run();
    std::move(callback).Run(true, std::u16string());
    return;
  }

  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
  *did_suppress_message = true;
  return;
}

void ShellJavaScriptDialogManager::RunBeforeUnloadDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  // During tests, if the BeforeUnload should not proceed automatically, store
  // the callback and return.
  if (dialog_request_callback_) {
    std::move(dialog_request_callback_).Run();

    if (should_proceed_on_beforeunload_) {
      std::move(callback).Run(beforeunload_success_, std::u16string());
    } else {
      before_unload_callback_ = std::move(callback);
    }
    return;
  }

  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
  std::move(callback).Run(true, std::u16string());
  return;
}

void ShellJavaScriptDialogManager::CancelDialogs(WebContents* web_contents,
                                                 bool reset_state) {
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if

  if (before_unload_callback_.is_null()) {
    return;
  }

  if (reset_state) {
    std::move(before_unload_callback_).Run(false, std::u16string());
  }
}

void ShellJavaScriptDialogManager::DialogClosed(ShellJavaScriptDialog* dialog) {
  // TODO: implement ShellJavaScriptDialog for other platforms, drop this #if
}

}  // namespace content
