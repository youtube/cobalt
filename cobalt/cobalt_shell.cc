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

#include "cobalt/cobalt_shell.h"
#include "base/command_line.h"
#include "content/public/browser/presentation_receiver_flags.h"
#include "content/public/common/content_switches.h"

// This file defines a Cobalt specific shell entry point, that delegates most
// of its functionality to content::Shell. This allows overrides to
// WebContentsDelegate and WebContentsObserver interfaces.
//
// We expect to fully migrate away from depending on content::Shell in the
// future.

namespace cobalt {

CobaltShell::CobaltShell(std::unique_ptr<content::WebContents> web_contents,
                         bool should_set_delegate)
    : content::Shell(std::move(web_contents), should_set_delegate) {}

// static
content::Shell* CobaltShell::CreateShell(
    std::unique_ptr<content::WebContents> web_contents,
    const gfx::Size& initial_size,
    bool should_set_delegate) {
  content::WebContents* raw_web_contents = web_contents.get();
  // Create a Cobalt specific shell instance
  CobaltShell* shell =
      new CobaltShell(std::move(web_contents), should_set_delegate);
  // Delegate the rest of Shell setup to content::Shell
  return content::Shell::CreateShellFromPointer(
      shell, raw_web_contents, initial_size, should_set_delegate);
}

// static
content::Shell* CobaltShell::CreateNewWindow(
    content::BrowserContext* browser_context,
    const GURL& url,
    const scoped_refptr<content::SiteInstance>& site_instance,
    const gfx::Size& initial_size) {
  content::WebContents::CreateParams create_params(browser_context,
                                                   site_instance);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePresentationReceiverForTesting)) {
    create_params.starting_sandbox_flags =
        content::kPresentationReceiverSandboxFlags;
  }
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);
  content::Shell* shell =
      CreateShell(std::move(web_contents), AdjustWindowSize(initial_size),
                  true /* should_set_delegate */);

  if (!url.is_empty()) {
    shell->LoadURL(url);
  }
  return shell;
}

// Placeholder for a WebContentsObserver override
void CobaltShell::PrimaryMainDocumentElementAvailable() {
  LOG(INFO) << "Cobalt::PrimaryMainDocumentElementAvailable";
}

}  // namespace cobalt
