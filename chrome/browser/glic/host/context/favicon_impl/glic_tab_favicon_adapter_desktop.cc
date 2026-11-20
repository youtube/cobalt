// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/favicon_impl/glic_tab_favicon_adapter_desktop.h"

#include "base/functional/bind.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace glic {

FaviconAdapterDesktop::FaviconAdapterDesktop(tabs::TabInterface* tab,
                                             FaviconNotifier* notifier)
    : FaviconAdapter(tab, notifier) {
  will_discard_contents_subscription_ = tab_->RegisterWillDiscardContents(
      base::BindRepeating(&FaviconAdapterDesktop::OnWillDiscardContents,
                          base::Unretained(this)));

  if (web_contents()) {
    auto* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents());
    if (favicon_driver) {
      favicon_driver->AddObserver(this);
    }
    notifier_->SetFaviconAndNotify(
        FaviconData::FromWebContents(*web_contents()));
  }
}

FaviconAdapterDesktop::~FaviconAdapterDesktop() {
  if (web_contents()) {
    auto* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents());
    if (favicon_driver) {
      favicon_driver->RemoveObserver(this);
    }
  }
}

void FaviconAdapterDesktop::OnWillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* previous_contents,
    content::WebContents* new_contents) {
  if (web_contents()) {
    auto* old_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents());
    if (old_driver) {
      old_driver->RemoveObserver(this);
    }
  }
  Observe(new_contents);
  if (new_contents) {
    auto* new_driver =
        favicon::ContentFaviconDriver::FromWebContents(new_contents);
    if (new_driver) {
      new_driver->AddObserver(this);
    }
  }
}

void FaviconAdapterDesktop::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  if (notification_icon_type != FaviconDriverObserver::NON_TOUCH_16_DIP) {
    return;
  }
  notifier_->SetFaviconAndNotify(FaviconData::FromImage(image));
}

}  // namespace glic
