// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"

class OmniboxSuggestionRowButton : public views::MdTextButton {
 public:
  METADATA_HEADER(OmniboxSuggestionRowButton);
  OmniboxSuggestionRowButton(PressedCallback callback,
                             const std::u16string& text,
                             const gfx::VectorIcon& icon,
                             OmniboxPopupViewViews* popup_contents_view,
                             OmniboxPopupSelection selection)
      : MdTextButton(std::move(callback), text, CONTEXT_OMNIBOX_PRIMARY),
        icon_(&icon),
        popup_contents_view_(popup_contents_view),
        selection_(selection) {
    SetTriggerableEventFlags(GetTriggerableEventFlags() |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    views::InstallPillHighlightPathGenerator(this);

    if (base::FeatureList::IsEnabled(omnibox::kCr2023ActionChips) ||
        features::GetChromeRefresh2023Level() ==
            features::ChromeRefresh2023Level::kLevel2) {
      SetImageLabelSpacing(4);
      SetCustomPadding(ChromeLayoutProvider::Get()->GetInsetsMetric(
          INSETS_OMNIBOX_PILL_BUTTON));
      SetCornerRadius(GetLayoutConstant(TOOLBAR_CORNER_RADIUS));
    } else {
      SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
      SetCustomPadding(ChromeLayoutProvider::Get()->GetInsetsMetric(
          INSETS_OMNIBOX_PILL_BUTTON));
      SetCornerRadius(GetInsets().height() +
                      GetLayoutConstant(LOCATION_BAR_ICON_SIZE));
    }

    auto* const ink_drop = views::InkDrop::Get(this);
    ink_drop->SetHighlightOpacity(kOmniboxOpacityHovered);
    SetAnimationDuration(base::TimeDelta());
    ink_drop->GetInkDrop()->SetHoverHighlightFadeDuration(base::TimeDelta());

    auto* const focus_ring = views::FocusRing::Get(this);
    focus_ring->SetHasFocusPredicate([=](View* view) {
      return view->GetVisible() &&
             popup_contents_view_->GetSelection() == selection_;
    });
    focus_ring->SetColorId(kColorOmniboxResultsFocusIndicator);
  }

  OmniboxSuggestionRowButton(const OmniboxSuggestionRowButton&) = delete;
  OmniboxSuggestionRowButton& operator=(const OmniboxSuggestionRowButton&) =
      delete;

  ~OmniboxSuggestionRowButton() override = default;

  void SetThemeState(OmniboxPartState theme_state) {
    if (theme_state_ == theme_state)
      return;
    theme_state_ = theme_state;
    OnThemeChanged();
  }

  OmniboxPopupSelection selection() { return selection_; }

  void OnThemeChanged() override {
    MdTextButton::OnThemeChanged();
    // We can't use colors from NativeTheme as the omnibox theme might be
    // different (for example, if the NTP colors are customized).
    const auto* const color_provider = GetColorProvider();
    const bool selected = theme_state_ == OmniboxPartState::SELECTED;
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      *icon_,
                      selected ? kColorOmniboxResultsButtonIconSelected
                               : kColorOmniboxResultsButtonIcon,
                      GetLayoutConstant(LOCATION_BAR_ICON_SIZE)));
    SetEnabledTextColors(color_provider->GetColor(
        selected ? kColorOmniboxResultsTextSelected : kColorOmniboxText));
    views::InkDrop::Get(this)->SetBaseColorId(
        selected ? kColorOmniboxResultsButtonInkDropSelected
                 : kColorOmniboxResultsButtonInkDrop);

    views::FocusRing::Get(this)->SchedulePaint();
  }

  void UpdateBackgroundColor() override {
    const auto* const color_provider = GetColorProvider();
    const SkColor stroke_color =
        color_provider->GetColor(kColorOmniboxResultsButtonBorder);
    const SkColor fill_color =
        color_provider->GetColor(GetOmniboxBackgroundColorId(theme_state_));
    SetBackground(CreateBackgroundFromPainter(
        views::Painter::CreateRoundRectWith1PxBorderPainter(
            fill_color, stroke_color, GetCornerRadiusValue())));
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Although this appears visually as a button, expose as a list box option
    // so that it matches the other options within its list box container.
    node_data->role = ax::mojom::Role::kListBoxOption;
    node_data->SetName(GetAccessibleName());
  }

  void SetIcon(const gfx::VectorIcon& icon) {
    if (icon_ != &icon) {
      icon_ = &icon;
      OnThemeChanged();
    }
  }

 private:
  raw_ptr<const gfx::VectorIcon> icon_;
  raw_ptr<OmniboxPopupViewViews> popup_contents_view_;
  OmniboxPartState theme_state_ = OmniboxPartState::NORMAL;

  OmniboxPopupSelection selection_;
};

