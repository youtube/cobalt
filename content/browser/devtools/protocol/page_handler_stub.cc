// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "content/browser/devtools/protocol/page_handler.h"

namespace content {
namespace protocol {

// static
std::vector<PageHandler*> PageHandler::EnabledForWebContents(
    WebContentsImpl* contents) {
  return {};
}

void PageHandler::DidRunJavaScriptDialog(const GURL& url,
                                         const base::UnguessableToken& frame_id,
                                         const std::u16string& message,
                                         const std::u16string& default_prompt,
                                         JavaScriptDialogType dialog_type,
                                         bool has_non_devtools_handlers,
                                         JavaScriptDialogCallback callback) {}

void PageHandler::DidCloseJavaScriptDialog(
    const base::UnguessableToken& frame_id,
    bool success,
    const std::u16string& user_input) {}

void PageHandler::DidRunBeforeUnloadConfirm(
    const GURL& url,
    const base::UnguessableToken& frame_id,
    bool has_non_devtools_handlers,
    JavaScriptDialogCallback callback) {}
}  // namespace protocol

}  // namespace content
