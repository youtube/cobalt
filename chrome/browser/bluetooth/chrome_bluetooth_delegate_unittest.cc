// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"

#include <memory>

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate_impl_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class ChromeBluetoothDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  std::unique_ptr<content::WebContents> CreateGuestWebContents(
      const GURL& url) {
    const content::StoragePartitionConfig kGuestConfig =
        content::StoragePartitionConfig::Create(
            profile(), "test_partition", "guest_partition", /*in_memory=*/true);
    scoped_refptr<content::SiteInstance> guest_instance =
        content::SiteInstance::CreateForGuest(profile(), kGuestConfig);
    std::unique_ptr<content::WebContents> guest_contents =
        content::WebContentsTester::CreateTestWebContents(profile(),
                                                          guest_instance);
    content::WebContentsTester::For(guest_contents.get())
        ->NavigateAndCommit(url);
    return guest_contents;
  }
};

TEST_F(ChromeBluetoothDelegateTest, BlocksGuestViews) {
  ChromeBluetoothDelegate bluetooth_delegate(
      std::make_unique<ChromeBluetoothDelegateImplClient>());

  // 1. Test HTTPS Guest (should be blocked)
  {
    std::unique_ptr<content::WebContents> guest =
        CreateGuestWebContents(GURL("https://example.com/"));
    EXPECT_FALSE(
        bluetooth_delegate.MayUseBluetooth(guest->GetPrimaryMainFrame()));
  }

  // 2. Test about:blank Guest (should be blocked)
  {
    std::unique_ptr<content::WebContents> guest =
        CreateGuestWebContents(GURL("about:blank"));
    EXPECT_FALSE(
        bluetooth_delegate.MayUseBluetooth(guest->GetPrimaryMainFrame()));
  }

  // 3. Test Non-Guest (should be allowed by default)
  {
    std::unique_ptr<content::WebContents> non_guest =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContentsTester::For(non_guest.get())
        ->NavigateAndCommit(GURL("https://example.com/"));
    EXPECT_TRUE(
        bluetooth_delegate.MayUseBluetooth(non_guest->GetPrimaryMainFrame()));
  }
}

}  // namespace
