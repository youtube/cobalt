// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"

#import <CoreText/CoreText.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <string>

#import "base/command_line.h"
#import "base/ios/device_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/clipboard_provider.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_metrics_helper.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/web_omnibox_edit_model_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/navigation/referrer.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/page_transition_types.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/window_open_disposition.h"
#import "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

#pragma mark - OminboxViewIOS

OmniboxViewIOS::OmniboxViewIOS(OmniboxTextFieldIOS* field,
                               WebOmniboxEditModelDelegate* edit_model_delegate,
                               ChromeBrowserState* browser_state,
                               id<OmniboxCommands> omnibox_focuser)
    : OmniboxView(
          edit_model_delegate,
          edit_model_delegate
              ? std::make_unique<ChromeOmniboxClientIOS>(edit_model_delegate,
                                                         browser_state)
              : nullptr),
      field_(field),
      edit_model_delegate_(edit_model_delegate),
      omnibox_focuser_(omnibox_focuser),
      ignore_popup_updates_(false),
      popup_provider_(nullptr) {
  DCHECK(field_);
}

OmniboxViewIOS::~OmniboxViewIOS() = default;

void OmniboxViewIOS::OnReceiveClipboardURLForOpenMatch(
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t selected_line,
    base::TimeTicks match_selection_timestamp,
    absl::optional<GURL> optional_gurl) {
  if (!optional_gurl) {
    return;
  }

  GURL url = std::move(optional_gurl).value();

  AutocompleteController* controller = model()->autocomplete_controller();

  OmniboxPopupSelection selection(controller->InjectAdHocMatch(
      controller->clipboard_provider()->NewClipboardURLMatch(url)));
  model()->OpenSelection(selection, match_selection_timestamp, disposition);
}

void OmniboxViewIOS::OnReceiveClipboardTextForOpenMatch(
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t selected_line,
    base::TimeTicks match_selection_timestamp,
    absl::optional<std::u16string> optional_text) {
  if (!optional_text) {
    return;
  }

  std::u16string text = std::move(optional_text).value();

  ClipboardProvider* clipboard_provider =
      model()->autocomplete_controller()->clipboard_provider();
  absl::optional<AutocompleteMatch> new_match =
      clipboard_provider->NewClipboardTextMatch(text);

  if (!new_match) {
    return;
  }

  OmniboxPopupSelection selection(
      model()->autocomplete_controller()->InjectAdHocMatch(new_match.value()));
  model()->OpenSelection(selection, match_selection_timestamp, disposition);
}

void OmniboxViewIOS::OnReceiveClipboardImageForOpenMatch(
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t selected_line,
    base::TimeTicks match_selection_timestamp,
    absl::optional<gfx::Image> optional_image) {
  ClipboardProvider* clipboard_provider =
      model()->autocomplete_controller()->clipboard_provider();
  clipboard_provider->NewClipboardImageMatch(
      optional_image,
      base::BindOnce(&OmniboxViewIOS::OnReceiveImageMatchForOpenMatch,
                     weak_ptr_factory_.GetWeakPtr(), disposition,
                     alternate_nav_url, pasted_text, selected_line,
                     match_selection_timestamp));
}

void OmniboxViewIOS::OnReceiveImageMatchForOpenMatch(
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t selected_line,
    base::TimeTicks match_selection_timestamp,
    absl::optional<AutocompleteMatch> optional_match) {
  if (!optional_match) {
    return;
  }
  OmniboxPopupSelection selection(
      model()->autocomplete_controller()->InjectAdHocMatch(
          optional_match.value()));
  model()->OpenSelection(selection, match_selection_timestamp, disposition);
}

std::u16string OmniboxViewIOS::GetText() const {
  return base::SysNSStringToUTF16([field_ displayedText]);
}

void OmniboxViewIOS::SetWindowTextAndCaretPos(const std::u16string& text,
                                              size_t caret_pos,
                                              bool update_popup,
                                              bool notify_text_changed) {
  // Do not call SetUserText() here, as the user has not triggered this change.
  // Instead, set the field's text directly.
  [field_ setText:base::SysUTF16ToNSString(text)];

  NSAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(text)];
  [field_ setText:as userTextLength:[as length]];

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed && model())
    model()->OnChanged();

  SetCaretPos(caret_pos);
}

