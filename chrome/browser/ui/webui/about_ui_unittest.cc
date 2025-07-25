// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/about_ui.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/ui/fake_login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "third_party/zlib/google/compression_utils.h"
#endif

namespace {

class TestDataReceiver {
 public:
  TestDataReceiver() = default;

  TestDataReceiver(const TestDataReceiver&) = delete;
  TestDataReceiver& operator=(const TestDataReceiver&) = delete;

  virtual ~TestDataReceiver() = default;

  bool data_received() const { return data_received_; }

  std::string data() const { return data_; }

  std::string Base64DecodedData() const {
    std::string decoded;
    base::Base64Decode(data_, &decoded);
    return decoded;
  }

  void OnDataReceived(scoped_refptr<base::RefCountedMemory> bytes) {
    data_received_ = true;
    data_ = std::string(base::StringPiece(
        reinterpret_cast<const char*>(bytes->front()), bytes->size()));
  }

 private:
  bool data_received_ = false;
  std::string data_;
};

}  // namespace

// Base class for ChromeOS offline terms tests.
class ChromeOSTermsTest : public testing::Test {
 public:
  ChromeOSTermsTest(const ChromeOSTermsTest&) = delete;
  ChromeOSTermsTest& operator=(const ChromeOSTermsTest&) = delete;

 protected:
  ChromeOSTermsTest() {}
  ~ChromeOSTermsTest() override = default;

  void SetUp() override {
    // Create root tmp directory for fake ARC ToS data.
    base::FilePath root_path;
    base::CreateNewTempDirectory(FILE_PATH_LITERAL(""), &root_path);
    ASSERT_TRUE(preinstalled_offline_resources_dir_.Set(root_path));
    arc_tos_dir_ =
        preinstalled_offline_resources_dir_.GetPath().Append("arc_tos");
    ASSERT_TRUE(base::CreateDirectory(arc_tos_dir_));

    tested_html_source_ = std::make_unique<AboutUIHTMLSource>(
        chrome::kChromeUITermsHost, nullptr);
  }

  // Creates directory for the given |locale| that contains terms.html. Writes
  // the |locale| string to the created file.
  bool CreateTermsForLocale(const std::string& locale) {
    base::FilePath dir = arc_tos_dir_.Append(base::ToLowerASCII(locale));
    if (!base::CreateDirectory(dir))
      return false;

    return base::WriteFile(dir.AppendASCII("terms.html"), locale);
  }

  // Creates directory for the given |locale| that contains privacy_policy.pdf.
  // Writes the |locale| string to the created file.
  bool CreatePrivacyPolicyForLocale(const std::string& locale) {
    base::FilePath dir = arc_tos_dir_.Append(base::ToLowerASCII(locale));
    if (!base::CreateDirectory(dir))
      return false;

    return base::WriteFile(dir.AppendASCII("privacy_policy.pdf"), locale);
  }

  // Sets device region in VPD.
  void SetRegion(const std::string& region) {
    statistics_provider_.SetMachineStatistic(ash::system::kRegionKey, region);
  }

  // Starts data request with the |request_url|.
  void StartRequest(const std::string& request_url,
                    TestDataReceiver* data_receiver) {
    content::WebContents::Getter wc_getter;
    tested_html_source_->StartDataRequest(
        GURL(base::StrCat(
            {"chrome://", chrome::kChromeUITermsHost, "/", request_url})),
        std::move(wc_getter),
        base::BindOnce(&TestDataReceiver::OnDataReceived,
                       base::Unretained(data_receiver)));
    task_environment_.RunUntilIdle();
  }

  const base::FilePath& PreinstalledOfflineResourcesPath() {
    return preinstalled_offline_resources_dir_.GetPath();
  }

 private:
  base::ScopedTempDir preinstalled_offline_resources_dir_;
  base::FilePath arc_tos_dir_;

  content::BrowserTaskEnvironment task_environment_;

  ash::system::ScopedFakeStatisticsProvider statistics_provider_;

