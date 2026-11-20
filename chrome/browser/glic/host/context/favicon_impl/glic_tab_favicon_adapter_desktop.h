// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_DESKTOP_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_DESKTOP_H_

#include "base/callback_list.h"
#include "chrome/browser/glic/host/context/favicon_impl/glic_tab_favicon_adapter.h"
#include "components/favicon/core/favicon_driver_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class FaviconAdapterDesktop : public FaviconAdapter,
                              public favicon::FaviconDriverObserver {
 public:
  FaviconAdapterDesktop(tabs::TabInterface* tab, FaviconNotifier* notifier);
  ~FaviconAdapterDesktop() override;

 private:
  void OnWillDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* previous_contents,
                             content::WebContents* new_contents);

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  base::CallbackListSubscription will_discard_contents_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_DESKTOP_H_
