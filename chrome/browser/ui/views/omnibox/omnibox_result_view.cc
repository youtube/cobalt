// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"

#include <limits.h>

#include <algorithm>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/browser/ui/views/omnibox/remove_suggestion_bubble.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/atl.h"
#endif

namespace {

class OmniboxRemoveSuggestionButton : public views::ImageButton {
 public:
  METADATA_HEADER(OmniboxRemoveSuggestionButton);
  explicit OmniboxRemoveSuggestionButton(PressedCallback callback)
      : ImageButton(std::move(callback)) {
    views::ConfigureVectorImageButton(this);

    SetAnimationDuration(base::TimeDelta());
    views::InkDrop::Get(this)->GetInkDrop()->SetHoverHighlightFadeDuration(
        base::TimeDelta());

    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Although this appears visually as a button, expose as a list box option
    // so that it matches the other options within its list box container.
    node_data->role = ax::mojom::Role::kListBoxOption;
    node_data->SetNameChecked(
        l10n_util::GetStringUTF16(IDS_ACC_REMOVE_SUGGESTION_BUTTON));
  }
};

BEGIN_METADATA(OmniboxRemoveSuggestionButton, views::ImageButton)
END_METADATA

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultSelectionIndicator

class OmniboxResultSelectionIndicator : public views::View {
 public:
  METADATA_HEADER(OmniboxResultSelectionIndicator);

  const bool cr2023_expanded_state_colors_enabled =
      features::GetChromeRefresh2023Level() ==
          features::ChromeRefresh2023Level::kLevel2 ||
      base::FeatureList::IsEnabled(omnibox::kExpandedStateColors);
  const int kStrokeThickness = cr2023_expanded_state_colors_enabled ? 4 : 3;

  explicit OmniboxResultSelectionIndicator(OmniboxResultView* result_view)
      : result_view_(result_view) {
    SetPreferredSize(gfx::Size(kStrokeThickness, 0));
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    SkPath path = GetPath();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(
        GetColorProvider()->GetColor(kColorOmniboxResultsFocusIndicator));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(path, flags);
  }

 private:
  // Pointer to the parent view.
  const raw_ptr<OmniboxResultView> result_view_;

  // The focus bar is a straight vertical line with half-rounded endcaps. Since
  // this geometry is nontrivial to represent using primitives, it's instead
  // represented using a fill path. This matches the style and implementation
  // used in Tab Groups.
  SkPath GetPath() const {
    SkPath path;

    path.moveTo(0, 0);
    path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
               SkPathDirection::kCW, kStrokeThickness, kStrokeThickness);
    path.lineTo(kStrokeThickness, height() - kStrokeThickness);
    path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
               SkPathDirection::kCW, 0, height());
    path.close();

    return path;
  }
};

BEGIN_METADATA(OmniboxResultSelectionIndicator, views::View)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, public:

