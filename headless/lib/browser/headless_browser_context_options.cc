// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_context_options.h"

#include <string>
#include <utility>

namespace headless {

namespace {

template <class T>
const T& GetValueOr(const std::optional<T>& value, const T& default_value) {
  return value ? *value : default_value;
}

template <class T>
const T* GetValueOr(const T* value, const T* default_value) {
  return value ? value : default_value;
}

}  // namespace

HeadlessBrowserContextOptions::HeadlessBrowserContextOptions(
    HeadlessBrowserContextOptions&& options) = default;

HeadlessBrowserContextOptions::~HeadlessBrowserContextOptions() = default;

HeadlessBrowserContextOptions& HeadlessBrowserContextOptions::operator=(
    HeadlessBrowserContextOptions&& options) = default;

HeadlessBrowserContextOptions::HeadlessBrowserContextOptions(
    HeadlessBrowser::Options* options,
    HeadlessBrowserContext::CreateParams params)
    : browser_options_(options), create_params_(std::move(params)) {}

const std::string& HeadlessBrowserContextOptions::accept_language() const {
  return GetValueOr(create_params_.accept_language,
                    browser_options_->accept_language);
}

const std::string& HeadlessBrowserContextOptions::user_agent() const {
  return GetValueOr(create_params_.user_agent, browser_options_->user_agent);
}

const net::ProxyConfig* HeadlessBrowserContextOptions::proxy_config() const {
  return GetValueOr(create_params_.proxy_config.get(),
                    browser_options_->proxy_config.get());
}

const gfx::Size& HeadlessBrowserContextOptions::window_size() const {
  return GetValueOr(create_params_.window_size, browser_options_->window_size);
}

const base::FilePath& HeadlessBrowserContextOptions::user_data_dir() const {
  return GetValueOr(create_params_.user_data_dir,
                    browser_options_->user_data_dir);
}

const base::FilePath& HeadlessBrowserContextOptions::disk_cache_dir() const {
  return GetValueOr(create_params_.disk_cache_dir,
                    browser_options_->disk_cache_dir);
}

bool HeadlessBrowserContextOptions::incognito_mode() const {
  return GetValueOr(create_params_.incognito_mode,
                    browser_options_->incognito_mode);
}

bool HeadlessBrowserContextOptions::block_new_web_contents() const {
  return GetValueOr(create_params_.block_new_web_contents,
                    browser_options_->block_new_web_contents);
}

gfx::FontRenderParams::Hinting
HeadlessBrowserContextOptions::font_render_hinting() const {
  return GetValueOr(create_params_.font_render_hinting,
                    browser_options_->font_render_hinting);
}

}  // namespace headless
