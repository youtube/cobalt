// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/wm_state.h"

#include "cobalt/shell/browser/cobalt_views_delegate.h"

namespace content {

struct ShellPlatformDelegate::ShellData {
  gfx::Size content_size;
  // Self-owned Widget, destroyed through CloseNow().
  raw_ptr<views::Widget> window_widget = nullptr;
  gfx::Size initial_size_;
};

struct ShellPlatformDelegate::PlatformData {
  std::unique_ptr<wm::WMState> wm_state;
  std::unique_ptr<display::Screen> screen;

  // TODO(danakj): This looks unused?
  std::unique_ptr<views::ViewsDelegate> views_delegate;
};

namespace {

// Maintain the UI controls and web view for content shell
class ShellView : public views::BoxLayoutView,
                  public views::TextfieldController {
 public:
  METADATA_HEADER(ShellView);

  enum UIControl { BACK_BUTTON, FORWARD_BUTTON, STOP_BUTTON };

  explicit ShellView(Shell* shell) : shell_(shell) { InitShellWindow(); }
  ShellView(const ShellView&) = delete;
  ShellView& operator=(const ShellView&) = delete;
  ~ShellView() override = default;

  // Update the state of UI controls
  void SetAddressBarURL(const GURL& url) {
    url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
  }

  void SetWebContents(WebContents* web_contents, const gfx::Size& size) {
    // If there was a previous WebView in this Shell it should be removed and
    // deleted.
    if (web_view_) {
      // ExtractAsDangling clears the underlying pointer and returns another
      // raw_ptr instance that is allowed to dangle.
      contents_view_->RemoveChildViewT(web_view_.ExtractAsDangling().get());
    }
    views::Builder<views::View>(contents_view_)
        .AddChild(views::Builder<views::WebView>()
                      .CopyAddressTo(&web_view_)
                      .SetBrowserContext(web_contents->GetBrowserContext())
                      .SetWebContents(web_contents)
                      .SetPreferredSize(size))
        .BuildChildren();
    web_contents->Focus();
    web_view_->SizeToPreferredSize();

    // Resize the widget, keeping the same origin.
    gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
    bounds.set_size(GetWidget()->GetRootView()->GetPreferredSize());
    GetWidget()->SetBounds(bounds);
  }

  void EnableUIControl(UIControl control, bool is_enabled) {
    if (control == BACK_BUTTON) {
      back_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                        : views::Button::STATE_DISABLED);
    } else if (control == FORWARD_BUTTON) {
      forward_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                           : views::Button::STATE_DISABLED);
    } else if (control == STOP_BUTTON) {
      stop_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                        : views::Button::STATE_DISABLED);
    }
  }

 private:
  // Initialize the UI control contained in shell window
  void InitShellWindow() {
    auto toolbar_button_rule = [](const views::View* view,
                                  const views::SizeBounds& size_bounds) {
      gfx::Size preferred_size = view->GetPreferredSize();
      if (size_bounds != views::SizeBounds() &&
          size_bounds.width().is_bounded()) {
        preferred_size.set_width(std::max(
            std::min(size_bounds.width().value(), preferred_size.width()),
            preferred_size.width() / 2));
      }
      return preferred_size;
    };

    auto builder =
        views::Builder<views::BoxLayoutView>(this)
            .SetBackground(
                views::CreateThemedSolidBackground(ui::kColorWindowBackground))
            .SetOrientation(views::BoxLayout::Orientation::kVertical);

    if (!Shell::ShouldHideToolbar()) {
      builder.AddChild(
          views::Builder<views::FlexLayoutView>()
              .CopyAddressTo(&toolbar_view_)
              .SetOrientation(views::LayoutOrientation::kHorizontal)
              // Top padding = 2, Bottom padding = 5
              .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(2, 0, 5, 0))
              .AddChildren(
                  views::Builder<views::MdTextButton>()
                      .CopyAddressTo(&back_button_)
                      .SetText(u"Back")
                      .SetCallback(base::BindRepeating(
                          &Shell::GoBackOrForward,
                          base::Unretained(shell_.get()), -1))
                      .SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(base::BindRepeating(
                                       toolbar_button_rule))),
                  views::Builder<views::MdTextButton>()
                      .CopyAddressTo(&forward_button_)
                      .SetText(u"Forward")
                      .SetCallback(base::BindRepeating(
                          &Shell::GoBackOrForward,
                          base::Unretained(shell_.get()), 1))
                      .SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(base::BindRepeating(
                                       toolbar_button_rule))),
                  views::Builder<views::MdTextButton>()
                      .CopyAddressTo(&refresh_button_)
                      .SetText(u"Refresh")
                      .SetCallback(base::BindRepeating(
                          &Shell::Reload, base::Unretained(shell_.get())))
                      .SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(base::BindRepeating(
                                       toolbar_button_rule))),
                  views::Builder<views::MdTextButton>()
                      .CopyAddressTo(&stop_button_)
                      .SetText(u"Stop")
                      .SetCallback(base::BindRepeating(
                          &Shell::Stop, base::Unretained(shell_.get())))
                      .SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(base::BindRepeating(
                                       toolbar_button_rule))),
                  views::Builder<views::Textfield>()
                      .CopyAddressTo(&url_entry_)
                      .SetAccessibleName(u"Enter URL")
                      .SetController(this)
                      .SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_URL)
                      .SetProperty(
                          views::kFlexBehaviorKey,
                          views::FlexSpecification(
                              views::MinimumFlexSizeRule::kScaleToMinimum,
                              views::MaximumFlexSizeRule::kUnbounded))
                      // Left padding  = 2, Right padding = 2
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(0, 2, 0, 2))));
    }

    builder.AddChild(views::Builder<views::View>()
                         .CopyAddressTo(&contents_view_)
                         .SetUseDefaultFillLayout(true)
                         .CustomConfigure(base::BindOnce([](views::View* view) {
                           if (!Shell::ShouldHideToolbar()) {
                             view->SetProperty(views::kMarginsKey,
                                               gfx::Insets::TLBR(0, 2, 0, 2));
                           }
                         })));

    if (!Shell::ShouldHideToolbar()) {
      builder.AddChild(views::Builder<views::View>().SetProperty(
          views::kMarginsKey, gfx::Insets::TLBR(0, 0, 5, 0)));
    }

    std::move(builder).BuildChildren();
    SetFlexForView(contents_view_, 1);
  }
  void InitAccelerators() {
    // This function must be called when part of the widget hierarchy.
    DCHECK(GetWidget());
    static const ui::KeyboardCode keys[] = {ui::VKEY_F5, ui::VKEY_BROWSER_BACK,
                                            ui::VKEY_BROWSER_FORWARD};
    for (size_t i = 0; i < std::size(keys); ++i) {
      GetFocusManager()->RegisterAccelerator(
          ui::Accelerator(keys[i], ui::EF_NONE),
          ui::AcceleratorManager::kNormalPriority, this);
    }
  }
  // Overridden from TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {}
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() == ui::ET_KEY_PRESSED && sender == url_entry_ &&
        key_event.key_code() == ui::VKEY_RETURN) {
      std::string text = base::UTF16ToUTF8(url_entry_->GetText());
      GURL url(text);
      if (!url.has_scheme()) {
        url = GURL(std::string("http://") + std::string(text));
        url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
      }
      shell_->LoadURL(url);
      return true;
    }
    return false;
  }

  // Overridden from View
  gfx::Size GetMinimumSize() const override {
    // We want to be able to make the window smaller than its initial
    // (preferred) size.
    return gfx::Size();
  }
  void AddedToWidget() override { InitAccelerators(); }

  // Overridden from AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    switch (accelerator.key_code()) {
      case ui::VKEY_F5:
        shell_->Reload();
        return true;
      case ui::VKEY_BROWSER_BACK:
        shell_->GoBackOrForward(-1);
        return true;
      case ui::VKEY_BROWSER_FORWARD:
        shell_->GoBackOrForward(1);
        return true;
      default:
        return views::View::AcceleratorPressed(accelerator);
    }
  }

 private:
  std::unique_ptr<Shell> shell_;

  // Window title
  std::u16string title_;

  // Toolbar view contains forward/backward/reload button and URL entry
  raw_ptr<views::View> toolbar_view_ = nullptr;
  raw_ptr<views::Button> back_button_ = nullptr;
  raw_ptr<views::Button> forward_button_ = nullptr;
  raw_ptr<views::Button> refresh_button_ = nullptr;
  raw_ptr<views::Button> stop_button_ = nullptr;
  raw_ptr<views::Textfield> url_entry_ = nullptr;

  // Contents view contains the web contents view
  raw_ptr<views::View> contents_view_ = nullptr;
  raw_ptr<views::WebView> web_view_ = nullptr;
};

