// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_PARENTAL_CONTROLS_H_
#define CHROME_BROWSER_WIN_PARENTAL_CONTROLS_H_

struct WinParentalControls {
  bool any_restrictions = false;
  bool logging_required = false;
  bool web_filter = false;
};

// Calculates and caches the platform parental controls on a worker thread.
void InitializeWinParentalControls();

// Returns a struct of enabled parental controls. This method evaluates and
// caches if the platform controls have been enabled on the first call, which
// requires a thread supporting blocking. Subsequent calls may be from any
// thread.
[[nodiscard]] const WinParentalControls& GetWinParentalControls();

#endif  // CHROME_BROWSER_WIN_PARENTAL_CONTROLS_H_
