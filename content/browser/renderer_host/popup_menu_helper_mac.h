// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_MAC_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "ui/gfx/geometry/rect.h"

#ifdef __OBJC__
@class WebMenuRunner;
#else
class WebMenuRunner;
#endif

namespace base {
class ScopedPumpMessagesInPrivateModes;
}

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;
class RenderWidgetHostViewMac;

class PopupMenuHelper : public RenderWidgetHostObserver {
 public:
  class Delegate {
   public:
    virtual void OnMenuClosed() = 0;
  };

  // Creates a PopupMenuHelper that will notify |render_frame_host| when a user
  // selects or cancels the popup. |delegate| is notified when the menu is
  // closed.
  PopupMenuHelper(
      Delegate* delegate,
      RenderFrameHost* render_frame_host,
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client);

  PopupMenuHelper(const PopupMenuHelper&) = delete;
  PopupMenuHelper& operator=(const PopupMenuHelper&) = delete;

  ~PopupMenuHelper() override;
  void Hide();

  // Shows the popup menu and notifies the RenderFrameHost of the selection/
  // cancellation. This call is blocking.
  void ShowPopupMenu(const gfx::Rect& bounds,
                     int item_height,
                     double item_font_size,
                     int selected_item,
                     std::vector<blink::mojom::MenuItemPtr> items,
                     bool right_aligned,
                     bool allow_multiple_selection);

  // Immediately return from ShowPopupMenu.
  CONTENT_EXPORT static void DontShowPopupMenuForTesting();

 private:
  // RenderWidgetHostObserver implementation:
  void RenderWidgetHostVisibilityChanged(RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override;

  RenderWidgetHostViewMac* GetRenderWidgetHostView() const;

  raw_ptr<Delegate> delegate_;  // Weak. Owns |this|.

  base::ScopedObservation<RenderWidgetHost, RenderWidgetHostObserver>
      observation_{this};
  base::WeakPtr<RenderFrameHostImpl> render_frame_host_;
  mojo::Remote<blink::mojom::PopupMenuClient> popup_client_;
  WebMenuRunner* menu_runner_ = nil;
  bool popup_was_hidden_ = false;

  // Controls whether messages can be pumped during the menu fade.
  std::unique_ptr<base::ScopedPumpMessagesInPrivateModes> pump_in_fade_;

  base::WeakPtrFactory<PopupMenuHelper> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_MAC_H_