void OmniboxViewIOS::SetCaretPos(size_t caret_pos) {
  DCHECK(caret_pos <= field_.text.length || caret_pos == 0);
  UITextPosition* start = field_.beginningOfDocument;
  UITextPosition* newPosition =
      [field_ positionFromPosition:start offset:caret_pos];
  field_.selectedTextRange =
      [field_ textRangeFromPosition:newPosition toPosition:newPosition];
}

void OmniboxViewIOS::RevertAll() {
  ignore_popup_updates_ = true;
  OmniboxView::RevertAll();
  ignore_popup_updates_ = false;
}

void OmniboxViewIOS::UpdatePopup() {
  if (model())
    model()->SetInputInProgress(true);

  if (model() && !model()->has_focus())
    return;

  // Prevent inline-autocomplete if the IME is currently composing or if the
  // cursor is not at the end of the text.
  bool prevent_inline_autocomplete =
      IsImeComposing() ||
      NSMaxRange(current_selection_) != [field_.text length];
  if (model())
    model()->StartAutocomplete(current_selection_.length != 0,
                               prevent_inline_autocomplete);

  UpdatePopupAppearance();
}

void OmniboxViewIOS::UpdatePopupAppearance() {
  if (!popup_provider_) {
    return;
  }
  popup_provider_->SetTextAlignment([field_ bestTextAlignment]);
  popup_provider_->SetSemanticContentAttribute(
      [field_ bestSemanticContentAttribute]);
}

void OmniboxViewIOS::OnTemporaryTextMaybeChanged(
    const std::u16string& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  SetWindowTextAndCaretPos(display_text, display_text.size(), false, false);
  if (model())
    model()->OnChanged();
}

void OmniboxViewIOS::OnInlineAutocompleteTextMaybeChanged(
    const std::u16string& display_text,
    std::vector<gfx::Range> selections,
    const std::u16string& prefix_autocompletion,
    const std::u16string& inline_autocompletion) {
  if (display_text == GetText())
    return;

  NSAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(display_text)];
  // TODO(crbug.com/1062446): This `user_text_length` calculation  isn't
  //  accurate when there's prefix autocompletion. This should be addressed
  //  before we experiment with prefix autocompletion on iOS.
  size_t user_text_length = display_text.size() - inline_autocompletion.size();
  [field_ setText:as userTextLength:user_text_length];
}

void OmniboxViewIOS::OnBeforePossibleChange() {
  GetState(&state_before_change_);
  marked_text_before_change_ = [[field_ markedText] copy];
}

bool OmniboxViewIOS::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  State new_state;
  GetState(&new_state);
  // Manually update the selection state after calling GetState().
  new_state.sel_start = current_selection_.location;
  new_state.sel_end = current_selection_.location + current_selection_.length;

  OmniboxView::StateChanges state_changes =
      GetStateChanges(state_before_change_, new_state);

  // iOS does not supports KeywordProvider, so never allow keyword UI changes.
  const bool something_changed =
      model() &&
      model()->OnAfterPossibleChange(state_changes, allow_keyword_ui_change);

  if (model())
    model()->OnChanged();

  // TODO(justincohen): Find a different place to call this. Give the omnibox
  // a chance to update the alignment for a text direction change.
  [field_ updateTextDirection];
  return something_changed;
}

bool OmniboxViewIOS::IsImeComposing() const {
  return [field_ markedTextRange] != nil;
}

bool OmniboxViewIOS::IsIndicatingQueryRefinement() const {
  return false;
}

bool OmniboxViewIOS::IsSelectAll() const {
  return false;
}

void OmniboxViewIOS::GetSelectionBounds(std::u16string::size_type* start,
                                        std::u16string::size_type* end) const {
  if ([field_ isFirstResponder]) {
    NSRange selected_range = [field_ selectedNSRange];
    *start = selected_range.location;
    *end = selected_range.location + selected_range.length;
  } else {
    *start = *end = 0;
  }
}

size_t OmniboxViewIOS::GetAllSelectionsLength() const {
  return 0;
}

gfx::NativeView OmniboxViewIOS::GetNativeView() const {
  return nullptr;
}

gfx::NativeView OmniboxViewIOS::GetRelativeWindowForPopup() const {
  return nullptr;
}

