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

#include "cobalt/shell/browser/preload_web_contents_observer.h"

#include "base/command_line.h"
#include "cobalt/shell/common/shell_switches.h"
#include "content/public/browser/web_contents.h"

namespace content {

PreloadWebContentsObserver::PreloadWebContentsObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

void PreloadWebContentsObserver::PrimaryPageChanged(Page& page) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kPreload)) {
    web_contents()->SetVisibility(content::Visibility::PRERENDER);
  }
}

}  // namespace content
