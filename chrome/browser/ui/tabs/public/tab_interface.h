// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/tabs/public/supports_handles.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;

namespace tabs {

class TabFeatures;

// A feature which wants to show tab-modal UI should call
// TabInterface::ShowModalUI and keep alive the instance of ScopedTabModalUI for
// the duration of the tab-modal UI.
class ScopedTabModalUI {
 public:
  ScopedTabModalUI() = default;
  virtual ~ScopedTabModalUI() = default;
};

// See documentation for ShouldAcceptMouseEventsWhileWindowInactive.
class ScopedAcceptMouseEventsWhileWindowInactive {
 public:
  ScopedAcceptMouseEventsWhileWindowInactive() = default;
  virtual ~ScopedAcceptMouseEventsWhileWindowInactive() = default;
};

// This is the public interface for tabs in a desktop browser. Most features in
// //chrome/browser depend on this interface, and thus to prevent circular
// dependencies this interface should not depend on anything else in //chrome.
// Ping erikchen for assistance if this class does not have the functionality
// your feature needs. This comment will be deleted after there are 10+ features
// in TabFeatures.
class TabInterface : public SupportsHandles<TabInterface> {
 public:
  // This method exists to ease the transition from WebContents to TabInterface.
  // This method should only be called on instances of WebContents that are
  // known to be tabs. Calling this on a non-tab will crash.
  static TabInterface* GetFromContents(content::WebContents* web_contents);

  // Code that references a WebContents should already know whether the
  // WebContents is a tab, and thus should use GetFromContents(). For historical
  // reasons, there is code in the repository that typically lives in or below
  // //content which does not know whether it's being invoked in the context of
  // a tab. This is an anti-pattern that should be avoided. When it is
  // unavoidable, this method may be used. Features that live entirely above the
  // //content layer should not use this method.
  static TabInterface* MaybeGetFromContents(content::WebContents* web_contents);

  // When a tab is in the background, the WebContents may be discarded to save
  // memory. When a tab is in the foreground it is guaranteed to have a
  // WebContents.
  virtual content::WebContents* GetContents() const = 0;

  // Closes the tab.
  virtual void Close() = 0;

  // Register for this callback to detect changes to GetContents(). The first
  // WebContents is the contents that will be discarded. The second WebContents
  // is the new contents. The tab is guaranteed to be in the background.
  using WillDiscardContentsCallback = base::RepeatingCallback<
      void(TabInterface*, content::WebContents*, content::WebContents*)>;
  virtual base::CallbackListSubscription RegisterWillDiscardContents(
      WillDiscardContentsCallback callback) = 0;

  // Whether the tab is the Active Tab in its browser window (see
  // TabStripModel's SelectionModel's active index for more details about the
  // active state). When a tab is in the foreground, this class guarantees that
  // GetContents() will return a non-nullptr WebContents, and this WebContents
  // will not change. If a tab is dragged out of a window (creating a new
  // window), it will briefly be deactivated, and then reactivate. To see if
  // this is happened, check the BrowserWindowInterface's session id.
  virtual bool IsActivated() const = 0;

  // Register for these two callbacks to detect changes to IsActivated().
  using DidActivateCallback = base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterDidActivate(
      DidActivateCallback callback) = 0;

  using WillDeactivateCallback = base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterWillDeactivate(
      WillDeactivateCallback callback) = 0;

  // ONLY USE THIS IF YOURE FEATURE NEEDS MORE SPECIFICITY THAN IsActivated().
  // Whether the tab is visible in the contents area of the browser window.
  // This will directly match the attached state until there are features that
  // provide multiple visible tabs per window. This state is not related to
  // widget visibility or occlusion of the window.
  virtual bool IsVisible() const = 0;

  // Register for these two callbacks to detect changes to IsVisible().
  using DidBecomeVisibleCallback = base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterDidBecomeVisible(
      DidBecomeVisibleCallback callback) = 0;

  using WillBecomeHiddenCallback = base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterWillBecomeHidden(
      WillBecomeHiddenCallback callback) = 0;

  // Register for this callback to detect when a tab will be detached from a
  // window.
  enum class DetachReason {
    // The tab is about to be deleted.
    kDelete,
    // The tab is going to be removed, in order to be inserted into another
    // window.
    kInsertIntoOtherWindow
  };
  using WillDetach = base::RepeatingCallback<void(TabInterface*, DetachReason)>;
  virtual base::CallbackListSubscription RegisterWillDetach(
      WillDetach callback) = 0;