void OmniboxViewIOS::OnDidBeginEditing() {
  // If Open from Clipboard offers a suggestion, the popup may be opened when
  // `OnSetFocus` is called on the model. The state of the popup is saved early
  // to ignore that case.
  DCHECK(popup_provider_);
  bool popup_was_open_before_editing_began = popup_provider_->IsPopupOpen();

  // Make sure the omnibox popup's semantic content attribute is set correctly.
  popup_provider_->SetSemanticContentAttribute(
      [field_ bestSemanticContentAttribute]);

  OnBeforePossibleChange();

  if (model()) {
    // In the case where the user taps the fakebox on the Google landing page,
    // the focus source is already set to FAKEBOX. Otherwise, set it to OMNIBOX.
    if (model()->focus_source() != OmniboxFocusSource::FAKEBOX)
      model()->set_focus_source(OmniboxFocusSource::OMNIBOX);

    model()->StartZeroSuggestRequest();
    model()->OnSetFocus(/*control_down=*/false);
  }

  // If the omnibox is displaying a URL and the popup is not showing, set the
  // field into pre-editing state.  If the omnibox is displaying search terms,
  // leave the default behavior of positioning the cursor at the end of the
  // text.  If the popup is already open, that means that the omnibox is
  // regaining focus after a popup scroll took focus away, so the pre-edit
  // behavior should not be invoked.
  if (!popup_was_open_before_editing_began)
    [field_ enterPreEditState];

  // `edit_model_delegate_` is only forwarding the call to the BVC. This should
  // only happen when the omnibox is being focused and it starts showing the
  // popup; if the popup was already open, no need to call this.
  if (!popup_was_open_before_editing_began)
    edit_model_delegate_->OnSetFocus();
}

void OmniboxViewIOS::OnWillEndEditing() {
  // On iPad, this will be called when the "hide keyboard" button is pressed
  // on the software keyboard.
  // This will also be called if -resignFirstResponder is called
  // programmatically. On phone, the omnibox may still be editing when
  // the popup is open, so the Cancel button calls OnWillEndEditing.
  if (!base::FeatureList::IsEnabled(kEnableSuggestionsScrollingOnIPad) &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // This should be equivalent to tapping the typing
    // shield and should defocus the omnibox, transition the location bar to
    // steady view, and close the popup.
    [omnibox_focuser_ cancelOmniboxEdit];
  }
}

bool OmniboxViewIOS::OnWillChange(NSRange range, NSString* new_text) {
  bool ok_to_change = true;

  if ([field_ isPreEditing]) {
    [field_ setClearingPreEditText:YES];

    if (!base::FeatureList::IsEnabled(kIOSNewOmniboxImplementation)) {
      // Exit the pre-editing state in OnWillChange() instead of OnDidChange(),
      // as that allows IME to continue working.  The following code selects the
      // text as if the pre-edit fake selection was real.
      [field_ exitPreEditState];

      field_.text = @"";
    }

    // Reset `range` to be of zero-length at location zero, as the field will be
    // now cleared.
    range = NSMakeRange(0, 0);
  }

  // Figure out the old and current (new) selections.  Assume the new selection
  // will be of zero-length, located at the end of `new_text`.
  NSRange old_range = range;
  NSRange new_range = NSMakeRange(range.location + [new_text length], 0);

  // We may need to fix up the old and new ranges in the case where autocomplete
  // text was showing.  If there is autocomplete text, assume it was selected.
  // If the change is deleting one character from the end of the actual text,
  // disallow the change, but clear the autocomplete text and call OnDidChange
  // directly.  If there is autocomplete text AND a text field selection, or if
  // the user entered multiple characters, clear the autocomplete text and
  // pretend it never existed.
  if ([field_ hasAutocompleteText]) {
    bool adding_text = (range.length < [new_text length]);
    bool deleting_text = (range.length > [new_text length]);

    if (adding_text) {
      // TODO(rohitrao): What about cases where [new_text length] > 1?  This
      // could happen if an IME completion inserts multiple characters at once,
      // or if the user pastes some text in.  Let's loosen this test to allow
      // multiple characters, as long as the "old range" ends at the end of the
      // permanent text.
      NSString* userText = field_.text;
      if (base::FeatureList::IsEnabled(kIOSNewOmniboxImplementation)) {
        userText = field_.userText;
      }

      if (new_text.length == 1 && range.location == userText.length) {
        old_range =
            NSMakeRange(field_.text.length, field_.autocompleteText.length);
      }
    } else if (deleting_text) {
      NSString* userText = field_.text;
      if (base::FeatureList::IsEnabled(kIOSNewOmniboxImplementation)) {
        userText = field_.userText;
      }

      if ([new_text length] == 0 && range.location == [userText length] - 1) {
        ok_to_change = false;
      }
    }
  }

  // Update variables needed by OnDidChange() and GetState().
  old_selection_ = old_range;
  current_selection_ = new_range;

  // Store the displayed text.  Older version of Chrome used to clear the
  // autocomplete text here as well, but on iOS7 doing this causes the inline
  // autocomplete text to flicker, so the call was moved to the start on
  // OnDidChange().
  GetState(&state_before_change_);
  // Manually update the selection state after calling GetState().
  state_before_change_.sel_start = old_selection_.location;
  state_before_change_.sel_end =
      old_selection_.location + old_selection_.length;

  if (!ok_to_change) {
    // Force a change in the autocomplete system, since we won't get an
    // OnDidChange() message.
    OnDidChange(true);
  }

  return ok_to_change;
}

