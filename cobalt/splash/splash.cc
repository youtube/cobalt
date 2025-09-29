// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/splash/splash.h"

#include "base/logging.h"
// #include "starboard/player.h"
#include "cobalt/shell/browser/shell.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
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

#if BUILDFLAG(IS_ANDROIDTV)
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "ui/android/view_android.h"

using ::starboard::StarboardBridge;
#endif

namespace cobalt {

// static
std::unique_ptr<content::WebContents> Splash::Show(
    content::BrowserContext* browser_context,
    gfx::NativeView container,
    const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  LOG(WARNING) << "#############\n#############\n#############\n splash url: "
               << url;
  auto* source = content::Shell::windows()[0]->web_contents();
  LOG(WARNING) << "#############\n#############\n#############\n splash url: "
               << url;
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_WINDOW,
                                ui::PAGE_TRANSITION_TYPED, false);
  LOG(WARNING) << "#############\n#############\n#############\n splash url: "
               << url;
  auto* web_contents =
      content::Shell::windows()[0]->OpenURLFromTab(source, params);
  LOG(WARNING) << "#############\n#############\n#############\n splash url: "
               << url;
  return nullptr;
#else
  LOG(WARNING) << "#############\n#############\n#############\n splash url: "
               << url;

  content::WebContents::CreateParams create_params(browser_context);
  create_params.context = container;
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(create_params));
  content::NavigationController::LoadURLParams load_url_params(url);
  web_contents->GetController().LoadURLWithParams(load_url_params);
  web_contents->Resize(container->bounds());
  web_contents->GetNativeView()->Show();
  return web_contents;
#endif

  // #if BUILDFLAG(IS_ANDROID)
  //   // Not visible for android.
  //   LOG(WARNING) << "#############\n#############\n#############\n Android "
  //                   "showing splash!!!";
  //   // web_contents->WasShown();

  //   // content::Shell::windows()[0]->AddNewContents(nullptr,
  //   // std::move(web_contents),
  //   //                     url,
  //   //                     WindowOpenDisposition::NEW_FOREGROUND_TAB,
  //   //                     blink::mojom::WindowFeatures(),
  //   //                     false,
  //   //                     nullptr);
  //   JNIEnv* env = base::android::AttachCurrentThread();
  //   LOG(INFO) << "Show splash!!!     4";
  //   StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  //   LOG(INFO) << "Show splash!!!     5";
  //   auto* s = web_contents.release();
  //   LOG(INFO) << "Show splash!!!     6";
  //   starboard_bridge->SetSplashWebContents(env, s->GetJavaWebContents());
  //   return nullptr;
  // #elif defined(USE_AURA)
  //   web_contents->GetNativeView()->Show();
  // #endif
  //   LOG(WARNING) << "#############\n#############\n#############\n Android "
  //                   "showing splash!!! 2";
  //   return web_contents;
}

}  // namespace cobalt
