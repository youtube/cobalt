// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/cxx20_to_address.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_button_util.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_drag_data.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/page_navigator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/base/models/image_model.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SavedTabGroupButton,
                                      kDeleteGroupMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SavedTabGroupButton,
                                      kMoveGroupToNewWindowMenuItem);

namespace {
// The max height of the button and the max width of a button with no title.
constexpr int kButtonSize = 20;
// The corner radius for the button.
constexpr float kButtonRadius = 6.0f;
// The amount of insets above and below the text.
constexpr float kVerticalInsets = 2.0f;
// The amount of insets before and after the text.
constexpr float kHorizontalInsets = 6.0f;
// The width of the outline of the button when open in the Tab Strip.
constexpr float kBorderThickness = 1.0f;
// The size of the squircle (rounded rect) in a button with no text.
constexpr float kEmptyChipSize = 12.0f;
// The amount of padding around the squircle (rounded rect).
constexpr float kEmptyChipInsets = 4.0f;
// The radius of the squircle (rounded rect).
constexpr float kEmptyChipCornerRadius = 2.0f;
}  // namespace

SavedTabGroupButton::SavedTabGroupButton(
    const SavedTabGroup& group,
    base::RepeatingCallback<content::PageNavigator*()> page_navigator,
    PressedCallback callback,
    Browser* browser,
    bool animations_enabled)
    : MenuButton(std::move(callback), group.title()),
      tab_group_color_id_(group.color()),
      guid_(group.saved_guid()),
      local_group_id_(group.local_group_id()),
      tabs_(group.saved_tabs()),
      browser_(*browser),
      service_(
          *SavedTabGroupServiceFactory::GetForProfile(browser_->profile())),
      page_navigator_callback_(std::move(page_navigator)),
      context_menu_controller_(
          this,
          base::BindRepeating(
              &SavedTabGroupButton::CreateDialogModelForContextMenu,
              base::Unretained(this)),
          views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED) {
  SetAccessibilityProperties(
      ax::mojom::Role::kButton, /*name=*/GetAccessibleNameForButton(),
      /*description=*/absl::nullopt,
      l10n_util::GetStringUTF16(
          IDS_ACCNAME_SAVED_TAB_GROUP_BUTTON_ROLE_DESCRIPTION));
  SetTextProperties(group);
  SetID(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  SetProperty(views::kElementIdentifierKey, kSavedTabGroupButtonElementId);
  SetMaxSize(gfx::Size(bookmark_button_util::kMaxButtonWidth, kButtonSize));
  label()->SetTextStyle(views::style::STYLE_BODY_4_EMPHASIS);

  show_animation_ = std::make_unique<gfx::SlideAnimation>(this);
  if (!animations_enabled) {
    // For some reason during testing the events generated by animating throw
    // off the test. So, don't animate while testing.
    show_animation_->Reset(1);
  } else {
    show_animation_->Show();
  }

  ConfigureInkDropForToolbar(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(0),
                                                kButtonRadius);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  set_drag_controller(this);
}

SavedTabGroupButton::~SavedTabGroupButton() = default;

void SavedTabGroupButton::UpdateButtonData(const SavedTabGroup& group) {
  SetTextProperties(group);

  tab_group_color_id_ = group.color();
  local_group_id_ = group.local_group_id();
  guid_ = group.saved_guid();
  tabs_.clear();
  tabs_ = group.saved_tabs();

  UpdateButtonLayout();
}

std::u16string SavedTabGroupButton::GetTooltipText(const gfx::Point& p) const {
  return label()->GetPreferredSize().width() > label()->size().width()
             ? GetText()
             : std::u16string();
}

bool SavedTabGroupButton::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    ShowContextMenu(GetKeyboardContextMenuLocation(),
                    ui::MenuSourceType::MENU_SOURCE_KEYBOARD);
    return true;
  } else if (event.key_code() == ui::KeyboardCode::VKEY_SPACE) {
    NotifyClick(event);
    return true;
  }

  return false;
}

void SavedTabGroupButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::MenuButton::GetAccessibleNodeData(node_data);
  node_data->SetNameChecked(GetAccessibleNameForButton());
  node_data->role = ax::mojom::Role::kButton;
}

void SavedTabGroupButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!GetText().empty()) {
    return;
  }

  // When the title is empty, we draw a circle similar to the tab group
  // header when there is no title.
  const ui::ColorProvider* const cp = GetColorProvider();
  SkColor text_and_outline_color =
      cp->GetColor(GetSavedTabGroupOutlineColorId(tab_group_color_id_));

  // Draw squircle (rounded rect).
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(text_and_outline_color);

  canvas->DrawRoundRect(gfx::RectF(kEmptyChipInsets, kEmptyChipInsets,
                                   kEmptyChipSize, kEmptyChipSize),
                        kEmptyChipCornerRadius, flags);
}

std::u16string SavedTabGroupButton::GetAccessibleNameForButton() {
  const std::u16string& opened_state =
      local_group_id_.has_value()
          ? l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)
          : l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_CLOSED);

  const std::u16string saved_group_acessible_name =
      GetText().empty()
          ? l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT, opened_state)
          : l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_NAMED_SAVED_GROUP_FORMAT, GetText(),
                opened_state);
  return saved_group_acessible_name;
}

void SavedTabGroupButton::SetTextProperties(const SavedTabGroup& group) {
  SetAccessibleName(GetAccessibleNameForButton());
  SetTooltipText(group.title());
  SetText(group.title());
}

void SavedTabGroupButton::UpdateButtonLayout() {
  // Relies on logic in theme_helper.cc to determine dark/light palette.
  ui::ColorId background_color =
      GetTabGroupBookmarkColorId(tab_group_color_id_);

  SetEnabledTextColorIds(
      GetSavedTabGroupForegroundColorId(tab_group_color_id_));
  SetBackground(views::CreateThemedRoundedRectBackground(background_color,
                                                         kButtonRadius));

  const gfx::Insets& insets =
      gfx::Insets::VH(kVerticalInsets, kHorizontalInsets);

  // Only draw a border if the group is open in the tab strip.
  if (!local_group_id_.has_value()) {
    SetBorder(views::CreateEmptyBorder(insets));
  } else {
    std::unique_ptr<views::Border> border =
        views::CreateThemedRoundedRectBorder(
            kBorderThickness, kButtonRadius,
            GetSavedTabGroupOutlineColorId(tab_group_color_id_));
    SetBorder(views::CreatePaddedBorder(std::move(border), insets));
  }

  if (GetText().empty()) {
    // When the text is empty force the button to have square dimensions.
    SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
  } else {
    SetPreferredSize(CalculatePreferredSize());
  }
}

std::unique_ptr<views::LabelButtonBorder>
SavedTabGroupButton::CreateDefaultBorder() const {
  auto border = std::make_unique<views::LabelButtonBorder>();
  return border;
}

void SavedTabGroupButton::OnThemeChanged() {
  views::MenuButton::OnThemeChanged();
  UpdateButtonLayout();
}

void SavedTabGroupButton::WriteDragDataForView(View* sender,
                                               const gfx::Point& press_pt,
                                               ui::OSExchangeData* data) {
  SavedTabGroupButton* const button =
      views::AsViewClass<SavedTabGroupButton>(sender);
  CHECK(button);
  CHECK(button == this);

  // Write the image and MIME type to the OSExchangeData.
  SavedTabGroupDragData::WriteToOSExchangeData(this, press_pt,
                                               GetThemeProvider(), data);
}

int SavedTabGroupButton::GetDragOperationsForView(View* sender,
                                                  const gfx::Point& p) {
  // This may need to become more complicated
  return ui::DragDropTypes::DRAG_MOVE;
}

bool SavedTabGroupButton::CanStartDragForView(View* sender,
                                              const gfx::Point& press_pt,
                                              const gfx::Point& p) {
  // Check if we have not moved enough horizontally but we have moved downward
  // vertically - downward drag.
  gfx::Vector2d move_offset = p - press_pt;
  return View::ExceededDragThreshold(move_offset);
}

void SavedTabGroupButton::TabMenuItemPressed(const GURL& url, int event_flags) {
  CHECK(page_navigator_callback_);

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                /*is_renderer_initiated=*/false,
                                /*started_from_context_menu=*/true);
  page_navigator_callback_.Run()->OpenURL(params);
}

