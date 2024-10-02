// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/extensions/extension_view.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

namespace extensions {
class ExtensionViewHost;
}

// This handles the display portion of an ExtensionHost.
class ExtensionViewViews : public views::WebView,
                           public extensions::ExtensionView {
 public:
  METADATA_HEADER(ExtensionViewViews);
  // A class that represents the container that this view is in.
  // (bottom shelf, side bar, etc.)
  class Container {
   public:
    virtual ~Container() = default;

    virtual void OnExtensionSizeChanged(ExtensionViewViews* view) {}
    virtual gfx::Size GetMinBounds() = 0;
    virtual gfx::Size GetMaxBounds() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnViewDestroying() = 0;
  };

  explicit ExtensionViewViews(extensions::ExtensionViewHost* host);
  ExtensionViewViews(const ExtensionViewViews&) = delete;
  ExtensionViewViews& operator=(const ExtensionViewViews&) = delete;
  ~ExtensionViewViews() override;

  void Init();

  // views::WebView:
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  gfx::Size GetMinimumSize() const override;

  void SetMinimumSize(const gfx::Size& minimum_size);

  void SetContainer(ExtensionViewViews::Container* container);
  ExtensionViewViews::Container* GetContainer() const;

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // extensions::ExtensionView:
  gfx::NativeView GetNativeView() override;
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void OnLoaded() override;

  // views::WebView:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void PreferredSizeChanged() override;
  void OnWebContentsAttached() override;

  raw_ptr<extensions::ExtensionViewHost, DanglingUntriaged> host_;

  // What we should set the preferred width to once the ExtensionViewViews has
  // loaded.
  gfx::Size pending_preferred_size_;

  absl::optional<gfx::Size> minimum_size_;

  // The container this view is in (not necessarily its direct superview).
  // Note: the view does not own its container.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Container* container_ = nullptr;

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // The associated observers.
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_