BEGIN_METADATA(ShellView, views::View)
END_METADATA

ShellView* ShellViewForWidget(views::Widget* widget) {
  return static_cast<ShellView*>(widget->widget_delegate()->GetContentsView());
}

}  // namespace

ShellPlatformDelegate::ShellPlatformDelegate() = default;

std::unique_ptr<views::ViewsDelegate>
ShellPlatformDelegate::CreateViewsDelegate() {
  return std::make_unique<views::CobaltViewsDelegate>();
}

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size,
                                       bool is_visible) {
  is_visible_ = is_visible;
  platform_ = std::make_unique<PlatformData>();

  platform_->wm_state = std::make_unique<wm::WMState>();
  // FakeScreen tests create their own screen.
  if (!display::Screen::HasScreen()) {
    platform_->screen = views::CreateDesktopScreen();
  }

  platform_->views_delegate = CreateViewsDelegate();
}

ShellPlatformDelegate::~ShellPlatformDelegate() = default;

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  shell_data.content_size = initial_size;
  shell_data.initial_size_ = initial_size;

  if (IsVisible()) {
    CreatePlatformWindowInternal(shell, initial_size);
  } else {
    shell_data.window_widget = nullptr;
  }
}

void ShellPlatformDelegate::CreatePlatformWindowInternal(
    Shell* shell,
    const gfx::Size& initial_size) {
  ShellData& shell_data = shell_data_map_.at(shell);

  auto delegate = std::make_unique<views::WidgetDelegate>();
  delegate->SetContentsView(std::make_unique<ShellView>(shell));
  delegate->SetHasWindowSizeControls(true);
  delegate->SetOwnedByWidget(true);

  shell_data.window_widget = new views::Widget();
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(initial_size);
  params.delegate = delegate.release();
  params.wm_class_class = "chromium-content_shell";
  params.wm_class_name = params.wm_class_class;
  shell_data.window_widget->Init(std::move(params));

  // |window_widget| is made visible in PlatformSetContents(), so that the
  // platform-window size does not need to change due to layout again.
}

gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  return shell_data.window_widget->GetNativeWindow();
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell_data.window_widget) {
    ShellViewForWidget(shell_data.window_widget)
        ->SetWebContents(shell->web_contents(), shell_data.content_size);
    shell_data.window_widget->GetNativeWindow()->GetHost()->Show();
    shell_data.window_widget->Show();
  }
}

void ShellPlatformDelegate::RevealShell(Shell* shell) {
  ShellData& shell_data = shell_data_map_.at(shell);
  if (!shell_data.window_widget) {
    CreatePlatformWindowInternal(shell, shell_data.initial_size_);
  }

  if (IsVisible()) {
    SetContents(shell);
  }
}

void ShellPlatformDelegate::LoadSplashScreenContents(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell_data.window_widget) {
    ShellViewForWidget(shell_data.window_widget)
        ->SetWebContents(shell->splash_screen_web_contents(),
                         shell_data.content_size);
    shell_data.window_widget->GetNativeWindow()->GetHost()->Show();
    shell_data.window_widget->Show();
  }
}

void ShellPlatformDelegate::UpdateContents(Shell* shell) {
  SetContents(shell);
}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  shell->web_contents()->Resize(gfx::Rect(content_size));
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {
  if (Shell::ShouldHideToolbar()) {
    return;
  }

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell_data.window_widget) {
    auto* view = ShellViewForWidget(shell_data.window_widget);
    if (control == BACK_BUTTON) {
      view->EnableUIControl(ShellView::BACK_BUTTON, is_enabled);
    } else if (control == FORWARD_BUTTON) {
      view->EnableUIControl(ShellView::FORWARD_BUTTON, is_enabled);
    } else if (control == STOP_BUTTON) {
      view->EnableUIControl(ShellView::STOP_BUTTON, is_enabled);
    }
  }
}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {
  if (Shell::ShouldHideToolbar()) {
    return;
  }

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell_data.window_widget) {
    ShellViewForWidget(shell_data.window_widget)->SetAddressBarURL(url);
  }
}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const std::u16string& title) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell_data.window_widget) {
    shell_data.window_widget->widget_delegate()->SetTitle(title);
  }
}

void ShellPlatformDelegate::MainFrameCreated(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell_data.window_widget) {
    shell_data.window_widget->CloseNow();
    return true;  // The CloseNow() will do the destruction of Shell.
  }

  return false;
}

}  // namespace content