OmniboxResultView::OmniboxResultView(OmniboxPopupViewViews* popup_view,
                                     OmniboxEditModel* model,
                                     size_t model_index)
    : popup_view_(popup_view),
      model_(model),
      model_index_(model_index),
      // Using base::Unretained is correct here. 'this' outlives the callback.
      mouse_enter_exit_handler_(
          base::BindRepeating(&OmniboxResultView::UpdateHoverState,
                              base::Unretained(this))) {
  CHECK_GE(model_index, 0u);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  suggestion_container_ = AddChildView(std::make_unique<views::View>());
  suggestion_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  mouse_enter_exit_handler_.ObserveMouseEnterExitOn(suggestion_container_);

  // TODO(olesiamarukhno): Consider making it a decoration instead of separate
  // view (painting it in a layer).
  selection_indicator_ = suggestion_container_->AddChildView(
      std::make_unique<OmniboxResultSelectionIndicator>(this));

  views::View* suggestion_button_container =
      suggestion_container_->AddChildView(std::make_unique<views::View>());
  suggestion_button_container
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  suggestion_button_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  suggestion_view_ = suggestion_button_container->AddChildView(
      std::make_unique<OmniboxMatchCellView>(this));
  suggestion_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(4));

  const auto child_insets =
      gfx::Insets::TLBR(0, 0, 0, OmniboxMatchCellView::kMarginRight);

  remove_suggestion_button_ = suggestion_button_container->AddChildView(
      std::make_unique<OmniboxRemoveSuggestionButton>(base::BindRepeating(
          &OmniboxResultView::ButtonPressed, base::Unretained(this),
          OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION)));
  remove_suggestion_button_->SetProperty(views::kMarginsKey, child_insets);
  views::InstallCircleHighlightPathGenerator(remove_suggestion_button_);
  remove_suggestion_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_REMOVE_SUGGESTION));
  auto* const focus_ring = views::FocusRing::Get(remove_suggestion_button_);
  focus_ring->SetHasFocusPredicate([&](View* view) {
    return view->GetVisible() && GetMatchSelected() &&
           (popup_view_->GetSelection().state ==
            OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION);
  });
  focus_ring->SetColorId(kColorOmniboxResultsFocusIndicator);

  button_row_ = AddChildView(std::make_unique<OmniboxSuggestionButtonRowView>(
      popup_view_, model_, model_index));

  // Quickly mouse-exiting through the suggestion button row sometimes leaves
  // the whole row highlighted. This fixes that. It doesn't seem necessary to
  // further observe the child controls of |button_row_|.
  mouse_enter_exit_handler_.ObserveMouseEnterExitOn(button_row_);
}

OmniboxResultView::~OmniboxResultView() {}

// static
std::unique_ptr<views::Background> OmniboxResultView::GetPopupCellBackground(
    views::View* view,
    OmniboxPartState part_state) {
  DCHECK(view);

  bool prefers_contrast = view->GetNativeTheme() &&
                          view->GetNativeTheme()->UserHasContrastPreference();
  // TODO(tapted): Consider using background()->SetNativeControlColor() and
  // always have a background.
  if (part_state == OmniboxPartState::NORMAL && !prefers_contrast)
    return nullptr;

  return views::CreateThemedSolidBackground(
      GetOmniboxBackgroundColorId(part_state));
}

void OmniboxResultView::SetMatch(const AutocompleteMatch& match) {
  match_ = match.GetMatchWithContentsAndDescriptionPossiblySwapped();

  const int suggestion_indent =
      popup_view_->InExplicitExperimentalKeywordMode() ? 70 : 0;
  suggestion_view_->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(0, suggestion_indent, 0, 0));

  suggestion_view_->OnMatchUpdate(this, match_);
  UpdateRemoveSuggestionVisibility();

  suggestion_view_->content()->SetTextWithStyling(match_.contents,
                                                  match_.contents_class);
  if (match_.answer) {
    suggestion_view_->content()->AppendExtraText(match_.answer->first_line());
    suggestion_view_->description()->SetTextWithStyling(
        match_.answer->second_line(), true);
  } else {
    // Not all 2-line suggestions have deemphasized descriptions; specifically,
    // calculator answers are 2-line but not deemphasized.
    const bool deemphasize =
        match_.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
        OmniboxMatchCellView::ShouldDisplayImage(match_) &&
        !OmniboxFieldTrial::IsUniformRowHeightEnabled();
    suggestion_view_->description()->SetTextWithStyling(
        match_.description, match_.description_class, deemphasize);
  }
  button_row_->UpdateFromModel();

  ApplyThemeAndRefreshIcons();
  InvalidateLayout();
}

