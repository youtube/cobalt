// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_OPEN_URL_DELEGATE_H_
#define COMPONENTS_ARC_INTENT_HELPER_OPEN_URL_DELEGATE_H_

#include "ash/components/arc/mojom/intent_helper.mojom.h"

class GURL;

namespace arc {

class OpenUrlDelegate {
 public:
  virtual ~OpenUrlDelegate() = default;

  // Opens the given URL in the Chrome browser.
  virtual void OpenUrlFromArc(const GURL& url) = 0;

  // Opens the given URL as a web app in the Chrome browser, falling back to
  // opening as a tab if no installed web app is found.
  virtual void OpenWebAppFromArc(const GURL& url) = 0;

  // Opens the given URL in a custom tab.
  virtual void OpenArcCustomTab(
      const GURL& url,
      int32_t task_id,
      mojom::IntentHelperHost::OnOpenCustomTabCallback callback) = 0;

  // Opens the requested |chrome_page|. If |chrome_page| is a settings page,
  // this will open the settings window.
  virtual void OpenChromePageFromArc(mojom::ChromePage chrome_page) = 0;

  // Opens the installed web app for the given |start_url|, delivering the
  // |intent| to that app. If no app is installed, falls back to opening the
  // |intent->data| as a tab in the browser, unless |intent->data| is not set,
  // in which case nothing happens.
  virtual void OpenAppWithIntent(const GURL& start_url,
                                 arc::mojom::LaunchIntentPtr intent) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_OPEN_URL_DELEGATE_H_
