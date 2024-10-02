// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_text_change_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_suggestions_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class ChromeBrowserState;
class GURL;
class WebOmniboxEditModelDelegate;
struct AutocompleteMatch;
@class OmniboxTextFieldIOS;
@protocol OmniboxCommands;

// iOS implementation of OmniBoxView.  Wraps a UITextField and
// interfaces with the rest of the autocomplete system.
class OmniboxViewIOS : public OmniboxView,
                       public OmniboxPopupViewSuggestionsDelegate,
                       public OmniboxTextChangeDelegate,
                       public OmniboxTextAcceptDelegate {
 public:
  // Retains `field`.
  OmniboxViewIOS(OmniboxTextFieldIOS* field,
                 WebOmniboxEditModelDelegate* edit_model_delegate,
                 ChromeBrowserState* browser_state,
                 id<OmniboxCommands> omnibox_focuser);

  ~OmniboxViewIOS() override;

  void SetPopupProvider(OmniboxPopupProvider* provider) {
    popup_provider_ = provider;
  }

  void OnReceiveClipboardURLForOpenMatch(
      const AutocompleteMatch& match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t selected_line,
      base::TimeTicks match_selection_timestamp,
      absl::optional<GURL> optional_gurl);

  void OnReceiveClipboardTextForOpenMatch(
      const AutocompleteMatch& match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t selected_line,
      base::TimeTicks match_selection_timestamp,
      absl::optional<std::u16string> optional_text);

  void OnReceiveClipboardImageForOpenMatch(
      const AutocompleteMatch& match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t selected_line,
      base::TimeTicks match_selection_timestamp,
      absl::optional<gfx::Image> optional_image);

  void OnReceiveImageMatchForOpenMatch(
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t selected_line,
      base::TimeTicks match_selection_timestamp,
      absl::optional<AutocompleteMatch> optional_match);

  // OmniboxView implementation.
  std::u16string GetText() const override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override;
  void RevertAll() override;
  void UpdatePopup() override;
  void OnTemporaryTextMaybeChanged(const std::u16string& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& display_text,
      std::vector<gfx::Range> selections,
      const std::u16string& prefix_autocompletion,
      const std::u16string& inline_autocompletion) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  bool IsImeComposing() const override;
  bool IsIndicatingQueryRefinement() const override;

  // OmniboxView stubs.
  void Update() override {}
  void SetAdditionalText(const std::u16string& text) override {}
  void EnterKeywordModeForDefaultSearchProvider() override {}
  bool IsSelectAll() const override;
  void GetSelectionBounds(std::u16string::size_type* start,
                          std::u16string::size_type* end) const override;
  size_t GetAllSelectionsLength() const override;
  void SelectAll(bool reversed) override {}
  void SetFocus(bool is_user_initiated) override {}
  void ApplyCaretVisibility() override {}
  void OnInlineAutocompleteTextCleared() override {}
  void OnRevertTemporaryText(const std::u16string& display_text,
                             const AutocompleteMatch& match) override {}
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;

  // OmniboxTextChangeDelegate methods

  void OnDidBeginEditing() override;
  bool OnWillChange(NSRange range, NSString* new_text) override;
  void OnDidChange(bool processing_user_input) override;
  void OnWillEndEditing() override;
  void EndEditing() override;
  void OnCopy() override;
  void ClearText() override;
  void WillPaste() override;
  void OnDeleteBackward() override;

  // OmniboxTextAcceptDelegate methods
  void OnAccept() override;

  // OmniboxPopupViewSuggestionsDelegate methods
  void OnPopupDidScroll() override;
  void OnSelectedMatchForAppending(const std::u16string& str) override;
  void OnSelectedMatchForOpening(AutocompleteMatch match,
                                 WindowOpenDisposition disposition,
                                 const GURL& alternate_nav_url,
                                 const std::u16string& pasted_text,
                                 size_t index) override;

  // Updates this edit view to show the proper text, highlight and images.
  void UpdateAppearance();

  // Updates the appearance of popup to have proper text alignment.
  void UpdatePopupAppearance();

  void OnClear();

  // Hide keyboard only.  Used when omnibox popups grab focus but editing isn't
  // complete.
  void HideKeyboard();

  // Focus the omnibox field.  This is used when the omnibox popup copies a
  // search query to the omnibox so the user can modify it further.
  // This does not affect the popup state and is a NOOP if the omnibox is
  // already focused.
  void FocusOmnibox();

  // Returns `true` if AutocompletePopupView is currently open.
  BOOL IsPopupOpen();

 protected:
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override {}

 private:
  void SetEmphasis(bool emphasize, const gfx::Range& range) override {}
  void UpdateSchemeStyle(const gfx::Range& scheme_range) override {}

  // Removes the query refinement chip from the omnibox.
  void RemoveQueryRefinementChip();

  OmniboxTextFieldIOS* field_;

  WebOmniboxEditModelDelegate* edit_model_delegate_;  // weak, owns us
  // Focuser, used to transition the location bar to focused/defocused state as
  // necessary.
  __weak id<OmniboxCommands> omnibox_focuser_;

  State state_before_change_;
  NSString* marked_text_before_change_;
  NSRange current_selection_;
  NSRange old_selection_;

  // TODO(rohitrao): This is a monster hack, needed because closing the popup
  // ends up inadvertently triggering a new round of autocomplete.  Fix the
  // underlying problem, which is that textDidChange: is called when closing the
  // popup, and then remove this hack.  b/5877366.
  BOOL ignore_popup_updates_;

  // Whether the popup was scrolled during this omnibox interaction.
  bool suggestions_list_scrolled_ = false;

  OmniboxPopupProvider* popup_provider_;  // weak

  // Used to cancel clipboard callbacks if this is deallocated;
  base::WeakPtrFactory<OmniboxViewIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_
