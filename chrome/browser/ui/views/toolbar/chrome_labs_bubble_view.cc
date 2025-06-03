// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/chrome_labs_model.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_item_view.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

class ChromeLabsFooter : public views::View {
 public:
  METADATA_HEADER(ChromeLabsFooter);
  explicit ChromeLabsFooter(base::RepeatingClosure restart_callback) {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    AddChildView(
        views::Builder<views::Label>()
            .CopyAddressTo(&restart_label_)
            .SetText(l10n_util::GetStringUTF16(
                IDS_CHROMELABS_RELAUNCH_FOOTER_MESSAGE))
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetProperty(views::kFlexBehaviorKey,
                         views::FlexSpecification(
                             views::MinimumFlexSizeRule::kPreferred,
                             views::MaximumFlexSizeRule::kPreferred, true))
            .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
                0, 0,
                views::LayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_RELATED_CONTROL_VERTICAL),
                0)))
            .Build());
    AddChildView(views::Builder<views::MdTextButton>()
                     .CopyAddressTo(&restart_button_)
                     .SetCallback(restart_callback)
                     .SetText(l10n_util::GetStringUTF16(
                         IDS_CHROMELABS_RELAUNCH_BUTTON_LABEL))
                     .SetProminent(true)
                     .Build());
    SetBackground(
        views::CreateThemedSolidBackground(ui::kColorBubbleFooterBackground));
    SetBorder(views::CreateEmptyBorder(
        views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG)));
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred, true));
  }

 private:
  raw_ptr<views::MdTextButton> restart_button_;
  raw_ptr<views::Label> restart_label_;
};

BEGIN_METADATA(ChromeLabsFooter, views::View)
END_METADATA

}  // namespace

ChromeLabsBubbleView::ChromeLabsBubbleView(ChromeLabsButton* anchor_view)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT) {
  SetProperty(views::kElementIdentifierKey, kToolbarChromeLabsBubbleElementId);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetTitle(l10n_util::GetStringUTF16(IDS_WINDOW_TITLE_EXPERIMENTS));
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::BoxLayout::Orientation::kVertical);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(gfx::Insets(0));
  SetEnableArrowKeyTraversal(true);
  // Set `kDialog` to avoid the BubbleDialogDelegate returning a default of
  // `kAlertDialog` which would tell screen readers to announce all contents of
  // the bubble when it opens and previous accessibility feedback said that
  // behavior was confusing.
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);

  // TODO(crbug.com/1259763): Currently basing this off what extension menu uses
  // for sizing as suggested as an initial fix by UI. Discuss a more formal
  // solution.
  constexpr int kMaxChromeLabsHeightDp = 448;
  auto scroll_view = std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled);
  // TODO(elainechien): Check with UI whether we want to draw overflow
  // indicator.
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  menu_item_container_ = scroll_view->SetContents(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kScaleToZero,
                           views::MaximumFlexSizeRule::kPreferred, true))
          .SetBorder(views::CreateEmptyBorder(
              views::LayoutProvider::Get()->GetInsetsMetric(
                  views::INSETS_DIALOG)))
          .Build());
  scroll_view->ClipHeightTo(0, kMaxChromeLabsHeightDp);
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kDialogRadius);
  scroll_view->SetViewportRoundedCornerRadius(
      gfx::RoundedCornersF(0.0f, 0.0f, corner_radius, corner_radius));
  AddChildView(std::move(scroll_view));

  /* base::Unretained is safe here because NotifyRestartCallback will notify a
   * CallbackListSubscription, and ChromeLabsFooter is a child on
   * ChromeLabsBubbleView. ChromeLabsBubbleView will outlive the
   * ChromeLabsFooter and the CallbackListSubscription will ensure the restart
   * callback is valid.. */
  restart_prompt_ = AddChildView(std::make_unique<ChromeLabsFooter>(
      base::BindRepeating(&ChromeLabsBubbleView::NotifyRestartCallback,
                          base::Unretained(this))));
  restart_prompt_->SetVisible(about_flags::IsRestartNeededToCommitChanges());
}

ChromeLabsBubbleView::~ChromeLabsBubbleView() = default;

ChromeLabsItemView* ChromeLabsBubbleView::AddLabItem(
    const LabInfo& lab,
    int default_index,
    const flags_ui::FeatureEntry* entry,
    Browser* browser,
    base::RepeatingCallback<void(ChromeLabsItemView* item_view)>
        combobox_callback) {
  std::unique_ptr<ChromeLabsItemView> item_view =
      std::make_unique<ChromeLabsItemView>(
          lab, default_index, entry, std::move(combobox_callback), browser);

  item_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred, true));

  return menu_item_container_->AddChildView(std::move(item_view));
}

size_t ChromeLabsBubbleView::GetNumLabItems() {
  return menu_item_container_->children().size();
}

base::CallbackListSubscription ChromeLabsBubbleView::RegisterRestartCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return restart_callback_list_.Add(std::move(callback));
}

void ChromeLabsBubbleView::ShowRelaunchPrompt() {
  restart_prompt_->SetVisible(about_flags::IsRestartNeededToCommitChanges());

  // Manually announce the relaunch footer message because VoiceOver doesn't
  // announces the message when the footer appears.
#if BUILDFLAG(IS_MAC)
  if (restart_prompt_->GetVisible()) {
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_CHROMELABS_RELAUNCH_FOOTER_MESSAGE));
  }
#endif

  SizeToContents();
}

void ChromeLabsBubbleView::NotifyRestartCallback() {
  restart_callback_list_.Notify();
}

views::View* ChromeLabsBubbleView::GetMenuItemContainerForTesting() {
  return menu_item_container_;
}

bool ChromeLabsBubbleView::IsRestartPromptVisibleForTesting() {
  return restart_prompt_->GetVisible();
}

BEGIN_METADATA(ChromeLabsBubbleView, views::BubbleDialogDelegateView)
END_METADATA
