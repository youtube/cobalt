// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_extensions_container_wrapper.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/equals_traits.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

WebUIToolbarExtensionsContainerWrapper::WebUIToolbarExtensionsContainerWrapper(
    WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate) {}

WebUIToolbarExtensionsContainerWrapper::
    ~WebUIToolbarExtensionsContainerWrapper() = default;

void WebUIToolbarExtensionsContainerWrapper::Init(
    content::WebContents* web_contents) {
  BrowserWindowInterface* browser = delegate_->GetBrowser();

  extensions_container_ = std::make_unique<WebUIToolbarExtensionsContainer>(
      *browser, delegate_->GetView()->GetWidget(), web_contents->GetWeakPtr(),
      &delegate_->GetIconTable(),
      /*push_icon_table_updates=*/false);

  extensions_container_->SetObserver(this);

  // Register `extensions_container_` as the `ExtensionsContainer` for
  // `browser`.
  scoped_extensions_container_user_data_ =
      std::make_unique<ui::ScopedUnownedUserData<ExtensionsContainer>>(
          browser->GetUnownedUserDataHost(), *extensions_container_);

  active_tab_subscription_ =
      browser->RegisterActiveTabDidChange(base::BindRepeating(
          &WebUIToolbarExtensionsContainerWrapper::OnActiveTabChanged,
          base::Unretained(this)));
}

void WebUIToolbarExtensionsContainerWrapper::OnThemeChanged() {
  if (extensions_container_) {
    // Icons may need re-rendering.
    extensions_container_->NotifyOfAllActions();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  if (extensions_container_) {
    // State of extensions depends on what's active --- e.g. some may be
    // disabled on some URLs.
    extensions_container_->NotifyOfAllActions();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionsAddedOrUpdated(
    std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
    std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> actions) {
  bool changed = false;
  for (auto& action : actions) {
    auto it = cached_actions_.find(action->id);
    if (it == cached_actions_.end() || !mojo::Equals(it->second, action)) {
      cached_actions_[action->id] = std::move(action);
      changed = true;
    }
  }
  if (changed) {
    SendExtensionsState();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionRemoved(
    std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
    const std::string& id) {
  if (cached_actions_.erase(id) > 0) {
    SendExtensionsState();
  }
}

void WebUIToolbarExtensionsContainerWrapper::OnActionPoppedOut(
    base::OnceClosure callback) {
  // TODO: Need to delay here until the WebUI animates out the icon and a
  // TrackedElement is available to anchor to.
  std::move(callback).Run();
}

void WebUIToolbarExtensionsContainerWrapper::SendExtensionsState() {
  std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> state;
  for (const auto& [id, action] : cached_actions_) {
    state.push_back(mojo::Clone(action));
  }
  delegate_->OnExtensionsStateChanged(std::move(state));
}