void OmniboxResultView::ApplyThemeAndRefreshIcons(bool force_reapply_styles) {
  const ui::ColorId icon_color_id = GetMatchSelected()
                                        ? kColorOmniboxResultsIconSelected
                                        : kColorOmniboxResultsIcon;
  views::SetImageFromVectorIconWithColor(
      remove_suggestion_button_, vector_icons::kCloseRoundedIcon,
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetColorProvider()->GetColor(icon_color_id),
      /* omnibox buttons are never disabled */
      gfx::kPlaceholderColor);

  const OmniboxPartState state = GetThemeState();
  SetBackground(GetPopupCellBackground(this, state));

  // Reapply the dim color to account for the highlight state.
  const bool selected = (state == OmniboxPartState::SELECTED);
  const ui::ColorId dimmed_id = selected
                                    ? kColorOmniboxResultsTextDimmedSelected
                                    : kColorOmniboxResultsTextDimmed;
  suggestion_view_->separator()->ApplyTextColor(dimmed_id);
  if (remove_suggestion_button_->GetVisible())
    views::FocusRing::Get(remove_suggestion_button_)->SchedulePaint();

  // Recreate the icons in case the color needs to change.
  // Note: if this is an extension icon or favicon then this can be done in
  //       SetMatch() once (rather than repeatedly, as happens here). There may
  //       be an optimization opportunity here.
  // TODO(dschuyler): determine whether to optimize the color changes.
  auto icon = GetIcon();
  if (icon.IsEmpty())
    suggestion_view_->ClearIcon();
  else
    suggestion_view_->SetIcon(*icon.ToImageSkia());

  // We must reapply colors for all the text fields here. If we don't, we can
  // break theme changes for ZeroSuggest. See https://crbug.com/1095205.
  //
  // TODO(tommycli): We should finish migrating this logic to live entirely
  // within OmniboxTextView, which should keep track of its own OmniboxPart.
  const ui::ColorId default_id =
      selected ? kColorOmniboxResultsTextSelected : kColorOmniboxText;
  bool prefers_contrast =
      GetNativeTheme() && GetNativeTheme()->UserHasContrastPreference();
  if (match_.answer) {
    suggestion_view_->content()->ApplyTextColor(default_id);
    suggestion_view_->description()->ApplyTextColor(default_id);
  } else if (match_.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY) {
    suggestion_view_->content()->ApplyTextColor(default_id);
    suggestion_view_->description()->ApplyTextColor(dimmed_id);
  } else if (match_.type == AutocompleteMatchType::NULL_RESULT_MESSAGE) {
    suggestion_view_->content()->ApplyTextColor(kColorOmniboxText);
  } else if (prefers_contrast || force_reapply_styles) {
    // Normally, OmniboxTextView caches its appearance, but in high contrast,
    // selected-ness changes the text colors, so the styling of the text part of
    // the results needs to be recomputed.
    suggestion_view_->content()->ReapplyStyling();
    suggestion_view_->description()->ReapplyStyling();
  }

  button_row_->SetThemeState(GetThemeState());

  // The selection indicator indicates when the suggestion is focused. Do not
  // show the selection indicator if an auxiliary button is selected.
  selection_indicator_->SetVisible(selected &&
                                   popup_view_->GetSelection().state ==
                                       OmniboxPopupSelection::NORMAL);
}

void OmniboxResultView::OnSelectionStateChanged() {
  UpdateRemoveSuggestionVisibility();
  if (GetMatchSelected()) {
    // Immediately before notifying screen readers that the selected item has
    // changed, we want to update the name of the newly-selected item so that
    // any cached values get updated prior to the selection change.
    EmitTextChangedAccessiblityEvent();

    auto selection_state = popup_view_->GetSelection().state;

    // The text is also accessible via text/value change events in the omnibox
    // but this selection event allows the screen reader to get more details
    // about the list and the user's position within it.
    // Limit which selection states fire the events, in order to avoid duplicate
    // events. Specifically, OmniboxPopupViewViews::ProvideButtonFocusHint()
    // already fires the correct events when the user tabs to an attached button
    // in the current row.
    if (selection_state == OmniboxPopupSelection::FOCUSED_BUTTON_HEADER ||
        selection_state == OmniboxPopupSelection::NORMAL) {
      popup_view_->FireAXEventsForNewActiveDescendant(this);
    }
  }
  ApplyThemeAndRefreshIcons();
  button_row_->SelectionStateChanged();
}

