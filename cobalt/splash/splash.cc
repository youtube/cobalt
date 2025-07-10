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
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#elif defined(USE_AURA)
#include "ui/aura/window.h"
#endif

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
  content::NavigationController::LoadURLParams load_url_params(url);
  web_contents->GetController().LoadURLWithParams(load_url_params);
  web_contents->Resize(container->bounds());
#if BUILDFLAG(IS_ANDROID)
  // Not visible for android.
  LOG(WARNING) << "#############\n#############\n#############\n Android "
                  "showing splash!!!";
  web_contents->WasShown();
#elif defined(USE_AURA)
  web_contents->GetNativeView()->Show();
#endif

  return web_contents;
}

}  // namespace cobalt
