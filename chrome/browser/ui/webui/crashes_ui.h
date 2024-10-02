// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

class CrashesUI : public content::WebUIController {
 public:
  explicit CrashesUI(content::WebUI* web_ui);

  CrashesUI(const CrashesUI&) = delete;
  CrashesUI& operator=(const CrashesUI&) = delete;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_
