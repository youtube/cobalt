// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"

#include "base/strings/escape.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"

namespace {
// Verbatim Match is placed in a dedicated SECTION_MOBILE_VERBATIM.
// While there are no other occupants of this section, the Relevance score
// remains important, because the Verbatim Match may get de-duplicated to other,
// higher ranking suggestions listed later on the list.
// Keep the relevance high to ensure matching suggestions listed later are
// merged to the Verbatim Match, not the other way around.
const int kVerbatimMatchRelevanceScore = 1602;

// Returns whether specific context is eligible for a verbatim match.
// Only offer verbatim match on a site visit and SRP (no NTP etc).
bool IsVerbatimMatchEligible(
    metrics::OmniboxEventProto::PageClassification context) {
  // Only offer verbatim match on a site visit and SRP (no NTP etc).
  return context == metrics::OmniboxEventProto::
                        SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT ||
         context == metrics::OmniboxEventProto::
                        SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT ||
         context == metrics::OmniboxEventProto::ANDROID_SEARCH_WIDGET ||
         context == metrics::OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET ||
         context == metrics::OmniboxEventProto::OTHER;
}
}  // namespace

ZeroSuggestVerbatimMatchProvider::ZeroSuggestVerbatimMatchProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(TYPE_VERBATIM_MATCH), client_(client) {}

ZeroSuggestVerbatimMatchProvider::~ZeroSuggestVerbatimMatchProvider() = default;

void ZeroSuggestVerbatimMatchProvider::Start(const AutocompleteInput& input,
                                             bool minimal_changes) {
  Stop(true, false);
  if (!IsVerbatimMatchEligible(input.current_page_classification()))
    return;

  // Only offer verbatim match after the user just focused the Omnibox on NTP,
  // SRP, or existing website view, or if the input field is empty.
  if (input.focus_type() == metrics::OmniboxFocusType::INTERACTION_DEFAULT)
    return;

  // For consistency with other zero-prefix providers.
  const auto& page_url = input.current_url();
  if (input.type() != metrics::OmniboxInputType::EMPTY &&
      !(page_url.is_valid() &&
        ((page_url.scheme() == url::kHttpScheme) ||
         (page_url.scheme() == url::kHttpsScheme) ||
         (page_url.scheme() == url::kAboutScheme) ||
         (page_url.scheme() ==
          client_->GetEmbedderRepresentationOfAboutScheme())))) {
    return;
  }

  AutocompleteInput verbatim_input = input;
  verbatim_input.set_prevent_inline_autocomplete(true);
  verbatim_input.set_allow_exact_keyword_match(false);

  AutocompleteMatch match =
      VerbatimMatchForURL(this, client_, verbatim_input, page_url,
                          input.current_title(), kVerbatimMatchRelevanceScore);
  // Make sure the URL is formatted the same was as most visited sites.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.suggestion_group_id = omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX;
  match.contents = url_formatter::FormatUrl(page_url, format_types,
                                            base::UnescapeRule::SPACES, nullptr,
                                            nullptr, nullptr);

  TermMatches term_matches;
  if (input.text().length() > 0) {
    term_matches = {{0, 0, input.text().length()}};
  }

  match.contents_class = ClassifyTermMatches(
      term_matches, match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  // If the URL suggestion comes from the default search engine, extract the
  // original search query and place it in fill_into_edit, to permit re-use of
  // the query for manual refinement.
  if (base::FeatureList::IsEnabled(
          omnibox::kSearchReadyOmniboxAllowQueryEdit)) {
    auto* const url_service = client_->GetTemplateURLService();
    if (url_service->IsSearchResultsPageFromDefaultSearchProvider(
            match.destination_url)) {
      auto* const dse = url_service->GetDefaultSearchProvider();
      if (dse->url_ref().SupportsReplacement(
              url_service->search_terms_data())) {
        dse->ExtractSearchTermsFromURL(match.destination_url,
                                       url_service->search_terms_data(),
                                       &match.fill_into_edit);
      }
    }
  }

  match.provider = this;
  matches_.push_back(std::move(match));
}
