// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_page_handler.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/web_ui.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

using bookmarks::BookmarkModel;

namespace {

std::string SuggestionAnswerTypeToString(int answer_type) {
  switch (answer_type) {
    case SuggestionAnswer::ANSWER_TYPE_INVALID:
      return "invalid";
    case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
      return "dictionary";
    case SuggestionAnswer::ANSWER_TYPE_FINANCE:
      return "finance";
    case SuggestionAnswer::ANSWER_TYPE_KNOWLEDGE_GRAPH:
      return "knowledge graph";
    case SuggestionAnswer::ANSWER_TYPE_LOCAL:
      return "local";
    case SuggestionAnswer::ANSWER_TYPE_SPORTS:
      return "sports";
    case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
      return "sunrise";
    case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
      return "translation";
    case SuggestionAnswer::ANSWER_TYPE_WEATHER:
      return "weather";
    case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
      return "when is";
    case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
      return "currency";
    case SuggestionAnswer::ANSWER_TYPE_LOCAL_TIME:
      return "local time";
    case SuggestionAnswer::ANSWER_TYPE_PLAY_INSTALL:
      return "play install";
    default:
      return std::to_string(answer_type);
  }
}

std::string SuggestionAnswerImageLineToString(
    const SuggestionAnswer::ImageLine& image_line) {
  std::string string;
  for (auto text_field : image_line.text_fields())
    string += base::UTF16ToUTF8(text_field.text());
  if (image_line.additional_text())
    string += " " + base::UTF16ToUTF8(image_line.additional_text()->text());
  if (image_line.status_text())
    string += " " + base::UTF16ToUTF8(image_line.status_text()->text());
  return string;
}

}  // namespace

namespace mojo {

template <>
struct TypeConverter<std::vector<mojom::ACMatchClassificationPtr>,
                     AutocompleteMatch::ACMatchClassifications> {
  static std::vector<mojom::ACMatchClassificationPtr> Convert(
      const AutocompleteMatch::ACMatchClassifications& input) {
    std::vector<mojom::ACMatchClassificationPtr> array;
    for (auto classification : input) {
      auto item = mojom::ACMatchClassification::New(classification.offset,
                                                    classification.style);
      array.push_back(std::move(item));
    }
    return array;
  }
};

template <>
struct TypeConverter<std::vector<mojom::AutocompleteAdditionalInfoPtr>,
                     AutocompleteMatch::AdditionalInfo> {
  static std::vector<mojom::AutocompleteAdditionalInfoPtr> Convert(
      const AutocompleteMatch::AdditionalInfo& input) {
    std::vector<mojom::AutocompleteAdditionalInfoPtr> array(input.size());
    size_t index = 0;
    for (auto i = input.begin(); i != input.end(); ++i, index++) {
      mojom::AutocompleteAdditionalInfoPtr item(
          mojom::AutocompleteAdditionalInfo::New());
      item->key = i->first;
      item->value = i->second;
      array[index] = std::move(item);
    }
    return array;
  }
};

template <>
struct TypeConverter<mojom::AutocompleteMatchPtr, AutocompleteMatch> {
  static mojom::AutocompleteMatchPtr Convert(const AutocompleteMatch& input) {
    mojom::AutocompleteMatchPtr result(mojom::AutocompleteMatch::New());
    if (input.provider) {
      result->provider_name = std::string(input.provider->GetName());
      result->provider_done = input.provider->done();
    }
    result->relevance = input.relevance;
    result->deletable = input.deletable;
    result->fill_into_edit = base::UTF16ToUTF8(input.fill_into_edit);
    result->inline_autocompletion =
        base::UTF16ToUTF8(input.inline_autocompletion);
    result->destination_url = input.destination_url.spec();
    result->stripped_destination_url = input.stripped_destination_url.spec();
    result->image = input.ImageUrl().spec().c_str();
    result->contents = base::UTF16ToUTF8(input.contents);
    result->contents_class =
        mojo::ConvertTo<std::vector<mojom::ACMatchClassificationPtr>>(
            input.contents_class);
    result->description = base::UTF16ToUTF8(input.description);
    result->description_class =
        mojo::ConvertTo<std::vector<mojom::ACMatchClassificationPtr>>(
            input.description_class);
    result->swap_contents_and_description = input.swap_contents_and_description;
    if (input.answer) {
      result->answer =
          SuggestionAnswerImageLineToString(input.answer->first_line()) +
          " / " +
          SuggestionAnswerImageLineToString(input.answer->second_line()) +
          " / " + SuggestionAnswerTypeToString(input.answer->type());
    }
    result->transition =
        ui::PageTransitionGetCoreTransitionString(input.transition);
    result->allowed_to_be_default_match = input.allowed_to_be_default_match;
    result->type = AutocompleteMatchType::ToString(input.type);
    result->is_search_type = AutocompleteMatch::IsSearchType(input.type);
    auto subtypes = input.subtypes;
    omnibox::SuggestType type = omnibox::TYPE_NATIVE_CHROME;
    AutocompleteController::GetMatchTypeAndExtendSubtypes(input, &type,
                                                          &subtypes);
    std::vector<std::string> subtypes_str;
    subtypes_str.push_back(base::NumberToString(type));
    base::ranges::transform(
        subtypes, std::back_inserter(subtypes_str),
        [](int subtype) { return base::NumberToString(subtype); });
    result->aqs_type_subtypes = base::JoinString(subtypes_str, ",");
    result->has_tab_match = input.has_tab_match.value_or(false);
    if (input.associated_keyword.get()) {
      result->associated_keyword =
          base::UTF16ToUTF8(input.associated_keyword->keyword);
    }
    result->keyword = base::UTF16ToUTF8(input.keyword);
    result->duplicates = static_cast<int32_t>(input.duplicate_matches.size());
    result->from_previous = input.from_previous;
    const auto* pedal = OmniboxPedal::FromAction(input.GetPrimaryAction());
    result->pedal_id =
        pedal == nullptr ? 0 : static_cast<int32_t>(pedal->PedalId());
    result->additional_info =
        mojo::ConvertTo<std::vector<mojom::AutocompleteAdditionalInfoPtr>>(
            input.additional_info);
    return result;
  }
};

template <>
struct TypeConverter<mojom::AutocompleteResultsForProviderPtr,
                     scoped_refptr<AutocompleteProvider>> {
  static mojom::AutocompleteResultsForProviderPtr Convert(
      const scoped_refptr<AutocompleteProvider>& input) {
    mojom::AutocompleteResultsForProviderPtr resultsForProvider(
        mojom::AutocompleteResultsForProvider::New());
    resultsForProvider->provider_name = input->GetName();
    resultsForProvider->results =
        mojo::ConvertTo<std::vector<mojom::AutocompleteMatchPtr>>(
            input->matches());
    return resultsForProvider;
  }
};

}  // namespace mojo

