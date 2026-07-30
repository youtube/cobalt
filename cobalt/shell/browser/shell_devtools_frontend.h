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

#ifndef COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_
#define COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/shell/browser/shell_devtools_bindings.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class Shell;
class WebContents;

class ShellDevToolsFrontend : public ShellDevToolsDelegate,
                              public WebContentsObserver {
 public:
  static ShellDevToolsFrontend* Show(WebContents* inspected_contents);

  ShellDevToolsFrontend(const ShellDevToolsFrontend&) = delete;
  ShellDevToolsFrontend& operator=(const ShellDevToolsFrontend&) = delete;

  void Activate();
  void InspectElementAt(int x, int y);
  void Close() override;

  Shell* frontend_shell() const { return frontend_shell_; }

  base::WeakPtr<ShellDevToolsFrontend> GetWeakPtr();

 private:
  // WebContentsObserver overrides
  void PrimaryMainDocumentElementAvailable() override;
  void WebContentsDestroyed() override;

  ShellDevToolsFrontend(Shell* frontend_shell, WebContents* inspected_contents);
  ~ShellDevToolsFrontend() override;
  raw_ptr<Shell> frontend_shell_;
  std::unique_ptr<ShellDevToolsBindings> devtools_bindings_;

  base::WeakPtrFactory<ShellDevToolsFrontend> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_
