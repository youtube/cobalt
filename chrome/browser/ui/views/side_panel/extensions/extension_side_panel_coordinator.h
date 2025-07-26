// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_icon_image.h"

class BrowserWindowInterface;
class SidePanelEntryScope;
class SidePanelRegistry;

namespace tabs {
class TabInterface;
}

namespace content {
class WebContents;
}

namespace views {
class View;
}  // namespace views

namespace extensions {

class Extension;

// ExtensionSidePanelCoordinator handles the creation and registration of
// SidePanelEntries for the associated extension and creates the view to be
// shown if this extension's SidePanelEntry is active.
// TODO(crbug.com/40264634): Separate into different classes for global vs
// contextual extension side panels given the difference in behavior betweeen
// these two panel types.
class ExtensionSidePanelCoordinator : public ExtensionViewViews::Observer,
                                      public SidePanelService::Observer {
 public:
  explicit ExtensionSidePanelCoordinator(Profile* profile,
                                         BrowserWindowInterface* browser,
                                         tabs::TabInterface* tab_interface,
                                         const Extension* extension,
                                         SidePanelRegistry* registry,
                                         bool for_tab);
  ExtensionSidePanelCoordinator(const ExtensionSidePanelCoordinator&) = delete;
  ExtensionSidePanelCoordinator& operator=(
      const ExtensionSidePanelCoordinator&) = delete;
  ~ExtensionSidePanelCoordinator() override;

  // Returns the WebContents managed by `host_`.
  content::WebContents* GetHostWebContentsForTesting() const;

  // Deregisters this extension's SidePanelEntry from `registry_`.
  // To avoid re-entrancy this does not happen automatically in the destructor.
  void DeregisterEntry();

 private:
  SidePanelEntry::Key GetEntryKey() const;

  SidePanelEntry* GetEntry() const;

  bool IsGlobalCoordinator() const;

  // SidePanelService::Observer:
  void OnPanelOptionsChanged(
      const ExtensionId& extension_id,
      const api::side_panel::PanelOptions& updated_options) override;
  void OnSidePanelServiceShutdown() override;

  // ExtensionViewViews::Observer
  void OnViewDestroying() override;

  // Creates and registers the SidePanelEntry for this extension, and observes
  // the entry.
  void CreateAndRegisterEntry();

  // Creates a view for the extension's resource URL. This is called when this
  // extension's SidePanelEntry is about to be shown in the side panel and a
  // view for the entry has not been cached.
  std::unique_ptr<views::View> CreateView(SidePanelEntryScope& scope);

  // Called when window.close() is called from the extension's side panel page.
  // This closes the side panel if the extension's panel is showing. Otherwise
  // it clears the extension entry's cached view.
  void HandleCloseExtensionSidePanel(ExtensionHost* host);

  // Loads the `side_panel_url_` into the WebContents of the view for the
  // extension's SidePanelEntry. To avoid unnecessary updates, this is only
  // called when this extension's SidePanelEntry is currently active.
  void NavigateIfNecessary();

  // Loads the extension's icon for its SidePanelEntry.
  void LoadExtensionIcon();

  // Adds the icon of the extension to the action item. This action item is
  // later retrieved by the side panel coordinator to show the side panel.
  void UpdateActionItemIcon();

  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // Returns `browser_` if it is a global coordinator and otherwise it returns
  // the browser associated with `web_contents_`.
  BrowserWindowInterface* GetBrowser();

  // The profile associated with either `browser_` or `web_contents_`.
  raw_ptr<Profile> profile_;

  // The browser that owns `registry_` and the ExtensionSidePanelManager that
  // owns this class. A reference for this is kept so the side panel can be
  // closed when window.close() is called from the extension's side panel page.
  // Only one of `browser_` or `tab_interface_` should be defined.
  raw_ptr<BrowserWindowInterface> browser_;

  // The TabInterface that owns `registry_` and the ExtensionSidePanelManager
  // that owns this class. Refer to the comment for `browser_` on why this
  // reference needs to be kept.
  raw_ptr<tabs::TabInterface> tab_interface_;

  // The extension that registered the side panel content that's managed by this
  // class.
  raw_ptr<const Extension> extension_;

  // The SidePanelRegistry that lives in the same user data that an instance of
  // this class lives in. Owns all extension entries managed by `coordinators_`.
  raw_ptr<SidePanelRegistry> registry_;

  // The current URL set for the extension's global side panel. This is set in
  // the constructor or during OnPanelOptionsChanged.
  GURL side_panel_url_;

  // The ExtensionViewHost that backs the view in the side panel for this
  // extension. This is defined if and only if the aforementioned view exists.
  // Note: the view is destroyed when the side panel is closed or when the
  // SidePanelEntry for this extension is deregistered.
  std::unique_ptr<ExtensionViewHost> host_ = nullptr;

  // The extension's own icon for its side panel entry.
  std::unique_ptr<IconImage> extension_icon_;

  // Whether this coordinator is tab-scoped or window-scoped.
  const bool for_tab_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  base::ScopedObservation<ExtensionViewViews, ExtensionViewViews::Observer>
      scoped_view_observation_{this};
  base::ScopedObservation<SidePanelService, SidePanelService::Observer>
      scoped_service_observation_{this};

  // Must be the last member.
  base::WeakPtrFactory<ExtensionSidePanelCoordinator> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_
