// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SAFE_ACCEPT_FILE_DIALOG_EVENT_HANDLER_WIN_H_
#define UI_SHELL_DIALOGS_SAFE_ACCEPT_FILE_DIALOG_EVENT_HANDLER_WIN_H_

#include <shobjidl_core.h>
#include <wrl.h>
#include <wrl/client.h>

#include <optional>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

class SafeAcceptFileDialogEventHandlerTest;

namespace ui {

// This `IFileDialogEvents` implementation ensures that the file dialog it is
// attached to cannot be accepted immediately (within 500ms of opening) and
// prevents keyjacking by ignoring Enter key presses if the key was already
// held down when the dialog opened.
class SHELL_DIALOGS_EXPORT SafeAcceptFileDialogEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IFileDialogEvents> {
 public:
  SafeAcceptFileDialogEventHandler();

 private:
  friend class ::SafeAcceptFileDialogEventHandlerTest;

  ~SafeAcceptFileDialogEventHandler() override;

  // IFileDialogEvents:
  IFACEMETHODIMP OnTypeChange(IFileDialog* pfd) override;
  IFACEMETHODIMP OnFileOk(IFileDialog*) override;
  IFACEMETHODIMP OnFolderChange(IFileDialog*) override;
  IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) override;
  IFACEMETHODIMP OnSelectionChange(IFileDialog*) override;
  IFACEMETHODIMP OnShareViolation(IFileDialog*,
                                  IShellItem*,
                                  FDE_SHAREVIOLATION_RESPONSE*) override;
  IFACEMETHODIMP OnOverwrite(IFileDialog*,
                             IShellItem*,
                             FDE_OVERWRITE_RESPONSE*) override;

  // Initializes the event handler.
  HRESULT Initialize(IFileDialog* file_dialog);

  void InitializeForTesting(bool enter_down_at_init) {
    shown_time_ = base::TimeTicks::Now();
    initial_enter_hold_ = enter_down_at_init;
  }

  void SetKeyStateForTesting(bool is_down) { key_state_for_testing_ = is_down; }

  // Returns `true` if the Enter key is currently pressed.
  bool IsEnterKeyDown() const;

  // Clears the initial Enter hold mitigation upon physical key release.
  void OnEnterKeyboardReleased();

  // Thread message hook to intercept physical WM_KEYUP events.
  static LRESULT CALLBACK MessageHookCallback(int code,
                                              WPARAM wParam,
                                              LPARAM lParam);

  // Indicates if `Initialize()` has been invoked. Used to prevent multiple
  // initializations.
  bool initialize_called_ = false;

  // The message hook handle. Used to uninstall the hook.
  HHOOK message_hook_ = nullptr;

  // The timestamp when the dialog was opened. Used to enforce the minimum
  // display duration.
  base::TimeTicks shown_time_;

  // Indicates if Enter was already down when the dialog opened. Used to
  // prevent keyjacking.
  bool initial_enter_hold_ = false;

  // If set, overrides the actual keyboard state for testing.
  std::optional<bool> key_state_for_testing_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SAFE_ACCEPT_FILE_DIALOG_EVENT_HANDLER_WIN_H_