void OmniboxViewIOS::OnDidChange(bool processing_user_event) {
  if (base::FeatureList::IsEnabled(kIOSNewOmniboxImplementation)) {
    if (field_.isPreEditing)
      [field_ exitPreEditState];
  }

  // Sanitize pasted text.
  if (model() && model()->is_pasting()) {
    std::u16string pastedText = base::SysNSStringToUTF16(field_.text);
    std::u16string newText = OmniboxView::SanitizeTextForPaste(pastedText);
    if (pastedText != newText) {
      [field_ setText:base::SysUTF16ToNSString(newText)];
    }
  }

  // Clear the autocomplete text, since the omnibox model does not expect to see
  // it in OnAfterPossibleChange().  Clearing the text here should not cause
  // flicker as the UI will not get a chance to redraw before the new
  // autocomplete text is set by the model.
  [field_ clearAutocompleteText];
  [field_ setClearingPreEditText:NO];

  // Generally do not notify the autocomplete system of a text change unless the
  // change was a direct result of a user event.  One exception is if the marked
  // text changed, which could happen through a delayed IME recognition
  // callback.
  bool proceed_without_user_event = false;

  // The IME exception does not work for Korean text, because Korean does not
  // seem to ever have marked text.  It simply replaces or modifies previous
  // characters as you type.  Always proceed without user input if the
  // Korean keyboard is currently active.
  NSString* current_language = [[field_ textInputMode] primaryLanguage];

  if ([current_language hasPrefix:@"ko-"]) {
    proceed_without_user_event = true;
  } else {
    NSString* current_marked_text = [field_ markedText];

    // The IME exception kicks in if the current marked text is not equal to the
    // previous marked text.  Two nil strings should be considered equal, so
    // There is logic to avoid calling into isEqual: in that case.
    proceed_without_user_event =
        (marked_text_before_change_ || current_marked_text) &&
        ![current_marked_text isEqual:marked_text_before_change_];
  }

  if (!processing_user_event && !proceed_without_user_event)
    return;

  // TODO(crbug.com/564599): OnAfterPossibleChange() now takes an argument. It
  // use to not take an argument and was defaulting to false, so as it is
  // unclear what the correct value is, using what was that before seems
  // consistent.
  OnAfterPossibleChange(false);
  OnBeforePossibleChange();
}

void OmniboxViewIOS::OnAccept() {
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  if (model()) {
    model()->OpenSelection();
  }
  RevertAll();
}

void OmniboxViewIOS::OnClear() {
  [field_ clearAutocompleteText];
  [field_ exitPreEditState];
}

