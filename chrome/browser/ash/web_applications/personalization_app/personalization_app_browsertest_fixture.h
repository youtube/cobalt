// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_BROWSERTEST_FIXTURE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_BROWSERTEST_FIXTURE_H_

#include <memory>

#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"

namespace ash::personalization_app {

class TestPersonalizationAppWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override;
};

class PersonalizationAppBrowserTestFixture : public MojoWebUIBrowserTest {
 public:
  PersonalizationAppBrowserTestFixture();

  PersonalizationAppBrowserTestFixture(
      const PersonalizationAppBrowserTestFixture&) = delete;
  PersonalizationAppBrowserTestFixture& operator=(
      const PersonalizationAppBrowserTestFixture&) = delete;

  ~PersonalizationAppBrowserTestFixture() override;

  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

 private:
  raw_ptr<TestWallpaperImageDownloader, DanglingUntriaged>
      test_wallpaper_image_downloader_;
  TestChromeWebUIControllerFactory test_factory_;
  TestPersonalizationAppWebUIProvider test_web_ui_provider_;
  content::ScopedWebUIControllerFactoryRegistration
      scoped_controller_factory_registration_{&test_factory_};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_BROWSERTEST_FIXTURE_H_
