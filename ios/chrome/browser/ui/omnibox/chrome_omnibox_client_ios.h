// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_

#include <memory>
#include <unordered_map>

#include "base/scoped_multi_source_observation.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#include "ios/web/public/web_state_observer.h"

class ChromeBrowserState;
class WebLocationBar;
namespace feature_engagement {
class Tracker;
}
namespace web {
class NavigationContext;
class WebState;
}  // namespace web

class ChromeOmniboxClientIOS : public OmniboxClient,
                               public web::WebStateObserver {
 public:
  ChromeOmniboxClientIOS(WebLocationBar* location_bar,
                         ChromeBrowserState* browser_state,
                         feature_engagement::Tracker* tracker);

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
  PrefService* GetPrefs() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
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
  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match,
      IDNA2008DeviationCharacter deviation_char_in_hostname) override;
  LocationBarModel* GetLocationBarModel() override;

  // web::WebStateObserver.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // Object associated with a web state id in `web_state_tracker_`. If the
  // navigation succeeds, the shortcut is stored in the ShortcutsDatabase.
  struct ShortcutElement {
    std::u16string text;
    AutocompleteMatch match;
  };
  WebLocationBar* location_bar_;
  ChromeBrowserState* browser_state_;
  AutocompleteSchemeClassifierImpl scheme_classifier_;
  feature_engagement::Tracker* engagement_tracker_;
  // Stores observed navigations from the omnibox. Items are removed once
  // navigation finishes or when it's destroyed.
  std::unordered_map<int32_t, ShortcutElement> web_state_tracker_;

  // Automatically remove this observer from its host when destroyed.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      scoped_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_IOS_H_
