// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

class BookmarksUI : public content::WebUIController {
 public:
  explicit BookmarksUI(content::WebUI* web_ui);

  BookmarksUI(const BookmarksUI&) = delete;
  BookmarksUI& operator=(const BookmarksUI&) = delete;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);
};

#endif  // CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_UI_H_
