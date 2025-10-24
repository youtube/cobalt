// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_VIEW_H_
#define CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/editor_menu/utils/pre_target_handler_view.h"
#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/flex_layout_view.h"

class ApplicationLocaleStorage;

namespace views {
class ImageButton;
class FlexLayoutView;
class View;
class Widget;
}  // namespace views

namespace chromeos::editor_menu {

class EditorMenuTextfieldView;
class EditorMenuViewDelegate;

// A bubble style view to show Editor Menu.
class EditorMenuView : public PreTargetHandlerView,
                       public views::TabbedPaneListener {
  METADATA_HEADER(EditorMenuView, views::View)

 public:
  // `application_locale_storage` must be non-null and must outlive `this`.
  EditorMenuView(const ApplicationLocaleStorage* application_locale_storage,
                 TextAndImageMode text_and_image_mode,
                 const PresetTextQueries& preset_text_queries,
                 const gfx::Rect& anchor_view_bounds,
                 EditorMenuViewDelegate* delegate);

  EditorMenuView(const EditorMenuView&) = delete;
  EditorMenuView& operator=(const EditorMenuView&) = delete;

  ~EditorMenuView() override;

  // `application_locale_storage` must be non-null and must outlive the widget.
  static std::unique_ptr<views::Widget> CreateWidget(
      const ApplicationLocaleStorage* application_locale_storage,
      TextAndImageMode text_and_image_mode,
      const PresetTextQueries& preset_text_queries,
      const gfx::Rect& anchor_view_bounds,
      EditorMenuViewDelegate* delegate);

  // PreTargetHandlerView:
  void AddedToWidget() override;
  void RequestFocus() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // views::TabbedPaneListener
  void TabSelectedAt(int index) override;

  void UpdateBounds(const gfx::Rect& anchor_view_bounds);

  void DisableMenu();

  views::View* chips_container_for_testing() { return chips_container_; }

  EditorMenuTextfieldView* textfield_for_testing() { return textfield_; }

  static const char* GetWidgetNameForTest();

 private:
  void InitLayout(const PresetTextQueries& preset_text_queries);
  gfx::Insets GetTitleContainerInsets() const;
  void AddTitleContainer();
  void AddChipsContainer(const PresetTextQueries& preset_text_queries);
  void AddTextfield();

  void UpdateChipsContainer(int editor_menu_width);

  views::View* AddChipsRow();

  void OnSettingsButtonPressed();
  void OnChipButtonPressed(const std::string& text_query_id);

  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;

  const TextAndImageMode text_and_image_mode_;

  // `delegate_` outlives `this`.
  raw_ptr<EditorMenuViewDelegate> delegate_ = nullptr;

  // Containing title, badge, and icons.
  raw_ptr<views::View> title_container_ = nullptr;
  raw_ptr<views::TabbedPane> tabbed_pane_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;

  // Containing chips.
  raw_ptr<views::FlexLayoutView> chips_container_ = nullptr;

  raw_ptr<EditorMenuTextfieldView> textfield_ = nullptr;

  bool queued_announcement_ = false;

  base::WeakPtrFactory<EditorMenuView> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_VIEW_H_
