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

#include "content/public/browser/media_player_utils.h"

#include "base/check.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

void SuspendAllMediaPlayers(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents);
  static_cast<WebContentsImpl*>(web_contents)
      ->media_web_contents_observer()
      ->SuspendAllMediaPlayers();
}

}  // namespace content