  std::unique_ptr<AboutUIHTMLSource> tested_html_source_;
};

TEST_F(ChromeOSTermsTest, NoData) {
  SetRegion("ca");
  ScopedBrowserLocale browser_locale("en-CA");

  TestDataReceiver terms_data_receiver;
  StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

  EXPECT_FALSE(terms_data_receiver.data_received());
  EXPECT_EQ("", terms_data_receiver.data());

  TestDataReceiver privacy_policy_data_receiver;
  StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_policy_data_receiver);

  EXPECT_FALSE(privacy_policy_data_receiver.data_received());
  EXPECT_EQ("", privacy_policy_data_receiver.data());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Base class for ChromeOS offline terms tests.
class ChromeOSCreditsTest : public testing::Test {
 public:
  ChromeOSCreditsTest(const ChromeOSCreditsTest&) = delete;
  ChromeOSCreditsTest& operator=(const ChromeOSCreditsTest&) = delete;

 protected:
  ChromeOSCreditsTest() {}
  ~ChromeOSCreditsTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(resources_dir_.CreateUniqueTempDir());

    tested_html_source_ = std::make_unique<AboutUIHTMLSource>(
        chrome::kChromeUIOSCreditsHost, nullptr);
    tested_html_source_->SetOSCreditsPrefixForTesting(resources_dir_.GetPath());
  }

  bool CreateHtmlCredits() {
    return base::WriteFile(
        resources_dir_.GetPath().Append(
            base::FilePath(chrome::kChromeOSCreditsPath).BaseName()),
        kTestHtml);
  }

  bool CreateCompressedHtmlCredits() {
    std::string compressed;
    if (!compression::GzipCompress(std::string(kTestHtml), &compressed)) {
      return false;
    }
    return base::WriteFile(
        resources_dir_.GetPath().Append(
            base::FilePath(chrome::kChromeOSCreditsCompressedPath).BaseName()),
        compressed);
  }

  // Starts data request with the |request_url|.
  void StartRequest(TestDataReceiver* data_receiver) {
    content::WebContents::Getter wc_getter;
    tested_html_source_->StartDataRequest(
        GURL(base::StrCat({"chrome://", chrome::kChromeUIOSCreditsHost, "/"})),
        std::move(wc_getter),
        base::BindOnce(&TestDataReceiver::OnDataReceived,
                       base::Unretained(data_receiver)));
    task_environment_.RunUntilIdle();
  }

 protected:
  static constexpr char kTestHtml[] = "<html><body>test</body></html>";

 private:
  base::ScopedTempDir resources_dir_;

  content::BrowserTaskEnvironment task_environment_;

  ash::system::ScopedFakeStatisticsProvider statistics_provider_;

  std::unique_ptr<AboutUIHTMLSource> tested_html_source_;
};

// Verify that it reads decompressed html file
TEST_F(ChromeOSCreditsTest, Decompressed) {
  ASSERT_TRUE(CreateHtmlCredits());
  TestDataReceiver data_receiver;
  StartRequest(&data_receiver);

  EXPECT_TRUE(data_receiver.data_received());
  EXPECT_EQ(data_receiver.data(), kTestHtml);
}

// Verify that it reads compressed html file
TEST_F(ChromeOSCreditsTest, Compressed) {
  ASSERT_TRUE(CreateCompressedHtmlCredits());
  TestDataReceiver data_receiver;
  StartRequest(&data_receiver);

  EXPECT_TRUE(data_receiver.data_received());
  EXPECT_EQ(data_receiver.data(), kTestHtml);
}

// Verify that it falls back to a default
TEST_F(ChromeOSCreditsTest, Neither) {
  TestDataReceiver data_receiver;
  StartRequest(&data_receiver);

  EXPECT_TRUE(data_receiver.data_received());
  EXPECT_NE(data_receiver.data(), kTestHtml);
  EXPECT_FALSE(data_receiver.data().empty());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
