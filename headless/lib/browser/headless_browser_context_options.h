// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "ui/gfx/font_render_params.h"

namespace headless {

// Represents options which can be customized for a given BrowserContext.
// Provides a fallback to default browser options when an option is not set for
// a particular BrowserContext.
class HeadlessBrowserContextOptions {
 public:
  HeadlessBrowserContextOptions(HeadlessBrowserContextOptions&& options);

  HeadlessBrowserContextOptions(const HeadlessBrowserContextOptions&) = delete;
  HeadlessBrowserContextOptions& operator=(
      const HeadlessBrowserContextOptions&) = delete;

  ~HeadlessBrowserContextOptions();

  HeadlessBrowserContextOptions& operator=(
      HeadlessBrowserContextOptions&& options);

  // The following options fallback to HeadlessBrowser::Options defaults if they
  // are not set in HeadlessBrowserContext::CreateParams.
  const std::string& accept_language() const;
  const std::string& user_agent() const;
  const net::ProxyConfig* proxy_config() const;
  const gfx::Size& window_size() const;
  const base::FilePath& user_data_dir() const;
  const base::FilePath& disk_cache_dir() const;
  bool incognito_mode() const;
  bool block_new_web_contents() const;
  gfx::FontRenderParams::Hinting font_render_hinting() const;

 public:
  HeadlessBrowserContextOptions(HeadlessBrowser::Options* options,
                                HeadlessBrowserContext::CreateParams params);

 private:
  raw_ptr<HeadlessBrowser::Options> browser_options_;

  HeadlessBrowserContext::CreateParams create_params_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_
