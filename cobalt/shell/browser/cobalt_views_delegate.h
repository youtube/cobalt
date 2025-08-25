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

#ifndef COBALT_SHELL_BROWSER_COBALT_VIEWS_DELEGATE_H_
#define COBALT_SHELL_BROWSER_COBALT_VIEWS_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/views_delegate.h"

namespace views {

class CobaltViewsDelegate : public ViewsDelegate {
 public:
  CobaltViewsDelegate();

  CobaltViewsDelegate(const CobaltViewsDelegate&) = delete;
  CobaltViewsDelegate& operator=(const CobaltViewsDelegate&) = delete;

  ~CobaltViewsDelegate() override;

  // If set to |true|, forces widgets that do not provide a native widget to use
  // DesktopNativeWidgetAura instead of whatever the default native widget would
  // be. This has no effect on ChromeOS.
  void set_use_desktop_native_widgets(bool desktop) {
    use_desktop_native_widgets_ = desktop;
  }

  void set_use_transparent_windows(bool transparent) {
    use_transparent_windows_ = transparent;
  }

  // For convenience, we create a layout provider by default, but embedders
  // that use their own layout provider subclasses may need to set those classes
  // as the layout providers for their tests.
  void set_layout_provider(std::unique_ptr<LayoutProvider> layout_provider) {
    layout_provider_.swap(layout_provider);
  }

  // ViewsDelegate:
  void OnBeforeWidgetInit(Widget::InitParams* params,
                          internal::NativeWidgetDelegate* delegate) override;

 private:
  bool use_desktop_native_widgets_ = false;
  bool use_transparent_windows_ = false;
  std::unique_ptr<LayoutProvider> layout_provider_ =
      std::make_unique<LayoutProvider>();
};

}  // namespace views

#endif  // COBALT_SHELL_BROWSER_COBALT_VIEWS_DELEGATE_H_
