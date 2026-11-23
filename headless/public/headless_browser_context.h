// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "headless/public/headless_export.h"
#include "headless/public/headless_web_contents.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

// Represents an isolated session with a unique cache, cookies, and other
// profile/session related data.
// When browser context is deleted, all associated web contents are closed.
class HEADLESS_EXPORT HeadlessBrowserContext {
 public:
  struct HEADLESS_EXPORT CreateParams {
   public:
    CreateParams();
    ~CreateParams();

    CreateParams(const CreateParams&) = delete;
    CreateParams& operator=(const CreateParams&) = delete;
    CreateParams(CreateParams&&);
    CreateParams& operator=(CreateParams&&);

    std::optional<std::string> accept_language;
    std::optional<std::string> user_agent;
    std::unique_ptr<net::ProxyConfig> proxy_config;
    std::optional<gfx::Size> window_size;
    std::optional<base::FilePath> user_data_dir;
    std::optional<base::FilePath> disk_cache_dir;
    std::optional<bool> incognito_mode;
    std::optional<bool> block_new_web_contents;
    std::optional<gfx::FontRenderParams::Hinting> font_render_hinting;
  };

  HeadlessBrowserContext(const HeadlessBrowserContext&) = delete;
  HeadlessBrowserContext& operator=(const HeadlessBrowserContext&) = delete;

  virtual ~HeadlessBrowserContext() {}

  // Open a new tab.
  // Pointer to HeadlessWebContents becomes invalid after:
  // a) Calling HeadlessWebContents::Close, or
  // b) Calling HeadlessBrowserContext::Close on associated browser context, or
  // c) Calling HeadlessBrowser::Shutdown.
  virtual HeadlessWebContents* CreateWebContents(
      const HeadlessWebContents::CreateParams& params) = 0;
  virtual HeadlessWebContents* CreateWebContents(const GURL& initial_url) = 0;
  virtual HeadlessWebContents* CreateWebContents() = 0;

  // Returns all web contents owned by this browser context.
  virtual std::vector<HeadlessWebContents*> GetAllWebContents() = 0;

  // Destroy this BrowserContext and all WebContents associated with it.
  virtual void Close() = 0;

  // GUID for this browser context.
  virtual const std::string& Id() = 0;

 protected:
  HeadlessBrowserContext() {}
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_CONTEXT_H_
