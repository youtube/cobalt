// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_SETTINGS_WINDOW_FINDER_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_SETTINGS_WINDOW_FINDER_WIN_H_

#include <windows.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

// Encapsulates the Win32 window finding, polling, and event-hooking heuristics
// to discover the Windows Settings application window.
//
// All methods must be called on, and callbacks will be executed on, the same
// sequenced thread.
class SettingsWindowFinderWin {
 public:
  using WindowFoundCallback = base::OnceCallback<void(HWND)>;

  SettingsWindowFinderWin();

  SettingsWindowFinderWin(const SettingsWindowFinderWin&) = delete;
  SettingsWindowFinderWin& operator=(const SettingsWindowFinderWin&) = delete;

  virtual ~SettingsWindowFinderWin();

  // Starts the search for the Settings window. If the window is already
  // present, the callback is executed immediately. Otherwise, it listens for
  // window-create events and times out after the specified duration, calling
  // `on_timeout`.
  virtual void Start(base::TimeDelta timeout,
                     WindowFoundCallback on_found,
                     base::OnceClosure on_timeout);

  // Stops any active event hooks and timers, and invalidates outstanding
  // callbacks.
  virtual void Stop();

 protected:
  virtual HWND FindSettingsTopLevelWindow() const;

 private:
  bool IsLikelySettingsWindow(HWND hwnd) const;

  static void CALLBACK WinEventCallback(HWINEVENTHOOK hWinEventHook,
                                        DWORD event,
                                        HWND hwnd,
                                        LONG idObject,
                                        LONG idChild,
                                        DWORD dwEventThread,
                                        DWORD dwmsEventTime);

  void OnTimeout();

  WindowFoundCallback on_found_;
  base::OnceClosure on_timeout_;
  base::OneShotTimer timeout_timer_;
  HWINEVENTHOOK winevent_hook_ = nullptr;

  bool is_active_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SettingsWindowFinderWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_SETTINGS_WINDOW_FINDER_WIN_H_
