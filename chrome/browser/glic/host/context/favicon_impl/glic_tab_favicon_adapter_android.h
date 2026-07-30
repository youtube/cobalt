// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_ANDROID_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_favicon.h"
#include "chrome/browser/glic/host/context/favicon_impl/glic_tab_favicon_adapter.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class FaviconAdapterAndroid : public FaviconAdapter,
                              public TabFavicon::Observer {
 public:
  FaviconAdapterAndroid(tabs::TabInterface* tab, FaviconNotifier* notifier);
  ~FaviconAdapterAndroid() override;

  TabFavicon::Observer* GetTabFaviconObserverForTesting() override;

 private:
  // TabFavicon::Observer:
  void OnFaviconUpdated(const SkBitmap& bitmap) override;
  void OnInitialFaviconLoaded(const SkBitmap& bitmap);

  raw_ptr<TabAndroid> observed_tab_android_;
  base::WeakPtrFactory<FaviconAdapterAndroid> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_ANDROID_H_
