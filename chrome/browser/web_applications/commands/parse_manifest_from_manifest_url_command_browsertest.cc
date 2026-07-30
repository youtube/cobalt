// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/parse_manifest_from_manifest_url_command.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class ParseManifestFromManifestUrlCommandTest : public WebAppBrowserTestBase {
 public:
  blink::mojom::ManifestPtr ParseAndAwaitResult(
      const GURL& manifest_url,
      const std::string& manifest_contents) {
    base::test::TestFuture<blink::mojom::ManifestPtr> future;
    provider().command_manager().ScheduleCommand(
        std::make_unique<ParseManifestFromManifestUrlCommand>(
            manifest_url, manifest_contents, future.GetCallback()));
    return future.Take();
  }
};

// A valid manifest with a relative start_url resolves against manifest_url.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       RelativeStartUrlResolvesAgainstManifestUrl) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "/start"
  })json";

  auto manifest = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_TRUE(manifest);
  EXPECT_EQ(manifest->start_url, GURL("https://example.com/start"));
  EXPECT_EQ(manifest->name, u"Test App");
}

// A relative start_url without leading slash resolves relative to the manifest
// file's directory.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       RelativeStartUrlResolvesFromManifestDirectory) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "index.html"
  })json";

  auto manifest = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_TRUE(manifest);
  EXPECT_EQ(manifest->start_url, GURL("https://example.com/app/index.html"));
}

// A cross-origin absolute start_url is rejected by ManifestParser. The
// resulting manifest has no valid start_url, causing the command to fail.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       CrossOriginStartUrlFails) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "https://evil.com/start"
  })json";

  auto manifest = ParseAndAwaitResult(kManifestUrl, kManifest);

  // ManifestParser rejects cross-origin start_url, leaving has_valid_specified_
  // start_url false. The command sees this and returns nullptr.
  EXPECT_FALSE(manifest);
}

// Invalid JSON causes the command to return nullptr.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       InvalidJsonFails) {
  const GURL kManifestUrl("https://example.com/manifest.json");
  const std::string kManifest = "this is not json {{{";

  auto manifest = ParseAndAwaitResult(kManifestUrl, kManifest);

  EXPECT_FALSE(manifest);
}

// A manifest with no name or short_name fails the required fields check.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       MissingNameFails) {
  const GURL kManifestUrl("https://example.com/manifest.json");
  const std::string kManifest = R"json({
    "start_url": "/"
  })json";

  auto manifest = ParseAndAwaitResult(kManifestUrl, kManifest);

  EXPECT_FALSE(manifest);
}

// A valid manifest with an explicit id field preserves has_custom_id.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       ManifestWithCustomId) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "/",
    "id": "/my-app-id"
  })json";

  auto manifest = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_TRUE(manifest);
  EXPECT_TRUE(manifest->has_custom_id);
  EXPECT_EQ(manifest->id, GURL("https://example.com/my-app-id"));
}

// A valid manifest without an explicit id field has has_custom_id = false.
IN_PROC_BROWSER_TEST_F(ParseManifestFromManifestUrlCommandTest,
                       ManifestWithoutCustomId) {
  const GURL kManifestUrl("https://example.com/app/manifest.json");
  const std::string kManifest = R"json({
    "name": "Test App",
    "start_url": "/"
  })json";

  auto manifest = ParseAndAwaitResult(kManifestUrl, kManifest);

  ASSERT_TRUE(manifest);
  EXPECT_FALSE(manifest->has_custom_id);
}

}  // namespace
}  // namespace web_app