OmniboxPageHandler::OmniboxPageHandler(
    Profile* profile,
    mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {
  observation_.Observe(
      AutocompleteControllerEmitter::GetForBrowserContext(profile_));
  ResetController();
}

OmniboxPageHandler::~OmniboxPageHandler() = default;

void OmniboxPageHandler::OnStart(AutocompleteController* controller,
                                 const AutocompleteInput& input) {
  time_omnibox_started_ = base::Time::Now();
  input_ = input;
  page_->HandleNewAutocompleteQuery(controller == controller_.get(),
                                    base::UTF16ToUTF8(input.text()));
}

void OmniboxPageHandler::OnResultChanged(AutocompleteController* controller,
                                         bool default_match_changed) {
  mojom::OmniboxResponsePtr response(mojom::OmniboxResponse::New());
  response->cursor_position = input_.cursor_position();
  response->time_since_omnibox_started_ms =
      (base::Time::Now() - time_omnibox_started_).InMilliseconds();
  response->done = controller->done();
  response->type = AutocompleteInput::TypeToString(input_.type());
  const std::u16string host =
      input_.text().substr(input_.parts().host.begin, input_.parts().host.len);
  response->host = base::UTF16ToUTF8(host);
  bool is_typed_host;
  if (!LookupIsTypedHost(host, &is_typed_host))
    is_typed_host = false;
  response->is_typed_host = is_typed_host;
  response->input_text = base::UTF16ToUTF8(input_.text());

  {
    // Copy to an ACMatches to make conversion easier. Since this isn't
    // performance critical we don't worry about the cost here.
    ACMatches matches(controller->result().begin(), controller->result().end());
    response->combined_results =
        mojo::ConvertTo<std::vector<mojom::AutocompleteMatchPtr>>(matches);
  }
  response->results_by_provider =
      mojo::ConvertTo<std::vector<mojom::AutocompleteResultsForProviderPtr>>(
          controller->providers());

  // Fill AutocompleteMatch::starred.
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  if (bookmark_model) {
    for (size_t i = 0; i < response->combined_results.size(); ++i) {
      response->combined_results[i]->starred = bookmark_model->IsBookmarked(
          GURL(response->combined_results[i]->destination_url));
    }
    for (size_t i = 0; i < response->results_by_provider.size(); ++i) {
      const mojom::AutocompleteResultsForProvider& result_by_provider =
          *response->results_by_provider[i];
      for (size_t j = 0; j < result_by_provider.results.size(); ++j) {
        result_by_provider.results[j]->starred = bookmark_model->IsBookmarked(
            GURL(result_by_provider.results[j]->destination_url));
      }
    }
  }

  // Obtain a vector of all image urls required.
  std::vector<std::string> image_urls;
  for (size_t i = 0; i < response->combined_results.size(); ++i)
    image_urls.push_back(response->combined_results[i]->image);
  for (size_t i = 0; i < response->results_by_provider.size(); ++i) {
    const mojom::AutocompleteResultsForProvider& result_by_provider =
        *response->results_by_provider[i];
    for (size_t j = 0; j < result_by_provider.results.size(); ++j)
      image_urls.push_back(result_by_provider.results[j]->image);
  }

  page_->HandleNewAutocompleteResponse(std::move(response),
                                       controller == controller_.get());

  // Fill in image data
  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);

  for (std::string image_url : image_urls) {
    if (image_url.empty()) {
      continue;
    }
    bitmap_fetcher_service->RequestImage(
        GURL(image_url), base::BindOnce(&OmniboxPageHandler::OnBitmapFetched,
                                        weak_factory_.GetWeakPtr(), image_url));
  }
}

