// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/aura/content_window.h"

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/wm/core/compound_event_filter.h"

namespace webui_examples {

namespace {

class QuitOnClose : public aura::WindowTreeHostObserver {
 public:
  explicit QuitOnClose(base::OnceClosure window_close_requested)
      : window_close_requested_(std::move(window_close_requested)) {}
  QuitOnClose(const QuitOnClose&) = delete;
  QuitOnClose& operator=(const QuitOnClose&) = delete;
  ~QuitOnClose() override = default;

  void OnHostCloseRequested(aura::WindowTreeHost* host) override {
    std::move(window_close_requested_).Run();
  }

 private:
  base::OnceClosure window_close_requested_;
};

}  // namespace

ContentWindow::ContentWindow(AuraContext* aura_context,
                             content::BrowserContext* browser_context) {
  host_ = aura_context->CreateWindowTreeHost();

  // Cursor support.
  root_window_event_filter_ = std::make_unique<wm::CompoundEventFilter>();

  content::WebContents::CreateParams params(browser_context);
  web_contents_ = content::WebContents::Create(params);
  aura::Window* web_contents_window = web_contents_->GetNativeView();
  aura::WindowTreeHost* window_tree_host = host_->window_tree_host();
  window_tree_host->window()->GetRootWindow()->AddChild(web_contents_window);
  window_tree_host->window()->GetRootWindow()->AddPreTargetHandler(
      root_window_event_filter_.get());
}

ContentWindow::~ContentWindow() = default;

void ContentWindow::SetTitle(const std::u16string& title) {
  aura::WindowTreeHost* window_tree_host = host_->window_tree_host();
  window_tree_host->window()->SetTitle(title);
  static_cast<aura::WindowTreeHostPlatform*>(window_tree_host)
      ->platform_window()
      ->SetTitle(title);
}

void ContentWindow::Show() {
  web_contents_->GetNativeView()->Show();
  aura::WindowTreeHost* window_tree_host = host_->window_tree_host();
  window_tree_host->window()->Show();
  window_tree_host->Show();
}

void ContentWindow::NavigateToURL(GURL url) {
  content::NavigationController::LoadURLParams url_params(url);
  web_contents_->GetController().LoadURLWithParams(url_params);
}

void ContentWindow::SetCloseCallback(base::OnceClosure on_close) {
  host_->window_tree_host()->AddObserver(new QuitOnClose(std::move(on_close)));
}

}  // namespace webui_examples
