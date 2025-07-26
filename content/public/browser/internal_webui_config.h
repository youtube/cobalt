// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INTERNAL_WEBUI_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_INTERNAL_WEBUI_CONFIG_H_

#include <string>
#include <string_view>
#include <type_traits>

#include "base/functional/function_ref.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUI;
class WebUIController;

// Returns whether `url` has been registered as an internal debugging WebUI
// page.
CONTENT_EXPORT bool IsInternalWebUI(const GURL& url);

// This subclass of WebUIConfig returns the value of the current embedder's
// implementation of ContentBrowserClient::OverrideForInternalWebUI(). It also
// registers the URL as belonging to an internal debugging WebUI. This class
// should be overridden by internal debugging UIs that are intended for use by
// Chromium developer teams only, so that embedders can choose to control access
// to such UIs for end users.
class CONTENT_EXPORT InternalWebUIConfig : public WebUIConfig {
 public:
  explicit InternalWebUIConfig(std::string_view host);
  ~InternalWebUIConfig() override;

  std::unique_ptr<WebUIController> CreateWebUIController(
      WebUI* web_ui,
      const GURL& url) override;
};

// Extends InternalWebUIConfig and returns the value of
// InternalWebUIConfig::CreateWebUIController() if it is non-null. Otherwise,
// returns a unique_ptr to a new instance of T.
template <typename T>
class CONTENT_EXPORT DefaultInternalWebUIConfig : public InternalWebUIConfig {
 public:
  explicit DefaultInternalWebUIConfig(std::string_view host)
      : InternalWebUIConfig(host) {}

  // InternalWebUIConfig:
  std::unique_ptr<WebUIController> CreateWebUIController(
      WebUI* web_ui,
      const GURL& url) override {
    std::unique_ptr<WebUIController> default_controller =
        InternalWebUIConfig::CreateWebUIController(web_ui, url);
    if (default_controller) {
      return default_controller;
    }

    // Disallow dual constructibility.
    // The controller can be constructed either by T(WebUI*) or
    // T(WebUI*, const GURL&), but not both.
    static_assert(std::is_constructible_v<T, WebUI*> ^
                  std::is_constructible_v<T, WebUI*, const GURL&>);
    if constexpr (std::is_constructible_v<T, WebUI*>) {
      return std::make_unique<T>(web_ui);
    }
    if constexpr (std::is_constructible_v<T, WebUI*, const GURL&>) {
      return std::make_unique<T>(web_ui, url);
    }
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INTERNAL_WEBUI_CONFIG_H_
