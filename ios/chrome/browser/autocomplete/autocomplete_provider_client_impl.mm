// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/autocomplete_provider_client_impl.h"

#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/browser/top_sites.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/language/core/browser/pref_names.h"
#import "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/omnibox_triggered_feature_service.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/driver/sync_service.h"
#import "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/in_memory_url_index_factory.h"
#import "ios/chrome/browser/autocomplete/omnibox_pedal_implementation.h"
#import "ios/chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/autocomplete/shortcuts_backend_factory.h"
#import "ios/chrome/browser/autocomplete/tab_matcher_impl.h"
#import "ios/chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/history/top_sites_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AutocompleteProviderClientImpl::AutocompleteProviderClientImpl(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      url_consent_helper_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewPersonalizedDataCollectionConsentHelper(
                  SyncServiceFactory::GetForBrowserState(browser_state_))),
      omnibox_triggered_feature_service_(
          std::make_unique<OmniboxTriggeredFeatureService>()),
      tab_matcher_(browser_state_) {
  pedal_provider_ = std::make_unique<OmniboxPedalProvider>(
      *this, GetPedalImplementations(IsOffTheRecord(), false));
}

AutocompleteProviderClientImpl::~AutocompleteProviderClientImpl() {}

scoped_refptr<network::SharedURLLoaderFactory>
AutocompleteProviderClientImpl::GetURLLoaderFactory() {
  return browser_state_->GetSharedURLLoaderFactory();
}

PrefService* AutocompleteProviderClientImpl::GetPrefs() const {
  return browser_state_->GetPrefs();
}

PrefService* AutocompleteProviderClientImpl::GetLocalState() {
  return GetApplicationContext()->GetLocalState();
}

std::string AutocompleteProviderClientImpl::GetApplicationLocale() const {
  return GetApplicationContext()->GetApplicationLocale();
}

const AutocompleteSchemeClassifier&
AutocompleteProviderClientImpl::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier*
AutocompleteProviderClientImpl::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForBrowserState(browser_state_);
}

history::HistoryService* AutocompleteProviderClientImpl::GetHistoryService() {
  return ios::HistoryServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<history::TopSites> AutocompleteProviderClientImpl::GetTopSites() {
  return ios::TopSitesFactory::GetForBrowserState(browser_state_);
}

bookmarks::BookmarkModel*
AutocompleteProviderClientImpl::GetLocalOrSyncableBookmarkModel() {
  return ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
      browser_state_);
}

history::URLDatabase* AutocompleteProviderClientImpl::GetInMemoryDatabase() {
  // This method is called in unit test contexts where the HistoryService isn't
  // loaded. In that case, return null.
  history::HistoryService* history_service = GetHistoryService();
  return history_service ? history_service->InMemoryDatabase() : nullptr;
}

InMemoryURLIndex* AutocompleteProviderClientImpl::GetInMemoryURLIndex() {
  return ios::InMemoryURLIndexFactory::GetForBrowserState(browser_state_);
}

TemplateURLService* AutocompleteProviderClientImpl::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
}

const TemplateURLService*
AutocompleteProviderClientImpl::GetTemplateURLService() const {
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
}

RemoteSuggestionsService*
AutocompleteProviderClientImpl::GetRemoteSuggestionsService(
    bool create_if_necessary) const {
  return RemoteSuggestionsServiceFactory::GetForBrowserState(
      browser_state_, create_if_necessary);
}

DocumentSuggestionsService*
AutocompleteProviderClientImpl::GetDocumentSuggestionsService(
    bool create_if_necessary) const {
  return nullptr;
}

ZeroSuggestCacheService*
AutocompleteProviderClientImpl::GetZeroSuggestCacheService() {
  return ios::ZeroSuggestCacheServiceFactory::GetForBrowserState(
      browser_state_);
}

const ZeroSuggestCacheService*
AutocompleteProviderClientImpl::GetZeroSuggestCacheService() const {
  return ios::ZeroSuggestCacheServiceFactory::GetForBrowserState(
      browser_state_);
}

OmniboxPedalProvider* AutocompleteProviderClientImpl::GetPedalProvider() const {
  return pedal_provider_.get();
}