bool OmniboxResultView::GetMatchSelected() const {
  // The header button being focused means the match itself is NOT focused.
  OmniboxPopupSelection selection = popup_view_->GetSelection();
  return selection.line == model_index_ &&
         selection.state != OmniboxPopupSelection::FOCUSED_BUTTON_HEADER;
}

views::Button* OmniboxResultView::GetActiveAuxiliaryButtonForAccessibility() {
  if (popup_view_->GetSelection().state ==
      OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION) {
    return remove_suggestion_button_;
  }

  return button_row_->GetActiveButton();
}

OmniboxPartState OmniboxResultView::GetThemeState() const {
  // NULL_RESULT_MESSAGE matches are no-op suggestions that only deliver a
  // message. The selected and hovered states imply an action can be taken from
  // that suggestion, so do not allow those states for this result.
  if (match_.type == AutocompleteMatchType::NULL_RESULT_MESSAGE)
    return OmniboxPartState::NORMAL;

  if (GetMatchSelected())
    return OmniboxPartState::SELECTED;

  // If we don't highlight the whole row when the user has the mouse over the
  // remove suggestion button, it's unclear which suggestion is being removed.
  return IsMouseHovered() ? OmniboxPartState::HOVERED
                          : OmniboxPartState::NORMAL;
}

void OmniboxResultView::OnMatchIconUpdated() {
  // The new icon will be fetched during ApplyThemeAndRefreshIcons().
  ApplyThemeAndRefreshIcons();
}

void OmniboxResultView::SetRichSuggestionImage(const gfx::ImageSkia& image) {
  suggestion_view_->SetImage(image, match_);
}

void OmniboxResultView::ButtonPressed(OmniboxPopupSelection::LineState state,
                                      const ui::Event& event) {
  model_->OpenSelection(OmniboxPopupSelection(model_index_, state),
                        event.time_stamp());
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides:

bool OmniboxResultView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    popup_view_->SetSelectedIndex(model_index_);
    // Inform the model that a new result is now selected via mouse press.
    model_->OnNavigationLikely(model_index_,
                               omnibox::mojom::NavigationPredictor::kMouseDown);
  }
  return true;
}

bool OmniboxResultView::OnMouseDragged(const ui::MouseEvent& event) {
  if (HitTestPoint(event.location())) {
    // When the drag enters or remains within the bounds of this view, either
    // set the state to be selected or hovered, depending on the mouse button.
    if (event.IsOnlyLeftMouseButton()) {
      if (!GetMatchSelected()) {
        popup_view_->SetSelectedIndex(model_index_);
      }
    } else {
      UpdateHoverState();
    }
    return true;
  }

  // When the drag leaves the bounds of this view, cancel the hover state and
  // pass control to the popup view.
  UpdateHoverState();
  SetMouseAndGestureHandler(popup_view_);
  return false;
}

void OmniboxResultView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() || event.IsOnlyLeftMouseButton()) {
    WindowOpenDisposition disposition =
        event.IsOnlyLeftMouseButton()
            ? WindowOpenDisposition::CURRENT_TAB
            : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    model_->OpenSelection(OmniboxPopupSelection(model_index_),
                          event.time_stamp(), disposition);
  }
}

void OmniboxResultView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHoverState();
}

void OmniboxResultView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHoverState();
}

void OmniboxResultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Get the label without the ", n of m" positional text appended.
  // The positional info is provided via
  // ax::mojom::IntAttribute::kPosInSet/SET_SIZE and providing it via text as
  // well would result in duplicate announcements.

  node_data->role = ax::mojom::Role::kListBoxOption;

  // TODO(tommycli): We re-fetch the original match from the popup model,
  // because |match_| already has its contents and description swapped by this
  // class, and we don't want that for the bubble. We should improve this.
  bool is_selected = GetMatchSelected();
  if (model_index_ < model_->result().size()) {
    AutocompleteMatch raw_match = model_->result().match_at(model_index_);
    // The selected match can have a special name, e.g. when is one or more
    // buttons that can be tabbed to.
    std::u16string label =
        is_selected ? model_->GetPopupAccessibilityLabelForCurrentSelection(
                          raw_match.contents, false)
                    : AutocompleteMatchType::ToAccessibilityLabel(
                          raw_match, raw_match.contents);
    node_data->SetName(label);
  }

  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             model_index_ + 1);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             model_->result().size());

  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, is_selected);
  if (IsMouseHovered())
    node_data->AddState(ax::mojom::State::kHovered);
}

void OmniboxResultView::OnThemeChanged() {
  views::View::OnThemeChanged();
  ApplyThemeAndRefreshIcons(true);
}

void OmniboxResultView::EmitTextChangedAccessiblityEvent() {
  if (!popup_view_->IsOpen()) {
    return;
  }

  // The omnibox results list reuses the same items, but the text displayed for
  // these items is updated as the value of omnibox changes. The displayed text
  // for a given item is exposed to screen readers as the item's name/label.
  ui::AXNodeData node_data;
  GetAccessibleNodeData(&node_data);
  std::u16string current_name =
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
  if (accessible_name_ != current_name) {
    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
    accessible_name_ = current_name;
  }
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, private:

gfx::Image OmniboxResultView::GetIcon() const {
  // Usually, use kColorOmniboxResultsIcon[Selected] for icon color. Except for
  // history cluster suggestions which want to stand out. They reuse the
  // kColorOmniboxResultsUrl[Selected] color which is intended for the URL text
  // in suggestion texts.
  ui::ColorId vector_icon_color_id;
  if (match_.type == AutocompleteMatchType::HISTORY_CLUSTER) {
    // TODO(manukh): Fix this when fixing icon inconsistencies between the
    //   dropdown and omnibox.
    vector_icon_color_id = kColorOmniboxResultsStarterPackIcon;
  } else if (match_.type == AutocompleteMatchType::STARTER_PACK) {
    vector_icon_color_id = kColorOmniboxResultsStarterPackIcon;
  } else {
    vector_icon_color_id = GetMatchSelected() ? kColorOmniboxResultsIconSelected
                                              : kColorOmniboxResultsIcon;
  }
  return popup_view_->GetMatchIcon(
      match_, GetColorProvider()->GetColor(vector_icon_color_id));
}

void OmniboxResultView::UpdateHoverState() {
  UpdateRemoveSuggestionVisibility();
  ApplyThemeAndRefreshIcons();
}

void OmniboxResultView::UpdateRemoveSuggestionVisibility() {
  bool old_visibility = remove_suggestion_button_->GetVisible();
  bool new_visibility =
      model_->IsPopupControlPresentOnMatch(OmniboxPopupSelection(
          model_index_,
          OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION)) &&
      (GetMatchSelected() || IsMouseHovered());

  remove_suggestion_button_->SetVisible(new_visibility);

  if (old_visibility != new_visibility)
    InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, views::View overrides, private:

void OmniboxResultView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxResultView, overrides, private:

DEFINE_ENUM_CONVERTERS(OmniboxPartState,
                       {OmniboxPartState::NORMAL, u"NORMAL"},
                       {OmniboxPartState::HOVERED, u"HOVERED"},
                       {OmniboxPartState::SELECTED, u"SELECTED"})

BEGIN_METADATA(OmniboxResultView, views::View)
ADD_READONLY_PROPERTY_METADATA(bool, MatchSelected)
ADD_READONLY_PROPERTY_METADATA(OmniboxPartState, ThemeState)
ADD_READONLY_PROPERTY_METADATA(gfx::Image, Icon)
END_METADATA
