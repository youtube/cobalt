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

#ifndef COBALT_BROWSER_COBALT_WEB_CONTENTS_DELEGATE_H_
#define COBALT_BROWSER_COBALT_WEB_CONTENTS_DELEGATE_H_

#include "content/public/browser/web_contents_delegate.h"

namespace cobalt {

class CobaltWebContentsDelegate : public content::WebContentsDelegate {
 public:
  // Implements content::WebContentsDelegate:
  void RequestMediaAccessPermission(content::WebContents*,
                                    const content::MediaStreamRequest&,
                                    content::MediaResponseCallback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost*,
                                  const GURL&,
                                  blink::mojom::MediaStreamType) override;
  void CloseContents(content::WebContents* source) override;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_WEB_CONTENTS_DELEGATE_H_