scoped_refptr<ShortcutsBackend>
AutocompleteProviderClientImpl::GetShortcutsBackend() {
  return ios::ShortcutsBackendFactory::GetForBrowserState(browser_state_);
}

scoped_refptr<ShortcutsBackend>
AutocompleteProviderClientImpl::GetShortcutsBackendIfExists() {
  return ios::ShortcutsBackendFactory::GetForBrowserStateIfExists(
      browser_state_);
}

std::unique_ptr<KeywordExtensionsDelegate>
AutocompleteProviderClientImpl::GetKeywordExtensionsDelegate(
    KeywordProvider* keyword_provider) {
  return nullptr;
}

query_tiles::TileService* AutocompleteProviderClientImpl::GetQueryTileService()
    const {
  return nullptr;
}

OmniboxTriggeredFeatureService*
AutocompleteProviderClientImpl::GetOmniboxTriggeredFeatureService() const {
  return omnibox_triggered_feature_service_.get();
}

AutocompleteScoringModelService*
AutocompleteProviderClientImpl::GetAutocompleteScoringModelService() const {
  return nullptr;
}

std::string AutocompleteProviderClientImpl::GetAcceptLanguages() const {
  return browser_state_->GetPrefs()->GetString(
      language::prefs::kAcceptLanguages);
}

std::string
AutocompleteProviderClientImpl::GetEmbedderRepresentationOfAboutScheme() const {
  return kChromeUIScheme;
}

std::vector<std::u16string> AutocompleteProviderClientImpl::GetBuiltinURLs() {
  std::vector<std::string> chrome_builtins(
      kChromeHostURLs, kChromeHostURLs + kNumberOfChromeHostURLs);
  std::sort(chrome_builtins.begin(), chrome_builtins.end());

  std::vector<std::u16string> builtins;
  for (auto& url : chrome_builtins) {
    builtins.push_back(base::ASCIIToUTF16(url));
  }
  return builtins;
}

std::vector<std::u16string>
AutocompleteProviderClientImpl::GetBuiltinsToProvideAsUserTypes() {
  return {base::ASCIIToUTF16(kChromeUIChromeURLsURL),
          base::ASCIIToUTF16(kChromeUIVersionURL)};
}

component_updater::ComponentUpdateService*
AutocompleteProviderClientImpl::GetComponentUpdateService() {
  return GetApplicationContext()->GetComponentUpdateService();
}

signin::IdentityManager* AutocompleteProviderClientImpl::GetIdentityManager()
    const {
  return IdentityManagerFactory::GetForBrowserState(browser_state_);
}

bool AutocompleteProviderClientImpl::IsOffTheRecord() const {
  return browser_state_->IsOffTheRecord();
}

bool AutocompleteProviderClientImpl::IsIncognitoProfile() const {
  return browser_state_->IsOffTheRecord();
}

bool AutocompleteProviderClientImpl::IsGuestSession() const {
  return false;
}

bool AutocompleteProviderClientImpl::SearchSuggestEnabled() const {
  return browser_state_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

bool AutocompleteProviderClientImpl::IsPersonalizedUrlDataCollectionActive()
    const {
  return url_consent_helper_->IsEnabled();
}

bool AutocompleteProviderClientImpl::IsAuthenticated() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state_);
  return identity_manager &&
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
}

bool AutocompleteProviderClientImpl::IsSyncActive() const {
  syncer::SyncService* sync =
      SyncServiceFactory::GetForBrowserState(browser_state_);
  return sync && sync->IsSyncFeatureActive();
}

void AutocompleteProviderClientImpl::Classify(
    const std::u16string& text,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    metrics::OmniboxEventProto::PageClassification page_classification,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) {
  AutocompleteClassifier* classifier = GetAutocompleteClassifier();
  classifier->Classify(text, prefer_keyword, allow_exact_keyword_match,
                       page_classification, match, alternate_nav_url);
}

void AutocompleteProviderClientImpl::DeleteMatchingURLsForKeywordFromHistory(
    history::KeywordID keyword_id,
    const std::u16string& term) {
  GetHistoryService()->DeleteMatchingURLsForKeyword(keyword_id, term);
}

void AutocompleteProviderClientImpl::PrefetchImage(const GURL& url) {}

const TabMatcher& AutocompleteProviderClientImpl::GetTabMatcher() const {
  return tab_matcher_;
}
