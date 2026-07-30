// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"

OmniboxPopupViewFullWebUI::OmniboxPopupViewFullWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate)
    : OmniboxPopupViewWebUI(
          omnibox_view,
          controller,
          location_bar,
          presenter_delegate,
          std::make_unique<OmniboxPopupFullPresenter>(location_bar,
                                                      presenter_delegate,
                                                      controller)) {}

OmniboxPopupViewFullWebUI::~OmniboxPopupViewFullWebUI() = default;

void OmniboxPopupViewFullWebUI::UpdatePopupAppearance() {
  // Intentional no-op. Content updates are handled via `SyncNativeStateToWebUI`
  // called directly from specific events (focus, tab switch).
}

void OmniboxPopupViewFullWebUI::SyncNativeStateToWebUI(bool is_double_click) {
  controller()->edit_model()->ResetDisplayTexts();
  if (auto* popup_handler = GetPopupHandler()) {
    bool user_input_in_progress =
        controller()->edit_model()->user_input_in_progress();
    std::u16string text =
        user_input_in_progress
            ? controller()->edit_model()->user_text()
            : controller()->edit_model()->GetPermanentDisplayText();
    gfx::Range selection = gfx::Range(0, text.length());
    if (is_double_click) {
      if (auto* omnibox_view_views =
              static_cast<OmniboxViewViews*>(omnibox_view_)) {
        selection = omnibox_view_views->GetSelectedRange();
      }
    }
    const std::u16string full_url =
        controller()->client()->GetFormattedFullURL();

    // `last_sent_text_` is null after a state reset (e.g., tab switch).
    // Otherwise, check if `text` or `selection` has diverged.
    bool text_changed = !last_sent_text_ || text != *last_sent_text_;
    bool selection_changed = selection != popup_handler->latest_selection();

    if (text_changed || selection_changed) {
      // TODO(crbug.com/497883783): Consider adding a dedicated
      // `SetSelectionRange` IPC method so that when only the selection
      // changes (e.g. during double clicks or mouse dragging), we do not push
      // the input text and risk resetting DOM input state or scroll position.
      popup_handler->SetInputState(base::UTF16ToUTF8(text), selection,
                                   user_input_in_progress, is_double_click,
                                   base::UTF16ToUTF8(full_url));
      last_sent_text_ = text;
    }
  }
}

void OmniboxPopupViewFullWebUI::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

  auto* edit_model = controller()->edit_model();
  const OmniboxEditModel::State default_state =
      edit_model->GetStateForTabSwitch();
  std::unique_ptr<OmniboxEditModel::State> state;

  // `GetStateForTabSwitch()` reads `OmniboxView::GetText()`, which polls the
  // inactive native Views textfield. We override `user_text` here to prevent
  // wiping out uncommitted WebUI drafts on tab switches.
  if (edit_model->user_input_in_progress()) {
    std::u16string override_text = edit_model->user_text();
    state = std::make_unique<OmniboxEditModel::State>(
        /*user_input_in_progress=*/true, override_text, default_state.keyword,
        default_state.keyword_placeholder, default_state.keyword_state,
        default_state.keyword_mode_entry_method, default_state.focus_state,
        default_state.autocomplete_input);
  } else {
    state = std::make_unique<OmniboxEditModel::State>(default_state);
  }

  // The text input element lives in WebUI, so the native view hierarchy does
  // not track active text selection. Fetch it directly from the WebUI handler.
  gfx::Range selection;
  if (auto* popup_handler = GetPopupHandler()) {
    selection = popup_handler->latest_selection();
  }

  // We only need to sync the active selection because the WebUI input retains
  // its selection across blur and focus. `saved_selection_for_focus_change`
  // is set to `InvalidRange` because it is never used in WebUI.
  tab->SetUserData(
      OmniboxTabHelper::kOmniboxStateKey,
      std::make_unique<OmniboxState>(
          *state, selection,
          /*saved_selection_for_focus_change=*/gfx::Range::InvalidRange()));
}

void OmniboxPopupViewFullWebUI::OnTabChanged(content::WebContents* contents) {
  last_sent_text_.reset();

  OmniboxPopupState target_popup_state;
  auto* state = static_cast<OmniboxState*>(
      contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  if (state) {
    // Restore the saved state for the tab.
    controller()->edit_model()->RestoreState(&state->model_state);
    target_popup_state = (state->model_state.user_input_in_progress ||
                          state->selection != gfx::Range(0, 0))
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
  } else {
    // No saved state: revert to default and re-evaluate popup visibility based
    // on current focus.
    controller()->edit_model()->Revert();
    controller()->edit_model()->OnChanged();
    target_popup_state = controller()->edit_model()->has_focus()
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
  }

  // TODO(b/504668582): Fix flicker that occurs when switching between two tabs
  //   that have an Omnibox with text.
  controller()->popup_state_manager()->SetPopupState(target_popup_state);

  // Request focus before pushing content state so our `SetInputState` IPC
  // overrides any OS-default focus selection (such as macOS Select-All).
  if (target_popup_state == OmniboxPopupState::kFull) {
    presenter()->RequestFocus();
  }

  // Push the restored state to the WebUI handler so it can render the
  // correct text and selection range for the newly selected tab.
  if (auto* popup_handler = GetPopupHandler()) {
    bool user_input_in_progress =
        state ? state->model_state.user_input_in_progress : false;
    std::u16string text =
        user_input_in_progress
            ? state->model_state.user_text
            : controller()->edit_model()->GetPermanentDisplayText();
    gfx::Range selection = state ? state->selection : gfx::Range(0, 0);
    const std::u16string full_url =
        controller()->client()->GetFormattedFullURL();
    popup_handler->SetInputState(
        base::UTF16ToUTF8(text), selection, user_input_in_progress,
        /*is_double_click=*/false, base::UTF16ToUTF8(full_url));
    last_sent_text_ = text;
  }
}

void OmniboxPopupViewFullWebUI::OnFocus() {
  bool changed = controller()->popup_state_manager()->popup_state() !=
                 OmniboxPopupState::kFull;

  if (changed) {
    // Invalidate the cache when transitioning to a visible state to force a
    // fresh `SetInputState` IPC. This prevents the WebUI from rendering stale
    // text if the state changed while it was hidden and detached (e.g., tab
    // switching).
    last_sent_text_.reset();
  }

  // Update the state first. This triggers `presenter()->Show()` to re-attach
  // the WebContents before we push the subsequent DOM updates.
  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kFull);

  if (changed) {
    SyncNativeStateToWebUI(/*is_double_click=*/false);
  }
}

OmniboxPopupHandler* OmniboxPopupViewFullWebUI::GetPopupHandler() {
  if (!presenter() || !presenter()->GetWebUIContent()) {
    return nullptr;
  }
  auto* contents_wrapper = presenter()->GetWebUIContent()->contents_wrapper();
  auto* webui_controller =
      contents_wrapper ? contents_wrapper->GetWebUIController() : nullptr;
  auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller);
  return popup_ui ? popup_ui->popup_handler() : nullptr;
}
