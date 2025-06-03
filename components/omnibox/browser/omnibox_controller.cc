// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "ui/gfx/geometry/rect.h"

OmniboxController::OmniboxController(OmniboxView* view,
                                     std::unique_ptr<OmniboxClient> client)
    : client_(std::move(client)),
      edit_model_(std::make_unique<OmniboxEditModel>(
          /*omnibox_controller=*/this,
          view)),
      autocomplete_controller_(std::make_unique<AutocompleteController>(
          client_->CreateAutocompleteProviderClient(),
          AutocompleteClassifier::DefaultOmniboxProviders())) {
  // Directly observe omnibox's `AutocompleteController` instance - i.e., when
  // `view` is provided in the constructor. In the case of realbox - i.e., when
  // `view` is not provided in the constructor - `RealboxHandler` directly
  // observes the `AutocompleteController` instance itself.
  if (view) {
    autocomplete_controller_->AddObserver(this);
  }

  // Register the `AutocompleteController` with `AutocompleteControllerEmitter`.
  if (auto* emitter = client_->GetAutocompleteControllerEmitter()) {
    autocomplete_controller_->AddObserver(emitter);
  }

  if (PrefService* prefs = client_->GetPrefs()) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        omnibox::kSuggestionGroupVisibility,
        base::BindRepeating(
            &OmniboxController::OnSuggestionGroupVisibilityPrefChanged,
            base::Unretained(this)));
  }
}

OmniboxController::~OmniboxController() = default;

void OmniboxController::StartAutocomplete(
    const AutocompleteInput& input) const {
  TRACE_EVENT0("omnibox", "OmniboxController::StartAutocomplete");
  ClearPopupKeywordMode();

  // We don't explicitly clear OmniboxPopupModel::manually_selected_match, as
  // Start ends up invoking OmniboxPopupModel::OnResultChanged which clears it.
  autocomplete_controller_->Start(input);
}

void OmniboxController::OnResultChanged(AutocompleteController* controller,
                                        bool default_match_changed) {
  TRACE_EVENT0("omnibox", "OmniboxController::OnResultChanged");
  DCHECK(controller == autocomplete_controller_.get());

  const bool was_open = edit_model_->PopupIsOpen();
  if (default_match_changed) {
    // The default match has changed, we need to let the OmniboxEditModel know
    // about new inline autocomplete text (blue highlight).
    if (auto* match = result().default_match()) {
      edit_model_->OnCurrentMatchChanged();
    } else {
      edit_model_->OnPopupResultChanged();
      edit_model_->OnPopupDataChanged(
          std::u16string(),
          /*is_temporary_text=*/false, std::u16string(), std::u16string(),
          std::u16string(), false, std::u16string(), AutocompleteMatch());
    }
  } else {
    edit_model_->OnPopupResultChanged();
  }

  if (was_open && !edit_model_->PopupIsOpen()) {
    // Accept the temporary text as the user text, because it makes little sense
    // to have temporary text when the popup is closed.
    edit_model_->AcceptTemporaryTextAsUserText();
    // Closing the popup can change the default suggestion. This usually occurs
    // when it's unclear whether the input represents a search or URL; e.g.,
    // 'a.com/b c' or when title autocompleting. Clear the additional text to
    // avoid suggesting the omnibox contains a URL suggestion when that may no
    // longer be the case; i.e. when the default suggestion changed from a URL
    // to a search suggestion upon closing the popup.
    edit_model_->ClearAdditionalText();
  }

  // Note: The client outlives |this|, so bind a weak pointer to the callback
  // passed in to eliminate the potential for crashes on shutdown.
  // `should_preload` is set to `controller->done()` as prerender may only want
  // to start preloading a result after all Autocomplete results are ready.
  client_->OnResultChanged(
      result(), default_match_changed, /*should_preload=*/controller->done(),
      base::BindRepeating(&OmniboxController::SetRichSuggestionBitmap,
                          weak_ptr_factory_.GetWeakPtr()));
}

void OmniboxController::ClearPopupKeywordMode() const {
  TRACE_EVENT0("omnibox", "OmniboxController::ClearPopupKeywordMode");
  if (edit_model_->PopupIsOpen()) {
    OmniboxPopupSelection selection = edit_model_->GetPopupSelection();
    if (selection.state == OmniboxPopupSelection::KEYWORD_MODE) {
      selection.state = OmniboxPopupSelection::NORMAL;
      edit_model_->SetPopupSelection(selection);
    }
  }
}

std::u16string OmniboxController::GetHeaderForSuggestionGroup(
    omnibox::GroupId suggestion_group_id) const {
  return result().GetHeaderForSuggestionGroup(suggestion_group_id);
}

bool OmniboxController::IsSuggestionGroupHidden(
    omnibox::GroupId suggestion_group_id) const {
  PrefService* prefs = client_->GetPrefs();
  return prefs && result().IsSuggestionGroupHidden(prefs, suggestion_group_id);
}

void OmniboxController::SetSuggestionGroupHidden(
    omnibox::GroupId suggestion_group_id,
    bool hidden) const {
  if (PrefService* prefs = client_->GetPrefs()) {
    result().SetSuggestionGroupHidden(prefs, suggestion_group_id, hidden);
  }
}

void OmniboxController::SetRichSuggestionBitmap(int result_index,
                                                const SkBitmap& bitmap) {
  edit_model_->SetPopupRichSuggestionBitmap(result_index, bitmap);
}

void OmniboxController::OnSuggestionGroupVisibilityPrefChanged() {
  for (size_t i = 0; i < result().size(); ++i) {
    const AutocompleteMatch& match = result().match_at(i);
    bool suggestion_group_hidden =
        match.suggestion_group_id.has_value() &&
        IsSuggestionGroupHidden(match.suggestion_group_id.value());
    edit_model_->SetPopupSuggestionGroupVisibility(i, suggestion_group_hidden);
  }
}
