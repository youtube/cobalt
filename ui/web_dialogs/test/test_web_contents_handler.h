// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEB_DIALOGS_TEST_TEST_WEB_CONTENTS_HANDLER_H_
#define UI_WEB_DIALOGS_TEST_TEST_WEB_CONTENTS_HANDLER_H_

#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace ui {
namespace test {

class TestWebContentsHandler
    : public WebDialogWebContentsDelegate::WebContentsHandler {
 public:
  TestWebContentsHandler();

  TestWebContentsHandler(const TestWebContentsHandler&) = delete;
  TestWebContentsHandler& operator=(const TestWebContentsHandler&) = delete;

  ~TestWebContentsHandler() override;

 private:
  // Overridden from WebDialogWebContentsDelegate::WebContentsHandler:
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::BrowserContext* context,
                      content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
};

}  // namespace test
}  // namespace ui

#endif  // UI_WEB_DIALOGS_TEST_TEST_WEB_CONTENTS_HANDLER_H_
