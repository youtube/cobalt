// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/favicon_impl/glic_tab_favicon_adapter_android.h"

#include "base/functional/bind.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace glic {

FaviconAdapterAndroid::FaviconAdapterAndroid(tabs::TabInterface* tab,
                                             FaviconNotifier* notifier)
    : FaviconAdapter(tab, notifier) {
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab_->GetHandle());
  if (tab_android) {
    TabFavicon::AddObserver(tab_android, this);
    observed_tab_android_ = tab_android;
    TabFavicon::GetBitmapForTabOrFallback(
        tab_android,
        base::BindOnce(&FaviconAdapterAndroid::OnInitialFaviconLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

FaviconAdapterAndroid::~FaviconAdapterAndroid() {
  if (observed_tab_android_) {
    TabFavicon::RemoveObserver(observed_tab_android_, this);
  }
}

TabFavicon::Observer* FaviconAdapterAndroid::GetTabFaviconObserverForTesting() {
  return this;
}

void FaviconAdapterAndroid::OnFaviconUpdated(const SkBitmap& bitmap) {
  notifier_->SetFaviconAndNotify(FaviconData::FromBitmap(bitmap));
}

void FaviconAdapterAndroid::OnInitialFaviconLoaded(const SkBitmap& bitmap) {
  if (!observed_tab_android_) {
    return;
  }
  notifier_->SetFaviconAndNotify(FaviconData::FromBitmap(bitmap));
}

}  // namespace glic
