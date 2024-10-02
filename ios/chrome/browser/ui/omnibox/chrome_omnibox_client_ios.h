// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_

#include <memory>

#include "components/omnibox/browser/omnibox_client.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"

class ChromeBrowserState;
class WebOmniboxEditModelDelegate;

class ChromeOmniboxClientIOS : public OmniboxClient {
 public:
  ChromeOmniboxClientIOS(WebOmniboxEditModelDelegate* edit_model_delegate,
                         ChromeBrowserState* browser_state);

  ChromeOmniboxClientIOS(const ChromeOmniboxClientIOS&) = delete;
  ChromeOmniboxClientIOS& operator=(const ChromeOmniboxClientIOS&) = delete;

  ~ChromeOmniboxClientIOS() override;

  // OmniboxClient.
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  bool CurrentPageExists() const override;
  const GURL& GetURL() const override;
  bool IsLoading() const override;
  bool IsPasteAndGoEnabled() const override;
  bool IsDefaultSearchProviderEnabled() const override;
  SessionID GetSessionID() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const override;
  bool ProcessExtensionKeyword(const std::u16string& text,
                               const TemplateURL* template_url,
                               const AutocompleteMatch& match,
                               WindowOpenDisposition disposition) override;
  void OnUserPastedInOmniboxResultingInValidURL() override;
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override;
  void OnResultChanged(const AutocompleteResult& result,
                       bool default_match_changed,
                       bool should_prerender,
                       const BitmapFetchedCallback& on_bitmap_fetched) override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;
  void DiscardNonCommittedNavigations() override;
  const std::u16string& GetTitle() const override;
  gfx::Image GetFavicon() const override;

 private:
  WebOmniboxEditModelDelegate* edit_model_delegate_;
  ChromeBrowserState* browser_state_;
  AutocompleteSchemeClassifierImpl scheme_classifier_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_