void SavedTabGroupButton::MoveGroupToNewWindowPressed(int event_flags) {
  Browser* const browser_with_local_group_id =
      local_group_id_.has_value()
          ? SavedTabGroupUtils::GetBrowserWithTabGroupId(
                local_group_id_.value())
          : base::to_address(browser_);

  if (!local_group_id_.has_value()) {
    // Open the group in the browser the button was pressed.
    service_->OpenSavedTabGroupInBrowser(browser_with_local_group_id, guid_);
  }

  // Move the open group to a new browser window.
  const SavedTabGroup* group = service_->model()->Get(guid_);
  browser_with_local_group_id->tab_strip_model()
      ->delegate()
      ->MoveGroupToNewWindow(group->local_group_id().value());
}

void SavedTabGroupButton::DeleteGroupPressed(int event_flags) {
  if (local_group_id_.has_value()) {
    const Browser* const browser_with_local_group_id =
        SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id_.value());

    // Keep the opened tab group in the tabstrip but remove the SavedTabGroup
    // data from the model.
    TabGroup* tab_group = browser_with_local_group_id->tab_strip_model()
                              ->group_model()
                              ->GetTabGroup(local_group_id_.value());

    service_->UnsaveGroup(local_group_id_.value());

    // Notify observers to update the tab group header.
    // TODO(dljames): Find a way to move this into
    // SavedTabGroupKeyedService::DisconnectLocalTabGroup. The goal is to
    // abstract this logic from the button in case we need to do similar
    // functionality elsewhere in the future. Ensure this change works when
    // dragging a Saved group out of the window.
    tab_group->SetVisualData(*tab_group->visual_data());

  } else {
    // Remove the SavedTabGroup from the model. No need to worry about updating
    // tabstrip, since this group is not open.
    service_->model()->Remove(guid_);
  }
}

std::unique_ptr<ui::DialogModel>
SavedTabGroupButton::CreateDialogModelForContextMenu() {
  ui::DialogModel::Builder dialog_model = ui::DialogModel::Builder();

  const std::u16string move_or_open_group_text =
      local_group_id_.has_value()
          ? l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW)
          : l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW);

  bool should_enable_move_menu_item = true;
  if (local_group_id_.has_value()) {
    const Browser* const browser_with_local_group_id =
        SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id_.value());
    const TabStripModel* const tab_strip_model =
        browser_with_local_group_id->tab_strip_model();

    // Show the menu item if there are tabs outside of the saved group.
    should_enable_move_menu_item =
        tab_strip_model->count() != tab_strip_model->group_model()
                                        ->GetTabGroup(local_group_id_.value())
                                        ->tab_count();
  }

  dialog_model
      .AddMenuItem(
          ui::ImageModel::FromVectorIcon(kMoveGroupToNewWindowRefreshIcon),
          move_or_open_group_text,
          base::BindRepeating(&SavedTabGroupButton::MoveGroupToNewWindowPressed,
                              base::Unretained(this)),
          ui::DialogModelMenuItem::Params()
              .SetId(kMoveGroupToNewWindowMenuItem)
              .SetIsEnabled(should_enable_move_menu_item))
      .AddMenuItem(
          ui::ImageModel::FromVectorIcon(kCloseGroupRefreshIcon),
          l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP),
          base::BindRepeating(&SavedTabGroupButton::DeleteGroupPressed,
                              base::Unretained(this)),
          ui::DialogModelMenuItem::Params().SetId(kDeleteGroupMenuItem))
      .AddSeparator();

  for (const SavedTabGroupTab& tab : tabs_) {
    const ui::ImageModel& image =
        tab.favicon().has_value()
            ? ui::ImageModel::FromImage(tab.favicon().value())
            : favicon::GetDefaultFaviconModel(
                  GetTabGroupBookmarkColorId(tab_group_color_id_));
    const std::u16string title =
        tab.title().empty() ? base::UTF8ToUTF16(tab.url().spec()) : tab.title();
    dialog_model.AddMenuItem(
        image, title,
        base::BindRepeating(&SavedTabGroupButton::TabMenuItemPressed,
                            base::Unretained(this), tab.url()));
  }

  return dialog_model.Build();
}

BEGIN_METADATA(SavedTabGroupButton, MenuButton)
END_METADATA
