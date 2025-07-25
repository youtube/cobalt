// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

class ThemeServiceObserver : public base::CheckedObserver {
 public:
  // Called when the user has changed the browser theme.
  virtual void OnThemeChanged() = 0;

 protected:
  ~ThemeServiceObserver() override = default;
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_OBSERVER_H_