void OmniboxViewIOS::OnCopy() {
  NSString* selectedText = nil;
  NSInteger start_location = 0;
  if ([field_ isPreEditing]) {
    if (base::FeatureList::IsEnabled(kIOSNewOmniboxImplementation)) {
      selectedText = field_.text;
    } else {
      selectedText = field_.preEditText;
    }
    start_location = 0;
  } else {
    UITextRange* selected_range = [field_ selectedTextRange];
    selectedText = [field_ textInRange:selected_range];
    UITextPosition* start = [field_ beginningOfDocument];
    // The following call to `-offsetFromPosition:toPosition:` gives the offset
    // in terms of the number of "visible characters."  The documentation does
    // not specify whether this means glyphs or UTF16 chars.  This does not
    // matter for the current implementation of AdjustTextForCopy(), but it may
    // become an issue at some point.
    start_location =
        [field_ offsetFromPosition:start toPosition:[selected_range start]];
  }
  std::u16string text = base::SysNSStringToUTF16(selectedText);

  GURL url;
  bool write_url = false;
  // Model can be nullptr in tests.
  if (model())
    model()->AdjustTextForCopy(start_location, &text, &url, &write_url);

  // Create the pasteboard item manually because the pasteboard expects a single
  // item with multiple representations.  This is expressed as a single
  // NSDictionary with multiple keys, one for each representation.
  NSMutableDictionary* item = [NSMutableDictionary dictionaryWithCapacity:2];
  [item setObject:base::SysUTF16ToNSString(text)
           forKey:UTTypePlainText.identifier];

  if (write_url)
    [item setObject:net::NSURLWithGURL(url) forKey:UTTypeURL.identifier];

  StoreItemInPasteboard(item);
}

void OmniboxViewIOS::WillPaste() {
  if (model())
    model()->OnPaste();

  [field_ exitPreEditState];
}

void OmniboxViewIOS::UpdateAppearance() {
  // If Siri is thinking, treat that as user input being in progress.  It is
  // unsafe to modify the text field while voice entry is pending.
  if (model() && model()->ResetDisplayTexts()) {
    // Revert everything to the baseline look.
    RevertAll();
  } else if (model() && !model()->has_focus()) {
    // Even if the change wasn't "user visible" to the model, it still may be
    // necessary to re-color to the URL string.  Only do this if the omnibox is
    // not currently focused.
    NSAttributedString* as = [[NSMutableAttributedString alloc]
        initWithString:base::SysUTF16ToNSString(
                           model()->GetPermanentDisplayText())];
    [field_ setText:as userTextLength:[as length]];
  }
}

void OmniboxViewIOS::OnDeleteBackward() {
  if (field_.text.length == 0) {
    // If the user taps backspace while the pre-edit text is showing,
    // OnWillChange is invoked before this method and sets the text to an empty
    // string, so use the `clearingPreEditText` to determine if the chip should
    // be cleared or not.
    if ([field_ clearingPreEditText]) {
      // In the case where backspace is tapped while in pre-edit mode,
      // OnWillChange is called but OnDidChange is never called so ensure the
      // clearingPreEditText flag is set to false again.
      [field_ setClearingPreEditText:NO];
      // Explicitly set the input-in-progress flag. Normally this is set via
      // in model()->OnAfterPossibleChange, but in this case the text has been
      // set to the empty string by OnWillChange so when OnAfterPossibleChange
      // checks if the text has changed it does not see any difference so it
      // never sets the input-in-progress flag.
      if (model())
        model()->SetInputInProgress(YES);
    } else {
      RemoveQueryRefinementChip();
    }
  }
}

void OmniboxViewIOS::ClearText() {
  // Ensure omnibox is first responder. This will bring up the keyboard so the
  // user can start typing a new query.
  if (![field_ isFirstResponder])
    [field_ becomeFirstResponder];
  if (field_.text.length == 0) {
    // If `field_` is empty, remove the query refinement chip.
    RemoveQueryRefinementChip();
  } else {
    // Otherwise, just remove the text in the omnibox.
    // Calling -[UITextField setText:] does not trigger
    // -[id<UITextFieldDelegate> textDidChange] so it must be called explicitly.
    OnClear();
    [field_ setText:@""];
    OnDidChange(YES);
  }
  // Calling OnDidChange() can trigger a scroll event, which removes focus from
  // the omnibox.
  [field_ becomeFirstResponder];
}

void OmniboxViewIOS::RemoveQueryRefinementChip() {
  edit_model_delegate_->OnChanged();
}