BEGIN_METADATA(OmniboxSuggestionRowButton, views::MdTextButton)
END_METADATA

OmniboxSuggestionButtonRowView::OmniboxSuggestionButtonRowView(
    OmniboxPopupViewViews* popup_contents_view,
    OmniboxEditModel* model,
    int model_index)
    : popup_contents_view_(popup_contents_view),
      model_(model),
      model_index_(model_index) {
  int bottom_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetInteriorMargin(gfx::Insets::TLBR(
          0, OmniboxMatchCellView::GetTextIndent(), bottom_margin, 0))
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  BuildViews();
}

void OmniboxSuggestionButtonRowView::BuildViews() {
  // Clear and reset existing views.
  {
    RemoveAllChildViews();
    keyword_button_ = nullptr;
    tab_switch_button_ = nullptr;
    action_buttons_.clear();
    previous_active_button_ = nullptr;
  }

  // For all of these buttons, the visibility set from UpdateFromModel().
  // The Keyword and Pedal buttons also get their text from there, since the
  // text depends on the actual match. That shouldn't produce a flicker, because
  // it's called directly from OmniboxResultView::SetMatch(). If this flickers,
  // then so does everything else in the result view.
  keyword_button_ = AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
      base::BindRepeating(&OmniboxSuggestionButtonRowView::ButtonPressed,
                          base::Unretained(this),
                          OmniboxPopupSelection::KEYWORD_MODE),
      std::u16string(), vector_icons::kSearchIcon, popup_contents_view_,
      OmniboxPopupSelection(model_index_,
                            OmniboxPopupSelection::KEYWORD_MODE)));
  tab_switch_button_ =
      AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
          base::BindRepeating(&OmniboxSuggestionButtonRowView::ButtonPressed,
                              base::Unretained(this),
                              OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH),
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SUGGEST_HINT),
          omnibox::kSwitchIcon, popup_contents_view_,
          OmniboxPopupSelection(
              model_index_, OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH)));
  tab_switch_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACC_TAB_SWITCH_BUTTON));

  if (!HasMatch()) {
    // Skip remaining code that depends on `match()`.
    return;
  }

  // Only create buttons for existent actions.
  for (size_t action_index = 0; action_index < match().actions.size();
       action_index++) {
    auto* button = AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
        base::BindRepeating(&OmniboxSuggestionButtonRowView::ButtonPressed,
                            base::Unretained(this),
                            OmniboxPopupSelection::FOCUSED_BUTTON_ACTION),
        std::u16string(), match().actions[action_index]->GetVectorIcon(),
        popup_contents_view_,
        OmniboxPopupSelection(model_index_,
                              OmniboxPopupSelection::FOCUSED_BUTTON_ACTION,
                              action_index)));
    action_buttons_.push_back(button);
  }
}

OmniboxSuggestionButtonRowView::~OmniboxSuggestionButtonRowView() = default;

