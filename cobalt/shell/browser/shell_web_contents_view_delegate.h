// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
#define COBALT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"

#if defined(SHELL_USE_TOOLKIT_VIEWS)
#include "ui/menus/simple_menu_model.h"          // nogncheck
#include "ui/views/controls/menu/menu_runner.h"  // nogncheck
#endif

namespace content {

class ShellWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  explicit ShellWebContentsViewDelegate(WebContents* web_contents);

  ShellWebContentsViewDelegate(const ShellWebContentsViewDelegate&) = delete;
  ShellWebContentsViewDelegate& operator=(const ShellWebContentsViewDelegate&) =
      delete;

  ~ShellWebContentsViewDelegate() override;

  // Overridden from WebContentsViewDelegate:
  void ShowContextMenu(RenderFrameHost& render_frame_host,
                       const ContextMenuParams& params) override;

 private:
  raw_ptr<WebContents> web_contents_;

#if defined(SHELL_USE_TOOLKIT_VIEWS)
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
#endif
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
