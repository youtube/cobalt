// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_command_processor.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "components/headless/command_handler/headless_command_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace headless {

bool ShouldProcessHeadlessCommands() {
  return IsHeadlessMode() && HeadlessCommandHandler::HasHeadlessCommandSwitches(
                                 *base::CommandLine::ForCurrentProcess());
}

void ProcessHeadlessCommands(content::BrowserContext* browser_context,
                             GURL target_url,
                             base::OnceClosure done_callback) {
  DCHECK(browser_context);

  // Create web contents to run the command processing in.
  content::WebContents::CreateParams create_params(browser_context);
  create_params.is_never_visible = true;
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(create_params));
  if (!web_contents) {
    return;
  }

  // Navigate web contents to the command processor page.
  GURL handler_url = HeadlessCommandHandler::GetHandlerUrl();
  content::NavigationController::LoadURLParams load_url_params(handler_url);
  web_contents->GetController().LoadURLWithParams(load_url_params);

  // Preserve web contents pointer so that the order of ProcessCommands
  // arguments evaluation does not wipe out unique pointer before it's
  // passed to command handler
  content::WebContents* web_contents_ptr = web_contents.get();
  HeadlessCommandHandler::ProcessCommands(
      web_contents_ptr, std::move(target_url),
      base::BindOnce(
          [](std::unique_ptr<content::WebContents> web_contents,
             base::OnceClosure done_callback) {
            web_contents.reset();
            std::move(done_callback).Run();
          },
          std::move(web_contents), std::move(done_callback)));
}

}  // namespace headless