  // Register for this callback to detect when a tab has been inserted into a
  // window's tab strip. Registered callbacks will fire for all tab strip
  // insertion events, including when the tab is first created and added to the
  // tab strip if a callback has been registered early enough in the tab's
  // lifecycle.
  using DidInsertCallback = base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterDidInsert(
      DidInsertCallback callback) = 0;

  // Register for this callback to detect when the pinned state changes.
  using PinnedStateChangedCallback =
      base::RepeatingCallback<void(TabInterface*, bool new_pinned_state)>;
  virtual base::CallbackListSubscription RegisterPinnedStateChanged(
      PinnedStateChangedCallback callback) = 0;

  // Register for this callback to detect when the group changes.
  using GroupChangedCallback = base::RepeatingCallback<
      void(TabInterface*, std::optional<tab_groups::TabGroupId> new_group)>;
  virtual base::CallbackListSubscription RegisterGroupChanged(
      GroupChangedCallback callback) = 0;

  // Features that want to show tab-modal UI are mutually exclusive. Before
  // showing a modal UI first check `CanShowModal`. Then call ShowModal() and
  // keep `ScopedTabModal` alive to prevent other features from showing
  // tab-modal UI.
  virtual bool CanShowModalUI() const = 0;
  virtual std::unique_ptr<ScopedTabModalUI> ShowModalUI() = 0;

  // Register for this callback to detect when a modal is shown or hidden.
  using TabInterfaceCallback = base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterModalUIChanged(
      TabInterfaceCallback callback) = 0;

  // A normal browser window has a tab strip and an omnibox. The returned value
  // never changes.
  virtual bool IsInNormalWindow() const = 0;

  // Always valid in production code. Exceptions are:
  //  (1) Tabs briefly do not have a BrowserWindowInterface when they are
  //  detached from one window and moved to another. That is an implementation
  //  detail of tab dragging and should not affect code outside of the tab-strip
  //  implementation.
  //  (2) Some tab-related unit tests create a TabInterface but do not create
  //  TabFeatures, BrowserWindowInterface or BrowserWindowFeatures. Most code
  //  that accesses this method should typically be scoped to TabFeatures or
  //  BrowserWindowFeatures, but some code (e.g. tab_helpers) are currently
  //  created in these unit tests. The proper solution is to convert these
  //  tab_helpers to TabFeatures, which is tracked in
  //  https://crbug.com/369319589.
  // This is a long winded way of saying: if you are using this code from
  // TabFeatures or BrowserWindowFeatures, you can safely assume that this is
  // always non-nullptr.
  virtual BrowserWindowInterface* GetBrowserWindowInterface() = 0;

  // Returns the feature controllers scoped to this tab.
  // TabFeatures that depend on other TabFeatures should not use this method.
  // Instead they should use use dependency injection to pass dependencies at
  // construction or initialization. This method exists for three reasons:
  //   (1) BrowserWindowFeatures often depend on state of TabFeatures for the
  //   active tab, which can change. BrowserWindowFeatures need a way to
  //   dynamically fetch TabFeatures.
  //   (2) To expose TabFeatures for tests.
  //   (3) It is not possible to perform dependency injection for legacy code
  //   that is conceptually a TabFeature and needs access to other TabFeatures.
  virtual tabs::TabFeatures* GetTabFeatures() = 0;

  // Return true if the tab is pinned in its tabstrip, or false otherwise.
  virtual bool IsPinned() const = 0;

  // Return true if the tab is part of a split view, or false otherwise.
  virtual bool IsSplit() const = 0;

  // Returns the id of the tab group this tab belongs to, or nullopt if the tab
  // is not grouped.
  virtual std::optional<tab_groups::TabGroupId> GetGroup() const = 0;

  // On macOS, tabs do not accept mouse events if the window is not active, even
  // if it's a child window that is active. Calling this method overrides that
  // behavior until the unique_ptr is destroyed. This is only relevant if the
  // tab is in the foreground.
  virtual bool ShouldAcceptMouseEventsWhileWindowInactive() const = 0;
  virtual std::unique_ptr<ScopedAcceptMouseEventsWhileWindowInactive>
  AcceptMouseEventsWhileWindowInactive() = 0;
};

using TabHandle = TabInterface::Handle;

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_
