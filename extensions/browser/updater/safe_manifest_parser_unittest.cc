// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/safe_manifest_parser.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace {

class ExtensionUpdateManifestTest : public testing::Test {
 public:
  void TestParseUpdateManifest(const std::string& xml) {
    base::RunLoop run_loop;
    ParseUpdateManifest(
        xml,
        base::BindOnce(&ExtensionUpdateManifestTest::OnUpdateManifestParsed,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  UpdateManifestResults* results() const { return results_.get(); }
  const absl::optional<ManifestParseFailure>& parse_error() const {
    return parse_error_;
  }

  void ExpectNoError() {
    EXPECT_FALSE(parse_error_)
        << "Unexpected error: '" << parse_error_.value().error_detail;
  }

 private:
  void OnUpdateManifestParsed(
      base::OnceClosure quit_loop,
      std::unique_ptr<UpdateManifestResults> results,
      const absl::optional<ManifestParseFailure>& failure) {
    results_ = std::move(results);
    parse_error_ = failure;
    std::move(quit_loop).Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<UpdateManifestResults> results_;
  absl::optional<ManifestParseFailure> parse_error_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

}  // namespace

TEST_F(ExtensionUpdateManifestTest, InvalidXml) {
  TestParseUpdateManifest(std::string());
  EXPECT_FALSE(results());
  EXPECT_TRUE(parse_error());
  EXPECT_EQ(parse_error().value().error,
            ManifestInvalidError::XML_PARSING_FAILED);
}

TEST_F(ExtensionUpdateManifestTest, MissingAppId) {
  TestParseUpdateManifest(
      "<?xml version='1.0'?>"
      "<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
      " <app>"
      "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.2.3.4' />"
      " </app>"
      "</gupdate>");
  ASSERT_EQ(1u, results()->update_list.size());
  EXPECT_TRUE(results()->update_list.at(0).parse_error);
  EXPECT_EQ(results()->update_list.at(0).parse_error.value().error,
            ManifestInvalidError::MISSING_APP_ID);
  EXPECT_FALSE(parse_error());
}

TEST_F(ExtensionUpdateManifestTest, InvalidCodebase) {
  TestParseUpdateManifest(CreateUpdateManifest(
      {UpdateManifestItem("12345").version("1.2.3.4").codebase(
          "example.com/extension_1.2.3.4.crx")}));
  ASSERT_EQ(1u, results()->update_list.size());
  EXPECT_TRUE(results()->update_list.at(0).parse_error);
  EXPECT_EQ(results()->update_list.at(0).parse_error.value().error,
            ManifestInvalidError::EMPTY_CODEBASE_URL);
  EXPECT_FALSE(parse_error());
}

TEST_F(ExtensionUpdateManifestTest, MissingVersion) {
  TestParseUpdateManifest(
      CreateUpdateManifest({UpdateManifestItem("12345").codebase(
          "http://example.com/extension_1.2.3.4.crx")}));
  ASSERT_EQ(1u, results()->update_list.size());
  EXPECT_TRUE(results()->update_list.at(0).parse_error);
  EXPECT_EQ(results()->update_list.at(0).parse_error.value().error,
            ManifestInvalidError::MISSING_VERSION_FOR_UPDATE_CHECK);
  EXPECT_FALSE(parse_error());
}

TEST_F(ExtensionUpdateManifestTest, InvalidVersion) {
  TestParseUpdateManifest(CreateUpdateManifest(
      {UpdateManifestItem("12345").version("1.2.3.a").codebase(
          "http://example.com/extension_1.2.3.4.crx")}));
  ASSERT_EQ(1u, results()->update_list.size());
  EXPECT_TRUE(results()->update_list.at(0).parse_error);
  EXPECT_EQ(results()->update_list.at(0).parse_error.value().error,
            ManifestInvalidError::INVALID_VERSION);
  EXPECT_FALSE(parse_error());
}

TEST_F(ExtensionUpdateManifestTest, ValidXml) {
  TestParseUpdateManifest(CreateUpdateManifest(
      {UpdateManifestItem("12345")
           .version("1.2.3.4")
           .prodversionmin("2.0.143.0")
           .codebase("http://example.com/extension_1.2.3.4.crx")}));
  ExpectNoError();
  ASSERT_TRUE(results());
  EXPECT_EQ(1U, results()->update_list.size());
  const UpdateManifestResult& first_result = results()->update_list.at(0);
  EXPECT_FALSE(first_result.parse_error);
  EXPECT_EQ(GURL("http://example.com/extension_1.2.3.4.crx"),
            first_result.crx_url);
  EXPECT_EQ("1.2.3.4", first_result.version);
  EXPECT_EQ("2.0.143.0", first_result.browser_min_version);
}

TEST_F(ExtensionUpdateManifestTest, ValidXmlWithNamespacePrefix) {
  TestParseUpdateManifest(
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<g:gupdate xmlns:g='http://www.google.com/update2/response'"
      "           protocol='2.0'>"
      " <g:app appid='12345'>"
      "  <g:updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.2.3.4' prodversionmin='2.0.143.0' />"
      " </g:app>"
      "</g:gupdate>");
  ExpectNoError();
  ASSERT_TRUE(results());
  EXPECT_EQ(1U, results()->update_list.size());
  const UpdateManifestResult& first_result = results()->update_list.at(0);
  EXPECT_FALSE(first_result.parse_error);
  EXPECT_EQ(GURL("http://example.com/extension_1.2.3.4.crx"),
            first_result.crx_url);
  EXPECT_EQ("1.2.3.4", first_result.version);
  EXPECT_EQ("2.0.143.0", first_result.browser_min_version);
}

TEST_F(ExtensionUpdateManifestTest, SimilarTagnames) {
  // Includes unrelated <app> tags from other xml namespaces.
  // This should not cause problems, the unrelated app tags should be ignored.
  TestParseUpdateManifest(
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response'"
      "         xmlns:a='http://a' protocol='2.0'>"
      " <a:app/>"
      " <b:app xmlns:b='http://b' />"
      " <app appid='12345'>"
      "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.2.3.4' prodversionmin='2.0.143.0' />"
      " </app>"
      " <a:app appid='12345'>"
      "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.2.3.4' prodversionmin='2.0.143.0' />"
      " </a:app>"
      "</gupdate>");
  ExpectNoError();
  ASSERT_TRUE(results());
  // We should still have parsed the gupdate app tag.
  EXPECT_EQ(1U, results()->update_list.size());
  EXPECT_FALSE(results()->update_list.at(0).parse_error);
  EXPECT_EQ(GURL("http://example.com/extension_1.2.3.4.crx"),
            results()->update_list.at(0).crx_url);
}

TEST_F(ExtensionUpdateManifestTest, XmlWithHash) {
  TestParseUpdateManifest(CreateUpdateManifest(
      {UpdateManifestItem("12345")
           .version("1.2.3.4")
           .prodversionmin("2.0.143.0")
           .codebase("http://example.com/extension_1.2.3.4.crx")
           .hash_sha256("1234")}));
  ExpectNoError();
  ASSERT_TRUE(results());
  EXPECT_EQ(1U, results()->update_list.size());
  const UpdateManifestResult& first_result = results()->update_list.at(0);
  EXPECT_FALSE(first_result.parse_error);
  EXPECT_EQ("1234", first_result.package_hash);
}

TEST_F(ExtensionUpdateManifestTest, XmlWithDaystart) {
  TestParseUpdateManifest(
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
      " <daystart elapsed_seconds='456' />"
      " <app appid='12345'>"
      "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.2.3.4' prodversionmin='2.0.143.0' />"
      " </app>"
      "</gupdate>");
  ExpectNoError();
  ASSERT_TRUE(results());
  EXPECT_EQ(results()->daystart_elapsed_seconds, 456);
}

TEST_F(ExtensionUpdateManifestTest, NoUpdateResponse) {
  TestParseUpdateManifest(
      CreateUpdateManifest({UpdateManifestItem("12345").status("noupdate")}));
  ExpectNoError();
  ASSERT_TRUE(results());
  ASSERT_FALSE(results()->update_list.empty());
  const UpdateManifestResult& first_result = results()->update_list.at(0);
  EXPECT_FALSE(first_result.parse_error);
  EXPECT_EQ(first_result.extension_id, "12345");
  EXPECT_TRUE(first_result.version.empty());
}

TEST_F(ExtensionUpdateManifestTest, TwoAppsOneError) {
  TestParseUpdateManifest(
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
      " <app appid='aaaaaaaa' status='error-unknownApplication'>"
      "  <updatecheck status='error-unknownapplication'/>"
      " </app>"
      " <app appid='bbbbbbbb'>"
      "  <updatecheck codebase='http://example.com/b_3.1.crx' version='3.1'/>"
      " </app>"
      "</gupdate>");
  EXPECT_FALSE(parse_error());
  ASSERT_TRUE(results());
  EXPECT_EQ(2U, results()->update_list.size());
  EXPECT_FALSE(results()->update_list.at(1).parse_error);
  const UpdateManifestResult& second_result = results()->update_list.at(1);
  EXPECT_EQ(second_result.extension_id, "bbbbbbbb");
  EXPECT_TRUE(results()->update_list.at(0).parse_error);
}

TEST_F(ExtensionUpdateManifestTest, Duplicates) {
  TestParseUpdateManifest(
      CreateUpdateManifest({UpdateManifestItem("aaaaaaaa").status("noupdate"),
                            UpdateManifestItem("bbbbbbbb")
                                .version("3.1")
                                .codebase("http://example.com/b_3.1.crx"),
                            UpdateManifestItem("aaaaaaaa").status("noupdate"),
                            UpdateManifestItem("aaaaaaaa")
                                .version("2.0")
                                .codebase("http://example.com/a_2.0.crx")}));

  ExpectNoError();
  ASSERT_TRUE(results());

  const auto& list = results()->update_list;
  ASSERT_EQ(4u, list.size());

  EXPECT_FALSE(list[0].parse_error);
  EXPECT_EQ("aaaaaaaa", list[0].extension_id);
  EXPECT_TRUE(list[0].version.empty());

  EXPECT_FALSE(list[1].parse_error);
  EXPECT_EQ("bbbbbbbb", list[1].extension_id);
  EXPECT_EQ("3.1", list[1].version);
  EXPECT_EQ(GURL("http://example.com/b_3.1.crx"), list[1].crx_url);

  EXPECT_FALSE(list[2].parse_error);
  EXPECT_EQ("aaaaaaaa", list[2].extension_id);
  EXPECT_TRUE(list[2].version.empty());

  EXPECT_FALSE(list[3].parse_error);
  EXPECT_EQ("aaaaaaaa", list[3].extension_id);
  EXPECT_EQ("2.0", list[3].version);
  EXPECT_EQ(GURL("http://example.com/a_2.0.crx"), list[3].crx_url);
}

TEST_F(ExtensionUpdateManifestTest, GroupByID) {
  TestParseUpdateManifest(CreateUpdateManifest({
      UpdateManifestItem("aaaaaaaa").status("noupdate"),
      UpdateManifestItem("bbbbbbbb").status("noupdate"),
      UpdateManifestItem("aaaaaaaa").status("noupdate"),
      UpdateManifestItem("bbbbbbbb").status("noupdate"),
      UpdateManifestItem("cccccccc").status("noupdate"),
      UpdateManifestItem("aaaaaaaa").status("noupdate"),
  }));

  ExpectNoError();
  ASSERT_TRUE(results());
  ASSERT_EQ(6u, results()->update_list.size());

  const auto groups = results()->GroupSuccessfulByID();

  ASSERT_EQ(3u, groups.size());
  EXPECT_EQ(3u, groups.at("aaaaaaaa").size());
  EXPECT_EQ(2u, groups.at("bbbbbbbb").size());
  EXPECT_EQ(1u, groups.at("cccccccc").size());
}

}  // namespace extensions
