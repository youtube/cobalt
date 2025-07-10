// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/splash/splash.h"

#include "base/logging.h"
// #include "starboard/player.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cobalt {

// static
std::unique_ptr<content::WebContents> Splash::Show(
    content::BrowserContext* browser_context,
    gfx::NativeView container,
    const GURL& url) {
  LOG(WARNING) << "#############\n#############\n#############\n splash url: "
               << url;

  content::WebContents::CreateParams create_params(browser_context);
  create_params.context = container;
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(create_params));
  // GURL url(url_spec);
  content::NavigationController::LoadURLParams load_url_params(url);
  web_contents->GetController().LoadURLWithParams(load_url_params);
  web_contents->GetRenderWidgetHostView()->SetBounds(gfx::Rect(0, 0, 500, 500));
  container->AddChild(web_contents->GetNativeView());
  web_contents->GetNativeView()->Show();
  container->StackChildAtTop(web_contents->GetNativeView());
  web_contents->GetRenderWidgetHostView()->Show();
  web_contents->GetRenderWidgetHostView()->Focus();
  return web_contents;
}

}  // namespace cobalt
