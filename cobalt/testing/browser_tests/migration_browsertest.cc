// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_restrictions.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/migrate_storage_record/migration_manager.h"
#include "cobalt/shell/browser/migrate_storage_record/storage.pb.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "starboard/common/storage.h"
#include "url/gurl.h"

namespace content {

namespace {

std::string GetApplicationKey(const GURL& url) {
  std::string encoded_url;
  base::Base64UrlEncode(url_matcher::util::Normalize(url).spec(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_url);
  return encoded_url;
}

}  // namespace

class MigrationBrowserTest : public ContentBrowserTest {
 public:
  MigrationBrowserTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    content::RenderFrameHost::AllowInjectingJavaScript();
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    home_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_HOME, temp_dir_.GetPath());
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("migration.test", "127.0.0.1");
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> home_override_;
  base::test::ScopedCommandLine scoped_command_line_;
};

IN_PROC_BROWSER_TEST_F(MigrationBrowserTest, MigrateCookiesAndLocalStorage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url =
      embedded_test_server()->GetURL("migration.test", "/title1.html");

  // 1. Prepare legacy storage record.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string app_key = GetApplicationKey(test_url);
    starboard::StorageRecord record(app_key.c_str());

    cobalt::storage::Storage storage;

    // Add cookies.
    auto* cookie1 = storage.add_cookies();
    cookie1->set_name("migrated_cookie");
    cookie1->set_value("cookie_value");
    cookie1->set_domain("migration.test");
    cookie1->set_path("/");
    cookie1->set_secure(false);
    cookie1->set_http_only(false);

    auto* cookie2 = storage.add_cookies();
    cookie2->set_name("dot_cookie");
    cookie2->set_value("dot_value");
    cookie2->set_domain(".migration.test");
    cookie2->set_path("/");
    cookie2->set_secure(false);
    cookie2->set_http_only(false);

    // Add local storage.
    auto* local_storage = storage.add_local_storages();
    local_storage->set_serialized_origin(
        url::Origin::Create(test_url).Serialize());
    auto* entry = local_storage->add_local_storage_entries();
    entry->set_key("migrated_key");
    entry->set_value("migrated_value");

    std::string serialized;
    ASSERT_TRUE(storage.SerializeToString(&serialized));

    std::string data = "SAV1" + serialized;
    ASSERT_TRUE(record.Write(data.data(), data.size()));
  }

  // 2. Set the initial URL switch and enable insecure migration for testing.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      cobalt::switches::kInitialURL, test_url.spec());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableInsecureDomainForMigrationTesting);

  // 3. Trigger migration and wait for the reload.
  // Migration ends with a ReloadTask. We expect 2 navigations:
  // 1. The initial navigation to test_url which triggers migration.
  // 2. The reload triggered by migration after tasks finish.
  TestNavigationObserver reload_observer(shell()->web_contents(), 2);
  shell()->LoadURL(test_url);
  reload_observer.Wait();

  // 4. Verify cookie migration.
  std::string cookies = EvalJs(shell(), "document.cookie").ExtractString();
  EXPECT_THAT(cookies, ::testing::HasSubstr("migrated_cookie=cookie_value"));
  EXPECT_THAT(cookies, ::testing::HasSubstr("dot_cookie=dot_value"));

  // 5. Verify local storage migration.
  EXPECT_EQ("migrated_value",
            EvalJs(shell(), "localStorage.getItem('migrated_key')"));
}

}  // namespace content
