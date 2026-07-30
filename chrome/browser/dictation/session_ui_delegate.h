// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_

#include "chrome/browser/dictation/session_state.h"

namespace dictation {

// Interface for the UI to communicate back to the session controller.
class SessionUiDelegate {
 public:
  virtual ~SessionUiDelegate() = default;

  // Called when the session end has been requested via the UI.
  virtual void UiRequestEndSession() = 0;

  // Called to end the active stream from the UI.
  virtual void UiRequestEndActiveStream() = 0;

  // Returns the current state of the dictation session.
  virtual SessionState GetState() const = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_
