// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_favicon.h"
#endif

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class FaviconData {
 public:
  FaviconData();
  FaviconData(const FaviconData&);
  FaviconData& operator=(const FaviconData&) = default;
  ~FaviconData();

  static FaviconData FromImage(gfx::Image image);
  static FaviconData FromBitmap(SkBitmap bitmap);
  static FaviconData FromWebContents(content::WebContents& web_contents);

  const SkBitmap& GetBitmap() const;
  bool operator==(const FaviconData& other) const;

 private:
  explicit FaviconData(gfx::Image image);
  explicit FaviconData(SkBitmap bitmap);

  // The actual bitmap to notify receivers of. May be lazily computed.
  mutable std::optional<SkBitmap> bitmap_;

  // The source image, if it can be found. Used only to detect changes more
  // efficiently.
  gfx::Image image_;
};

// Holds receivers and FaviconData, and notifies receivers when the favicon
// changes.
class FaviconNotifier {
 public:
  FaviconNotifier();
  ~FaviconNotifier();

  void SetFaviconAndNotify(const FaviconData& favicon_data);
  void Subscribe(::mojo::PendingRemote<mojom::TabFaviconHandler> receiver);

  mojo::RemoteSet<mojom::TabFaviconHandler>& receivers() { return receivers_; }
  const mojo::RemoteSet<mojom::TabFaviconHandler>& receivers() const {
    return receivers_;
  }

  const FaviconData& favicon_data() const { return favicon_data_; }

 private:
  void NotifyFaviconChanged();

  mojo::RemoteSet<mojom::TabFaviconHandler> receivers_;
  FaviconData favicon_data_;
};

class FaviconAdapter : public content::WebContentsObserver {
 public:
  FaviconAdapter(tabs::TabInterface* tab, FaviconNotifier* notifier);
  ~FaviconAdapter() override = default;

#if BUILDFLAG(IS_ANDROID)
  virtual TabFavicon::Observer* GetTabFaviconObserverForTesting() = 0;
#endif

 protected:
  raw_ptr<tabs::TabInterface> tab_;
  raw_ptr<FaviconNotifier> notifier_;

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_FAVICON_IMPL_GLIC_TAB_FAVICON_ADAPTER_H_
