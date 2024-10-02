// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_H_
#define WEBLAYER_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace weblayer {

class WebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  WebUIControllerFactory(const WebUIControllerFactory&) = delete;
  WebUIControllerFactory& operator=(const WebUIControllerFactory&) = delete;

  static WebUIControllerFactory* GetInstance();

  // content::WebUIControllerFactory overrides
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  friend base::NoDestructor<WebUIControllerFactory>;

  WebUIControllerFactory();
  ~WebUIControllerFactory() override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_H_
