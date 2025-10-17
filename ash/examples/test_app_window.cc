// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/examples/test_app_window.h"

#include <memory>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/models/combobox_model.h"
#include "ui/color/color_variant.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

class AppTypeComboboxModel : public ui::ComboboxModel {
 public:
  AppTypeComboboxModel() = default;
  AppTypeComboboxModel(const AppTypeComboboxModel&) = delete;
  AppTypeComboboxModel& operator=(const AppTypeComboboxModel&) = delete;
  ~AppTypeComboboxModel() override = default;

 private:
  // ui::ComboboxModel:
  size_t GetItemCount() const override {
    return static_cast<size_t>(chromeos::AppType::kMaxValue) + 1;
  }
  std::u16string GetItemAt(size_t index) const override {
    switch (static_cast<chromeos::AppType>(index)) {
      case chromeos::AppType::NON_APP:
        return u"AppType: None";
      case chromeos::AppType::BROWSER:
        return u"AppType: Browser";
      case chromeos::AppType::CHROME_APP:
        return u"AppType: ChromeApp";
      case chromeos::AppType::ARC_APP:
        return u"AppType: Arc";
      case chromeos::AppType::CROSTINI_APP:
        return u"AppType: Crostini";
      case chromeos::AppType::SYSTEM_APP:
        return u"AppType: SystemApp";
    }
    return u"AppType: Unknown";
  }
};

class TestView : public views::View {
 public:
  TestView() {
    SetBackground(
        views::CreateSolidBackground(ui::ColorVariant(SK_ColorWHITE)));
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    auto add_checkbox =
        [](views::View* container, const std::u16string& text,
           const std::u16string& tooltip, bool initial_state,
           base::RepeatingCallback<void(const ui::Event&)> callback) {
          views::Checkbox* checkbox = container->AddChildView(
              std::make_unique<views::Checkbox>(text, std::move(callback)));
          checkbox->SetChecked(initial_state);
          checkbox->SetTooltipText(tooltip);
          return checkbox;
        };
    auto add_textfield = [](views::View* container, const std::u16string& text,
                            const std::u16string& tooltip,
                            const std::u16string& initial_value) {
      views::View* hc =
          container->AddChildView(std::make_unique<views::View>());
      auto* layout = hc->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);

      views::Label* label =
          hc->AddChildView(std::make_unique<views::Label>(text));
      label->SetTooltipText(tooltip);

      views::Textfield* textfield =
          hc->AddChildView(std::make_unique<views::Textfield>());
      textfield->SetText(initial_value);
      textfield->GetViewAccessibility().SetName(
          std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
      layout->SetFlexForView(textfield, 1);
      return textfield;
    };

    add_checkbox(
        this, u"Activatable", u"activatable",
        /*initial_state=*/true, base::BindRepeating([](const ui::Event& event) {
          views::Checkbox* cb = static_cast<views::Checkbox*>(event.target());
          cb->GetWidget()->widget_delegate()->SetCanActivate(cb->GetChecked());
        }));
    add_checkbox(
        this, u"Resizable", u"resizable",
        /*initial_state=*/true, base::BindRepeating([](const ui::Event& event) {
          views::Checkbox* cb = static_cast<views::Checkbox*>(event.target());
          cb->GetWidget()->widget_delegate()->SetCanResize(cb->GetChecked());
        }));
    add_checkbox(
        this, u"Maximizable", u"maximizable",
        /*initial_state=*/true, base::BindRepeating([](const ui::Event& event) {
          views::Checkbox* cb = static_cast<views::Checkbox*>(event.target());
          cb->GetWidget()->widget_delegate()->SetCanMaximize(cb->GetChecked());
        }));
    add_checkbox(this, u"Always On Top", u"always on top",
                 /*initial_state=*/false,
                 base::BindRepeating([](const ui::Event& event) {
                   views::Checkbox* cb =
                       static_cast<views::Checkbox*>(event.target());
                   auto* window = cb->GetWidget()->GetNativeWindow();
                   window->SetProperty(aura::client::kZOrderingKey,
                                       cb->GetChecked()
                                           ? ui::ZOrderLevel::kFloatingWindow
                                           : ui::ZOrderLevel::kNormal);
                 }));

    AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&TestView::UpdateMinimumSize,
                            base::Unretained(this)),
        u"Apply Minimum Size"));

    width_ = add_textfield(this, u"Min Width", u"minimum width",
                           base::NumberToString16(GetMinimumSize().width()));
    height_ = add_textfield(this, u"Min height", u"minimum height",
                            base::NumberToString16(GetMinimumSize().height()));
    auto* app_types = AddChildView(std::make_unique<views::Combobox>(
        std::make_unique<AppTypeComboboxModel>()));
    app_types->GetViewAccessibility().SetName(
        std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    app_types->SetCallback(base::BindRepeating(
        [](views::Combobox* app_types) -> void {
          size_t selected = app_types->GetSelectedIndex().value_or(0);
          auto* window = app_types->GetWidget()->GetNativeWindow();
          window->SetProperty(chromeos::kAppTypeKey,
                              static_cast<chromeos::AppType>(selected));
        },
        base::Unretained(app_types)));
  }

  gfx::Size GetMinimumSize() const override { return minimum_size_; }

  void UpdateMinimumSize() {
    int width;
    int height;
    if (base::StringToInt(width_->GetText(), &width) &&
        base::StringToInt(height_->GetText(), &height)) {
      minimum_size_.SetSize(width, height);
      PreferredSizeChanged();
    } else {
      LOG(ERROR) << "Failed to set minimum size to:" << width_->GetText()
                 << " x " << height_->GetText();
    }
  }

 private:
  raw_ptr<views::Textfield> width_, height_;
  gfx::Size minimum_size_{200, 100};
};

}  // namespace

void OpenTestAppWindow() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  auto delegate = std::make_unique<views::WidgetDelegate>();
  delegate->SetCanActivate(true);
  delegate->SetCanFullscreen(true);
  delegate->SetCanMaximize(true);
  delegate->SetCanMinimize(true);
  delegate->SetCanResize(true);

  params.delegate = delegate.release();
  params.delegate->SetContentsView(std::make_unique<TestView>());

  params.context = Shell::GetPrimaryRootWindow();
  params.name = "AshTestAppWindow";
  widget->Init(std::move(params));
  widget->Show();
}

}  // namespace ash
