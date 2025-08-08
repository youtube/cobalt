// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_

class Browser;

// Used for reading mode option in context menu.
void ShowReadAnythingSidePanel(Browser* browser);
bool IsReadAnythingEntryShowing(Browser* browser);

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_
