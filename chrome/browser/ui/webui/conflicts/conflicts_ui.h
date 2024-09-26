// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

// The Web UI handler for about:conflicts.
class ConflictsUI : public content::WebUIController {
 public:
  explicit ConflictsUI(content::WebUI* web_ui);

  ConflictsUI(const ConflictsUI&) = delete;
  ConflictsUI& operator=(const ConflictsUI&) = delete;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_UI_H_
