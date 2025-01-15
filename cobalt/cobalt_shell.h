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

#ifndef COBALT_COBALT_SHELL_H
#define COBALT_COBALT_SHELL_H

#include "components/js_injection/browser/js_communication_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"

namespace cobalt {

class CobaltShell : public content::Shell {
 public:
  // Override content::Shell entry point
  static content::Shell* CreateNewWindow(
      content::BrowserContext* browser_context,
      const GURL& url,
      const scoped_refptr<content::SiteInstance>& site_instance,
      const gfx::Size& initial_size);

  //   static std::unique_ptr<js_injection::JsCommunicationHost>
  //       js_communication_host_;

 private:
  CobaltShell(std::unique_ptr<content::WebContents> web_contents);

  // Overridden from content::Shell
  static content::Shell* CreateShell(
      std::unique_ptr<content::WebContents> web_contents,
      const gfx::Size& initial_size,
      bool should_set_delegate);

  // WebContentsObserver interface
  void PrimaryMainDocumentElementAvailable() override;

  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;

  void PortalWebContentsCreated(
      content::WebContents* portal_web_contents) override;
};

}  // namespace cobalt

#endif  // COBALT_COBALT_SHELL_H
