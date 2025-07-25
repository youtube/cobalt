// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_REGISTRY_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_REGISTRY_H_

#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace content {

// A singleton which holds on to all the registered WebUIControllerFactory
// instances.
class CONTENT_EXPORT WebUIControllerFactoryRegistry
    : public WebUIControllerFactory {
 public:
  static WebUIControllerFactoryRegistry* GetInstance();

  WebUIControllerFactoryRegistry(const WebUIControllerFactoryRegistry&) =
      delete;
  WebUIControllerFactoryRegistry& operator=(
      const WebUIControllerFactoryRegistry&) = delete;

  // WebUIControllerFactory implementation. Each method loops through the same
  // method on all the factories.
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override;
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override;
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override;

  // Returns true if the given URL can be loaded by Web UI system. This allows
  // URLs that UseWebUIForURL returns true for, and also URLs that can be loaded
  // by normal tabs such as javascript: URLs or about:hang.
  bool IsURLAcceptableForWebUI(BrowserContext* browser_context,
                               const GURL& url);

 private:
  friend struct base::DefaultSingletonTraits<WebUIControllerFactoryRegistry>;

  WebUIControllerFactoryRegistry();
  ~WebUIControllerFactoryRegistry() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_REGISTRY_H_