void OmniboxViewIOS::EndEditing() {
  if (model() && model()->has_focus()) {
    CloseOmniboxPopup();

    RecordSuggestionsListScrolled(model()->GetPageClassification(),
                                  suggestions_list_scrolled_);
    suggestions_list_scrolled_ = false;

    model()->OnWillKillFocus();
    model()->OnKillFocus();
    if ([field_ isPreEditing])
      [field_ exitPreEditState];

    // The controller looks at the current pre-edit state, so the call to
    // OnKillFocus() must come after exiting pre-edit.
    edit_model_delegate_->OnKillFocus();

    // Blow away any in-progress edits.
    RevertAll();
    DCHECK(![field_ hasAutocompleteText]);
  }
}

void OmniboxViewIOS::HideKeyboard() {
  [field_ resignFirstResponder];
}

void OmniboxViewIOS::FocusOmnibox() {
  [field_ becomeFirstResponder];
}

BOOL OmniboxViewIOS::IsPopupOpen() {
  if (!popup_provider_) {
    return NO;
  }
  return popup_provider_->IsPopupOpen();
}

int OmniboxViewIOS::GetOmniboxTextLength() const {
  return [field_ displayedText].length;
}

#pragma mark - OmniboxPopupViewSuggestionsDelegate

void OmniboxViewIOS::OnPopupDidScroll() {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET ||
      base::FeatureList::IsEnabled(kEnableSuggestionsScrollingOnIPad)) {
    this->HideKeyboard();
  }
  suggestions_list_scrolled_ = true;
}

void OmniboxViewIOS::OnSelectedMatchForAppending(const std::u16string& str) {
  // Exit preedit state and append the match. Refocus if necessary.
  if ([field_ isPreEditing])
    [field_ exitPreEditState];
  this->SetUserText(str);
  // Calling setText: does not trigger UIControlEventEditingChanged, so
  // trigger that manually.
  [field_ sendActionsForControlEvents:UIControlEventEditingChanged];
  this->FocusOmnibox();
}

void OmniboxViewIOS::OnSelectedMatchForOpening(
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t index) {
  const auto match_selection_timestamp = base::TimeTicks();
  AutocompleteController* controller = model()->autocomplete_controller();

  // Sometimes the match provided does not correspond to the autocomplete
  // result match specified by `index`. Most Visited Tiles, for example,
  // provide ad hoc matches that are not in the result at all.
  if (index >= controller->result().size() ||
      controller->result().match_at(index).destination_url !=
          match.destination_url) {
    OmniboxPopupSelection selection(controller->InjectAdHocMatch(match));
    model()->OpenSelection(selection, match_selection_timestamp, disposition);
    return;
  }

  // Fill in clipboard matches if they don't have a destination URL.
  if (match.destination_url.is_empty()) {
    if (match.type == AutocompleteMatchType::CLIPBOARD_URL) {
      ClipboardRecentContent* clipboard_recent_content =
          ClipboardRecentContent::GetInstance();
      clipboard_recent_content->GetRecentURLFromClipboard(base::BindOnce(
          &OmniboxViewIOS::OnReceiveClipboardURLForOpenMatch,
          weak_ptr_factory_.GetWeakPtr(), match, disposition, alternate_nav_url,
          pasted_text, index, match_selection_timestamp));
      return;
    } else if (match.type == AutocompleteMatchType::CLIPBOARD_TEXT) {
      ClipboardRecentContent* clipboard_recent_content =
          ClipboardRecentContent::GetInstance();
      clipboard_recent_content->GetRecentTextFromClipboard(base::BindOnce(
          &OmniboxViewIOS::OnReceiveClipboardTextForOpenMatch,
          weak_ptr_factory_.GetWeakPtr(), match, disposition, alternate_nav_url,
          pasted_text, index, match_selection_timestamp));
      return;
    } else if (match.type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
      ClipboardRecentContent* clipboard_recent_content =
          ClipboardRecentContent::GetInstance();
      clipboard_recent_content->GetRecentImageFromClipboard(base::BindOnce(
          &OmniboxViewIOS::OnReceiveClipboardImageForOpenMatch,
          weak_ptr_factory_.GetWeakPtr(), match, disposition, alternate_nav_url,
          pasted_text, index, match_selection_timestamp));
      return;
    }
  }
  model()->OpenSelection(OmniboxPopupSelection(index),
                         match_selection_timestamp, disposition);
}
