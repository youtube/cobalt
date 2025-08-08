// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller_chromeos_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "chromeos/crosapi/mojom/sharesheet_mojom_traits.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

class FakeSharesheet : public crosapi::mojom::Sharesheet {
 public:
  FakeSharesheet() = default;
  FakeSharesheet(const FakeSharesheet&) = delete;
  FakeSharesheet& operator=(const FakeSharesheet&) = delete;
  ~FakeSharesheet() override = default;

  void AwaitShow() {
    base::RunLoop run_loop;
    show_bubble_called_.Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void AwaitClose() {
    base::RunLoop run_loop;
    close_bubble_called_.Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  // crosapi::mojom::Sharesheet:
  void ShowBubble(
      const std::string& window_id,
      sharesheet::LaunchSource source,
      crosapi::mojom::IntentPtr intent,
      crosapi::mojom::Sharesheet::ShowBubbleCallback callback) override {
    NOTREACHED();
  }
  void ShowBubbleWithOnClosed(
      const std::string& window_id,
      sharesheet::LaunchSource source,
      crosapi::mojom::IntentPtr intent,
      crosapi::mojom::Sharesheet::ShowBubbleWithOnClosedCallback callback)
      override {
    show_bubble_called_.Signal();
    callback_ = std::move(callback);
  }
  void CloseBubble(const std::string& window_id) override {
    close_bubble_called_.Signal();
    std::move(callback_).Run();
  }

 public:
  base::OneShotEvent show_bubble_called_;
  base::OneShotEvent close_bubble_called_;
  crosapi::mojom::Sharesheet::ShowBubbleWithOnClosedCallback callback_;
};

class SharingHubBubbleControllerChromeOsBrowserTest
    : public InProcessBrowserTest {
 public:
  SharingHubBubbleControllerChromeOsBrowserTest() = default;
  ~SharingHubBubbleControllerChromeOsBrowserTest() override = default;

  // Lacros tests may be run with an old version of ash-chrome where the lacros
  // service or the sharesheet interface are not available.
  bool IsServiceAvailable() {
    auto* const service = chromeos::LacrosService::Get();
    return service && service->IsAvailable<crosapi::mojom::Sharesheet>();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // If the lacros service or the sharesheet interface are not
    // available on this version of ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable())
      return;

    // Replace the production sharesheet with a fake for testing.
    mojo::Remote<crosapi::mojom::Sharesheet>& remote =
        chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Sharesheet>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

 protected:
  FakeSharesheet service_;
  mojo::Receiver<crosapi::mojom::Sharesheet> receiver_{&service_};
};

IN_PROC_BROWSER_TEST_F(SharingHubBubbleControllerChromeOsBrowserTest,
                       OpenSharesheet_Lacros) {
  auto* const service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::Sharesheet>())
    return;

  // Open the sharesheet using the sharing hub controller.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  sharing_hub::SharingHubBubbleControllerChromeOsImpl::
      CreateOrGetFromWebContents(web_contents)
          ->ShowBubble(share::ShareAttempt(web_contents));

  // Verify that the sharesheet is opened.
  service_.AwaitShow();

  // Close the sharesheet using the sharing hub controller.
  sharing_hub::SharingHubBubbleControllerChromeOsImpl::
      CreateOrGetFromWebContents(web_contents)
          ->HideBubble();

  // Verify that the sharesheet is closed.
  service_.AwaitClose();
}

}  // namespace