void OmniboxSuggestionButtonRowView::UpdateFromModel() {
  if (!HasMatch()) {
    // Skip remaining code that depends on `match()`.
    return;
  }

  // Only build views if there was a structural change. Without this check,
  // performance could be impacted by frequent unnecessary rebuilds.
  if (action_buttons_.size() != match().actions.size()) {
    BuildViews();
  }

  SetPillButtonVisibility(keyword_button_, OmniboxPopupSelection::KEYWORD_MODE);
  if (keyword_button_->GetVisible()) {
    std::u16string keyword;
    bool is_keyword_hint = false;
    match().GetKeywordUIState(model_->client()->GetTemplateURLService(),
                              &keyword, &is_keyword_hint);

    const auto names = SelectedKeywordView::GetKeywordLabelNames(
        keyword, model_->client()->GetTemplateURLService());
    keyword_button_->SetText(names.full_name);
    keyword_button_->SetAccessibleName(
        l10n_util::GetStringFUTF16(IDS_ACC_KEYWORD_MODE, names.short_name));
  }

  SetPillButtonVisibility(tab_switch_button_,
                          OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH);

  for (const auto& action_button : action_buttons_) {
    SetPillButtonVisibility(action_button,
                            OmniboxPopupSelection::FOCUSED_BUTTON_ACTION);
    if (action_button->GetVisible()) {
      const OmniboxAction* action =
          match().actions[action_button->selection().action_index].get();
      const auto label_strings = action->GetLabelStrings();
      action_button->SetText(label_strings.hint);
      action_button->SetTooltipText(label_strings.suggestion_contents);
      action_button->SetAccessibleName(label_strings.accessibility_hint);
      action_button->SetIcon(action->GetVectorIcon());
    }
  }

  bool is_any_button_visible =
      keyword_button_->GetVisible() || tab_switch_button_->GetVisible() ||
      base::ranges::any_of(action_buttons_, [](const auto& action_button) {
        return action_button->GetVisible();
      });
  SetVisible(is_any_button_visible);
}

void OmniboxSuggestionButtonRowView::SelectionStateChanged() {
  auto* const active_button = GetActiveButton();
  if (active_button == previous_active_button_) {
    return;
  }
  if (previous_active_button_) {
    views::FocusRing::Get(previous_active_button_)->SchedulePaint();
  }
  if (active_button) {
    views::FocusRing::Get(active_button)->SchedulePaint();
  }
  previous_active_button_ = active_button;
}

void OmniboxSuggestionButtonRowView::SetThemeState(
    OmniboxPartState theme_state) {
  keyword_button_->SetThemeState(theme_state);
  tab_switch_button_->SetThemeState(theme_state);
  for (const auto& action_button : action_buttons_) {
    action_button->SetThemeState(theme_state);
  }
}

views::Button* OmniboxSuggestionButtonRowView::GetActiveButton() const {
  std::vector<OmniboxSuggestionRowButton*> buttons{
      keyword_button_,
      tab_switch_button_,
  };
  buttons.insert(buttons.end(), action_buttons_.begin(), action_buttons_.end());

  // Find the button that matches model selection.
  auto selected_button =
      base::ranges::find(buttons, popup_contents_view_->GetSelection(),
                         &OmniboxSuggestionRowButton::selection);
  return selected_button == buttons.end() ? nullptr : *selected_button;
}

bool OmniboxSuggestionButtonRowView::HasMatch() const {
  return model_->result().size() > model_index_;
}

const AutocompleteMatch& OmniboxSuggestionButtonRowView::match() const {
  return model_->result().match_at(model_index_);
}

void OmniboxSuggestionButtonRowView::SetPillButtonVisibility(
    OmniboxSuggestionRowButton* button,
    OmniboxPopupSelection::LineState state) {
  button->SetVisible(model_->IsPopupControlPresentOnMatch(
      OmniboxPopupSelection(model_index_, state)));
}

void OmniboxSuggestionButtonRowView::ButtonPressed(
    OmniboxPopupSelection::LineState state,
    const ui::Event& event) {
  const OmniboxPopupSelection selection(model_index_, state);
  if (state == OmniboxPopupSelection::KEYWORD_MODE) {
    // TODO(yoangela): Port to PopupModel and merge with keyEvent
    // TODO(orinj): Clear out existing suggestions, particularly this one, as
    // once we AcceptKeyword, we are really in a new scope state and holding
    // onto old suggestions is confusing and error prone. Without this check,
    // a second click of the button violates assumptions in |AcceptKeyword|.
    // Note: Since keyword mode logic depends on state of the edit model, the
    // selection must first be set to prepare for keyword mode before accepting.
    model_->SetPopupSelection(selection);
    if (model_->is_keyword_hint()) {
      const auto entry_method =
          event.IsMouseEvent() ? metrics::OmniboxEventProto::CLICK_HINT_VIEW
                               : metrics::OmniboxEventProto::TAP_HINT_VIEW;
      model_->AcceptKeyword(entry_method);
    }
  } else {
    WindowOpenDisposition disposition =
        ui::DispositionFromEventFlags(event.flags());
    model_->OpenSelection(selection, event.time_stamp(), disposition);
  }
}

BEGIN_METADATA(OmniboxSuggestionButtonRowView, views::View)
END_METADATA