void OmniboxPageHandler::OnBitmapFetched(const std::string& image_url,
                                         const SkBitmap& bitmap) {
  auto data = gfx::Image::CreateFrom1xBitmap(bitmap).As1xPNGBytes();
  std::string base_64;
  base::Base64Encode(base::StringPiece(data->front_as<char>(), data->size()),
                     &base_64);
  const char kDataUrlPrefix[] = "data:image/png;base64,";
  std::string data_url = GURL(kDataUrlPrefix + base_64).spec();
  page_->HandleAnswerImageData(image_url, data_url);
}

bool OmniboxPageHandler::LookupIsTypedHost(const std::u16string& host,
                                           bool* is_typed_host) const {
  history::HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service)
    return false;
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return false;
  *is_typed_host =
      url_db->IsTypedHost(base::UTF16ToUTF8(host), /*scheme=*/nullptr);
  return true;
}

void OmniboxPageHandler::SetClientPage(
    mojo::PendingRemote<mojom::OmniboxPage> page) {
  page_.Bind(std::move(page));
}

void OmniboxPageHandler::StartOmniboxQuery(const std::string& input_string,
                                           bool reset_autocomplete_controller,
                                           int32_t cursor_position,
                                           bool zero_suggest,
                                           bool prevent_inline_autocomplete,
                                           bool prefer_keyword,
                                           const std::string& current_url,
                                           int32_t page_classification) {
  // Reset the controller.  If we don't do this, then the
  // AutocompleteController might inappropriately set its |minimal_changes|
  // variable (or something else) and some providers will short-circuit
  // important logic and return stale results.  In short, we want the
  // actual results to not depend on the state of the previous request.
  if (reset_autocomplete_controller)
    ResetController();
  AutocompleteInput input(
      base::UTF8ToUTF16(input_string), cursor_position,
      static_cast<metrics::OmniboxEventProto::PageClassification>(
          page_classification),
      ChromeAutocompleteSchemeClassifier(profile_));
  GURL current_url_gurl{current_url};
  if (current_url_gurl.is_valid())
    input.set_current_url(current_url_gurl);
  input.set_current_title(base::UTF8ToUTF16(current_url));
  input.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  input.set_prefer_keyword(prefer_keyword);
  if (prefer_keyword)
    input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  input.set_focus_type(
      zero_suggest ? input.text().empty() && current_url_gurl.is_valid()
                         ? metrics::OmniboxFocusType::INTERACTION_CLOBBER
                         : metrics::OmniboxFocusType::INTERACTION_FOCUS
                   : metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  controller_->Start(input);
}

void OmniboxPageHandler::ResetController() {
  controller_ = std::make_unique<AutocompleteController>(
      std::make_unique<ChromeAutocompleteProviderClient>(profile_),
      AutocompleteClassifier::DefaultOmniboxProviders());
  // We will observe our internal AutocompleteController directly, so there's
  // no reason to hook it up to the profile-keyed AutocompleteControllerEmitter.
  controller_->AddObserver(this);
}
