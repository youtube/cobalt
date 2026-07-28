// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_STUB_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "content/public/common/javascript_dialog_type.h"
#include "url/gurl.h"

namespace content {

class WebContentsImpl;

namespace protocol {

class PageHandler {
 public:
  using JavaScriptDialogCallback =
      base::OnceCallback<void(bool success, const std::u16string& user_input)>;
  static std::vector<PageHandler*> EnabledForWebContents(
      WebContentsImpl* contents);
  void DidRunJavaScriptDialog(const GURL& url,
                              const base::UnguessableToken& frame_id,
                              const std::u16string& message,
                              const std::u16string& default_prompt,
                              JavaScriptDialogType dialog_type,
                              bool has_non_devtools_handlers,
                              JavaScriptDialogCallback callback);
  void DidRunBeforeUnloadConfirm(const GURL& url,
                                 const base::UnguessableToken& frame_id,
                                 bool has_non_devtools_handlers,
                                 JavaScriptDialogCallback callback);
  void DidCloseJavaScriptDialog(const base::UnguessableToken& frame_id,
                                bool success,
                                const std::u16string& user_input);
};

}  // namespace protocol
}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_STUB_H_
