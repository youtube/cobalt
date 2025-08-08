// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/search_results_view.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

AshWebView::InitParams GetInitParams() {
  AshWebView::InitParams params;
  params.suppress_navigation = true;
  return params;
}

// Modifies `new_tab_params` to open in a new tab.
void OpenURLFromTabInternal(NavigateParams& new_tab_params) {
  new_tab_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&new_tab_params);
}

}  // namespace

SearchResultsView::SearchResultsView() : AshWebViewImpl(GetInitParams()) {
  DCHECK(IsSunfishFeatureEnabledWithFeatureKey());
}

SearchResultsView::~SearchResultsView() = default;

content::WebContents* SearchResultsView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // Open the URL specified by `params` in a new tab.
  NavigateParams new_tab_params(static_cast<Browser*>(nullptr), params.url,
                                params.transition);
  new_tab_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  new_tab_params.initiating_profile =
      Profile::FromBrowserContext(source->GetBrowserContext());
  OpenURLFromTabInternal(new_tab_params);

  if (auto* controller = CaptureModeController::Get()) {
    controller->OnSearchResultClicked();
  }
  return new_tab_params.navigated_or_inserted_contents;
}

bool SearchResultsView::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // All navigation attempts are suppressed by `suppress_navigation`, which is
  // needed to override opening links in `OpenURLFromTab()`. Ensure new web
  // contents also open in new tabs. See
  // `AshWebViewImpl::NotifyDidSuppressNavigation()`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<SearchResultsView>& self, GURL url) {
            if (self) {
              self->OpenURLFromTab(
                  self->web_contents(),
                  content::OpenURLParams(
                      url, content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_LINK,
                      /*is_renderer_initiated=*/false),
                  /*navigation_handle_callback=*/{});
            }
          },
          weak_factory_.GetWeakPtr(), target_url));
  return false;
}

BEGIN_METADATA(SearchResultsView)
END_METADATA

}  // namespace ash
