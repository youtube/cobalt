// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#endif

using content::BrowserContext;
using content::WebContents;
using content::WebContentsTester;
using permissions::PermissionAction;
using PersistedGrantStatus =
    ChromeFileSystemAccessPermissionContext::PersistedGrantStatus;
using GrantType = ChromeFileSystemAccessPermissionContext::GrantType;
using HandleType = ChromeFileSystemAccessPermissionContext::HandleType;
using PathType = ChromeFileSystemAccessPermissionContext::PathType;
using PersistedGrantType =
    ChromeFileSystemAccessPermissionContext::PersistedGrantType;
using UserAction = ChromeFileSystemAccessPermissionContext::UserAction;
using PermissionRequestOutcome =
    content::FileSystemAccessPermissionGrant::PermissionRequestOutcome;
using PermissionStatus =
    content::FileSystemAccessPermissionGrant::PermissionStatus;
using SensitiveDirectoryResult =
    ChromeFileSystemAccessPermissionContext::SensitiveEntryResult;
using UserActivationState =
    content::FileSystemAccessPermissionGrant::UserActivationState;

class TestFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit TestFileSystemAccessPermissionContext(
      content::BrowserContext* context,
      const base::Clock* clock)
      : ChromeFileSystemAccessPermissionContext(context, clock) {}
  ~TestFileSystemAccessPermissionContext() override = default;

 private:
  base::WeakPtrFactory<TestFileSystemAccessPermissionContext> weak_factory_{
      this};
};

class ChromeFileSystemAccessPermissionContextTest : public testing::Test {
 public:
  ChromeFileSystemAccessPermissionContextTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFileSystemAccessPersistentPermissions,
         permissions::features::kOneTimePermission},
        {});
  }
  void SetUp() override {
    // Create a scoped directory under %TEMP% instead of using
    // `base::ScopedTempDir::CreateUniqueTempDir`.
    // `base::ScopedTempDir::CreateUniqueTempDir` creates a path under
    // %ProgramFiles% on Windows when running as Admin, which is a blocked path
    // (`kBlockedPaths`). This can fail some of the tests.
    ASSERT_TRUE(
        temp_dir_.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));

    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile()));

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    FileSystemAccessPermissionRequestManager::CreateForWebContents(
        web_contents());
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(kTestOrigin.GetURL());

    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
        ->set_auto_response_for_test(PermissionAction::DISMISSED);
    permission_context_ =
        std::make_unique<TestFileSystemAccessPermissionContext>(
            browser_context(), task_environment_.GetMockClock());
#if !BUILDFLAG(IS_ANDROID)
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
#endif
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(temp_dir_.Delete());
    web_contents_.reset();
  }

  SensitiveDirectoryResult ConfirmSensitiveEntryAccessSync(
      ChromeFileSystemAccessPermissionContext* context,
      PathType path_type,
      const base::FilePath& path,
      HandleType handle_type,
      UserAction user_action) {
    base::test::TestFuture<
        ChromeFileSystemAccessPermissionContext::SensitiveEntryResult>
        future;
    permission_context_->ConfirmSensitiveEntryAccess(
        kTestOrigin, path_type, path, handle_type, user_action,
        content::GlobalRenderFrameHostId(), future.GetCallback());
    return future.Get();
  }

  void SetDefaultContentSettingValue(ContentSettingsType type,
                                     ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
    content_settings->SetDefaultContentSetting(type, value);
  }

  void SetContentSettingValueForOrigin(url::Origin origin,
                                       ContentSettingsType type,
                                       ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
    content_settings->SetContentSettingDefaultScope(
        origin.GetURL(), origin.GetURL(), type, value);
  }

#if !BUILDFLAG(IS_ANDROID)
  // Triggers the Restore permission prompt from two dormant grants (`kTestPath`
  // and `kTestPath2`). Note that the scoped references to active grants are
  // gone after this call, and the active grants may not exist in permission
  // context.
  PermissionRequestOutcome TriggerRestorePermissionPromptAfterBeingBackgrounded(
      const url::Origin& origin) {
    base::HistogramTester histograms;
    auto grant1 = permission_context()->GetReadPermissionGrant(
        kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
    auto grant2 = permission_context()->GetReadPermissionGrant(
        kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kOpen);
    EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
    EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

    // Dormant grants exist after tabs are backgrounded for the amount of time
    // specified by the extended permissions policy, and the corresponding
    // histogram is recorded.
    permission_context()->OnAllTabsInBackgroundTimerExpired(
        kTestOrigin,
        OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
    histograms.ExpectBucketCount(
        permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
            ContentSettingsType::FILE_SYSTEM_WRITE_GUARD),
        static_cast<base::HistogramBase::Sample>(
            permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND),
        1);
    EXPECT_EQ(grant1->GetStatus(), PermissionStatus::ASK);
    EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);

    // The Restore Permission prompt is triggered by calling
    // `requestPermission()` on the handle of an existing dormant grant.
    base::test::TestFuture<PermissionRequestOutcome> future;
    grant1->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                              future.GetCallback());
    auto result = future.Get();
    if (result == PermissionRequestOutcome::kGrantedByRestorePrompt) {
      EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
      EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);
    } else {
      EXPECT_EQ(grant1->GetStatus(), PermissionStatus::ASK);
      EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);
    }
    return result;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  ChromeFileSystemAccessPermissionContext* permission_context() {
    return permission_context_.get();
  }

  BrowserContext* browser_context() { return &profile_; }
  TestingProfile* profile() { return &profile_; }
  WebContents* web_contents() { return web_contents_.get(); }

  int process_id() {
    return web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  }

  content::GlobalRenderFrameHostId frame_id() {
    return content::GlobalRenderFrameHostId(
        process_id(), web_contents()->GetPrimaryMainFrame()->GetRoutingID());
  }

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }
  void Advance(base::TimeDelta delta) { task_environment_.AdvanceClock(delta); }

 protected:
  static constexpr char kPermissionIsDirectoryKey[] = "is-directory";
  static constexpr char kPermissionWritableKey[] = "writable";
  static constexpr char kPermissionReadableKey[] = "readable";
  static constexpr char kDeprecatedPermissionLastUsedTimeKey[] = "time";
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://test.com"));
  const url::Origin kPdfOrigin = url::Origin::Create(
      GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html"));
  const std::string kTestStartingDirectoryId = "test_id";
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
  const base::FilePath kTestPath2 = base::FilePath(FILE_PATH_LITERAL("/baz/"));
  const url::Origin kChromeOrigin = url::Origin::Create(GURL("chrome://test"));

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check_;
#endif
};

class ChromeFileSystemAccessPermissionContextNoPersistenceTest
    : public ChromeFileSystemAccessPermissionContextTest {
 public:
  ChromeFileSystemAccessPermissionContextNoPersistenceTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kFileSystemAccessPersistentPermissions);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_NoSpecialPath) {
  const base::FilePath kTestPath =
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\bar"));
#else
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
#endif

  // Paths outside of any special directories should be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, kTestPath,
                HandleType::kFile, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, kTestPath,
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);

  // External (relative) paths should also be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kExternal,
                base::FilePath(FILE_PATH_LITERAL("foo/bar")), HandleType::kFile,
                UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);

  // Paths outside of any special directories with no user action should be
  // allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, kTestPath,
                HandleType::kDirectory, UserAction::kNone),
            SensitiveDirectoryResult::kAllowed);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DontBlockAllChildren) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // The Home directory itself should not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, home_dir,
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // The parent of the Home directory should also not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // Paths inside of the Home directory should be allowed.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      home_dir.AppendASCII("foo"),
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal, home_dir.AppendASCII("foo"),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_BlockAllChildren) {
  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  base::ScopedPathOverride app_override(base::DIR_EXE, app_dir, true, true);

  // The App directory itself should not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, app_dir,
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // The parent of App directory should also not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // Paths inside of the App directory should also not be allowed.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      app_dir.AppendASCII("foo"),
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal, app_dir.AppendASCII("foo"),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_BlockChildrenNested) {
  base::FilePath user_data_dir = temp_dir_.GetPath().AppendASCII("user");
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir, true, true);
  base::FilePath download_dir = user_data_dir.AppendASCII("downloads");
  base::ScopedPathOverride download_override(chrome::DIR_DEFAULT_DOWNLOADS,
                                             download_dir, true, true);

  // The User Data directory itself should not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, user_data_dir,
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // The parent of the User Data directory should also not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, temp_dir_.GetPath(),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // The nested Download directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, download_dir,
                HandleType::kDirectory, UserAction::kOpen));
  // The paths inside of the nested Download directory should be allowed.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      download_dir.AppendASCII("foo"),
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                download_dir.AppendASCII("foo"), HandleType::kDirectory,
                UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);

#if BUILDFLAG(IS_WIN)
  // `DIR_IE_INTERNET_CACHE` is an example of a directory where nested
  // directories are blocked, but nested files should be allowed.
  base::FilePath internet_cache = user_data_dir.AppendASCII("INetCache");
  base::ScopedPathOverride internet_cache_override(base::DIR_IE_INTERNET_CACHE,
                                                   internet_cache, true, true);

  // The nested INetCache directory itself should not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, internet_cache,
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // Files inside of the nested INetCache directory should be allowed.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      internet_cache.AppendASCII("foo"),
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);
  // The directories should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                internet_cache.AppendASCII("foo"), HandleType::kDirectory,
                UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
#endif
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_RelativePathBlock) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // ~/.ssh should be blocked.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal, home_dir.AppendASCII(".ssh"),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
  // And anything inside ~/.ssh should also be blocked.
  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(permission_context(), PathType::kLocal,
                                      home_dir.AppendASCII(".ssh/id_rsa"),
                                      HandleType::kFile, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_ExplicitPathBlock) {
// Linux is the only OS where we have some blocked directories with explicit
// paths (as opposed to PathService provided paths).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // /dev should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // As well as children of /dev.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev/foo")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev/foo")),
                HandleType::kFile, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // Even if the user action is none, a blocklisted path should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("/dev")),
                HandleType::kDirectory, UserAction::kNone),
            SensitiveDirectoryResult::kAbort);
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("c:\\Program Files")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
#endif
}

#if BUILDFLAG(IS_MAC)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DontBlockAllChildren_Overlapping) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // The Home directory itself should not be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal, home_dir,
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // $HOME/Library should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII("Library"), HandleType::kDirectory,
                UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // $HOME/Library/Mobile Documents should be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII("Library/Mobile Documents"),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);
  // Paths within $HOME/Library/Mobile Documents should not be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII("Library/Mobile Documents/foo"),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);
  // Except for $HOME/Library/Mobile Documents/com~apple~CloudDocs, which should
  // be blocked.
  EXPECT_EQ(

      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          home_dir.AppendASCII("Library/Mobile Documents/com~apple~CloudDocs"),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
  // Paths within $HOME/Library/Mobile Documents/com~apple~CloudDocs should not
  // be blocked.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                home_dir.AppendASCII(
                    "Library/Mobile Documents/com~apple~CloudDocs/foo"),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_UNCPath) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessLocalUNCPathBlock)) {
    return;
  }

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\server\\share\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAllowed);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("c:\\\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\localhost\\c$\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\LOCALHOST\\c$\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\127.0.0.1\\c$\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("\\\\.\\c:\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL("\\\\?\\c:\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(FILE_PATH_LITERAL(
                    "\\\\;LanmanRedirector\\localhost\\c$\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                base::FilePath(
                    FILE_PATH_LITERAL("\\\\.\\UNC\\LOCALHOST\\c:\\foo\\bar")),
                HandleType::kDirectory, UserAction::kOpen),
            SensitiveDirectoryResult::kAbort);

  EXPECT_EQ(
      ConfirmSensitiveEntryAccessSync(
          permission_context(), PathType::kLocal,
          base::FilePath(FILE_PATH_LITERAL("\\\\myhostname\\c$\\foo\\bar")),
          HandleType::kDirectory, UserAction::kOpen),
      SensitiveDirectoryResult::kAbort);
}
#endif

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       ConfirmSensitiveEntryAccess_DangerousFile) {
  // Saving files with a harmless extension should be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.txt"), HandleType::kFile,
                UserAction::kSave),
            SensitiveDirectoryResult::kAllowed);
  // Saving files with a dangerous extension should show a prompt.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.swf"), HandleType::kFile,
                UserAction::kSave),
            SensitiveDirectoryResult::kAbort);
  // Files with a dangerous extension from no user action should be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.swf"), HandleType::kFile,
                UserAction::kNone),
            SensitiveDirectoryResult::kAllowed);
  // Opening files with a dangerous extension should be allowed.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.swf"), HandleType::kFile,
                UserAction::kOpen),
            SensitiveDirectoryResult::kAllowed);
  // Opening files with a dangerous compound extension should show a prompt.
  EXPECT_EQ(ConfirmSensitiveEntryAccessSync(
                permission_context(), PathType::kLocal,
                temp_dir_.GetPath().AppendASCII("test.txt.swf"),
                HandleType::kFile, UserAction::kSave),
            SensitiveDirectoryResult::kAbort);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CanObtainWritePermission_ContentSettingAsk) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_ASK);
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CanObtainWritePermission_ContentSettingsBlock) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       CanObtainWritePermission_ContentSettingAllow) {
  // chrome:// scheme is whitelisted, but we can't set the default content
  // setting here because `ALLOW` is not an acceptable option.
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kChromeOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyReadGuardPermission) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemReadGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PolicyWriteGuardPermission) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemWriteGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyReadAskForUrls) {
  // Set the default to `BLOCK` so that the policy being tested overrides it.
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemReadGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(
      prefs::kManagedFileSystemReadAskForUrls,
      base::test::ParseJsonList("[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_TRUE(permission_context()->CanObtainReadPermission(kTestOrigin));
  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyReadBlockedForUrls) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(
      prefs::kManagedFileSystemReadBlockedForUrls,
      base::test::ParseJsonList("[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_FALSE(permission_context()->CanObtainReadPermission(kTestOrigin));
  EXPECT_TRUE(permission_context()->CanObtainReadPermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyWriteAskForUrls) {
  // Set the default to `BLOCK` so that the policy being tested overrides it.
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultFileSystemWriteGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(
      prefs::kManagedFileSystemWriteAskForUrls,
      base::test::ParseJsonList("[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin));
  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, PolicyWriteBlockedForUrls) {
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(
      prefs::kManagedFileSystemWriteBlockedForUrls,
      base::test::ParseJsonList("[\"" + kTestOrigin.Serialize() + "\"]"));

  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, GetLastPickedDirectory) {
  auto file_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(file_info.path, base::FilePath());
  EXPECT_EQ(file_info.type, PathType::kLocal);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, SetLastPickedDirectory) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  auto type = PathType::kLocal;
  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, kTestPath, type);
  auto path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(path_info.path, kTestPath);
  EXPECT_EQ(path_info.type, type);

  auto new_path = path_info.path.AppendASCII("baz");
  auto new_type = PathType::kExternal;
  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, new_path, new_type);
  auto new_path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(new_path_info.path, new_path);
  EXPECT_EQ(new_path_info.type, new_type);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       SetLastPickedDirectory_DefaultId) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  // Call `SetLastPickedDirectory` with `kTestStartingDirectoryId`.
  auto type = PathType::kLocal;
  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, kTestPath, type);
  auto path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(path_info.path, kTestPath);
  EXPECT_EQ(path_info.type, type);

  // Call `SetLastPickedDirectory` with an empty (default) ID.
  auto new_id = std::string();
  auto new_path = path_info.path.AppendASCII("baz");
  auto new_type = PathType::kExternal;
  permission_context()->SetLastPickedDirectory(kTestOrigin, new_id, new_path,
                                               new_type);
  auto new_path_info =
      permission_context()->GetLastPickedDirectory(kTestOrigin, new_id);
  EXPECT_EQ(new_path_info.path, new_path);
  EXPECT_EQ(new_path_info.type, new_type);

  // Confirm that the original ID can still be retrieved as it was previously.
  auto old_path_info = permission_context()->GetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId);
  EXPECT_EQ(old_path_info.path, kTestPath);
  EXPECT_EQ(old_path_info.type, type);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, LimitNumberOfIds) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  permission_context()->SetMaxIdsPerOriginForTesting(3);

  // The default path should NOT be evicted.
  auto default_id = std::string();
  auto default_path = base::FilePath::FromUTF8Unsafe("default");

  std::string id1("1");
  auto path1 = base::FilePath::FromUTF8Unsafe("path1");
  std::string id2("2");
  auto path2 = base::FilePath::FromUTF8Unsafe("path2");
  std::string id3("3");
  auto path3 = base::FilePath::FromUTF8Unsafe("path3");
  std::string id4("4");
  auto path4 = base::FilePath::FromUTF8Unsafe("path4");

  // Set the path using the default ID. This should NOT be evicted.
  permission_context()->SetLastPickedDirectory(kTestOrigin, default_id,
                                               default_path, PathType::kLocal);
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, default_id)
                .path,
            default_path);

  // Set the maximum number of IDs. Only set IDs should return non-empty paths.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id1, path1,
                                               PathType::kLocal);
  Advance(base::Minutes(1));
  permission_context()->SetLastPickedDirectory(kTestOrigin, id2, path2,
                                               PathType::kLocal);
  Advance(base::Minutes(1));
  permission_context()->SetLastPickedDirectory(kTestOrigin, id3, path3,
                                               PathType::kLocal);
  Advance(base::Minutes(1));
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            path1);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            path2);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            base::FilePath());  // Unset.

  // Once the fourth id has been set, only `id1` should be evicted.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id4, path4,
                                               PathType::kLocal);
  Advance(base::Minutes(1));
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            base::FilePath());  // Unset.
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            path2);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            path4);

  // Reset `id1`, evicting `id2`.
  permission_context()->SetLastPickedDirectory(kTestOrigin, id1, path1,
                                               PathType::kLocal);
  Advance(base::Minutes(1));
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id1).path,
            path1);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id2).path,
            base::FilePath());  // Unset.
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id3).path,
            path3);
  EXPECT_EQ(permission_context()->GetLastPickedDirectory(kTestOrigin, id4).path,
            path4);

  // Ensure that the default path was not evicted.
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, default_id)
                .path,
            default_path);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       SetLastPickedDirectory_NewPermissionContext) {
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            base::FilePath());

  const base::FilePath path = base::FilePath(FILE_PATH_LITERAL("/baz/bar"));

  permission_context()->SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, path, PathType::kLocal);
  ASSERT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            path);

  TestFileSystemAccessPermissionContext new_permission_context(
      browser_context(), task_environment_.GetMockClock());
  EXPECT_EQ(new_permission_context
                .GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            path);

  auto new_path = path.AppendASCII("foo");
  new_permission_context.SetLastPickedDirectory(
      kTestOrigin, kTestStartingDirectoryId, new_path, PathType::kLocal);
  EXPECT_EQ(permission_context()
                ->GetLastPickedDirectory(kTestOrigin, kTestStartingDirectoryId)
                .path,
            new_path);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Base_OK) {
  base::ScopedPathOverride user_desktop_override(
      base::DIR_USER_DESKTOP, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context()->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDesktop, kTestOrigin),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Chrome_OK) {
  base::ScopedPathOverride user_documents_override(
      chrome::DIR_USER_DOCUMENTS, temp_dir_.GetPath(), true, true);
  EXPECT_EQ(permission_context()->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDocuments, kTestOrigin),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWellKnownDirectoryPath_Pdf_Downloads) {
  DownloadPrefs::FromBrowserContext(browser_context())
      ->SkipSanitizeDownloadTargetPathForTesting();
  DownloadPrefs::FromBrowserContext(browser_context())
      ->SetDownloadPath(temp_dir_.GetPath());
  EXPECT_EQ(permission_context()->GetWellKnownDirectoryPath(
                blink::mojom::WellKnownDirectory::kDirDownloads, kPdfOrigin),
            temp_dir_.GetPath());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_File) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_LoadFromStorage) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_File) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_Open_Directory) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_WritableImplicitState) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // The existing grant should not change if the permission is blocked globally.
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // Getting a grant for the same file again should also not change the grant,
  // even though asking for more permissions is blocked globally.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_WriteGrantedChangesExistingGrant) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  // All grants should be equivalent, and should be granted and persisted.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextNoPersistenceTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_NoPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  grant.reset();
  // After reset, the grant should go be cleared, and the new grant request is
  // in the `ASK` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_GrantIsAutoGrantedViaPersistentPermissions) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  // A valid persisted permission should be created.
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant.reset();
  // Permission should not be granted for `kOpen`.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  // Permission should be auto-granted here via the persisted permission.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(),
            PermissionRequestOutcome::kGrantedByPersistentPermission);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       IsValidObject_GrantsWithDeprecatedTimestampKeyAreNotValidObjects) {
  // Create a placeholder grant for testing, containing a
  // 'kDeprecatedPermissionLastUsedTimeKey' key, which should render the
  // permission object invalid.
  base::Value::Dict grant;
  grant.Set(ChromeFileSystemAccessPermissionContext::kPermissionPathKey,
            FilePathToValue(kTestPath));
  grant.Set(kPermissionIsDirectoryKey, true);
  grant.Set(kPermissionReadableKey, true);
  grant.Set(kDeprecatedPermissionLastUsedTimeKey,
            base::TimeToValue(base::Time::Min()));
  EXPECT_FALSE(permission_context()->IsValidObject(grant));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetGrantedObjectsAndConvertObjectsToGrants_GrantsAreRetainedViaPersistedPermissions) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto kTestPath2 = kTestPath.AppendASCII("baz");
  auto file_write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto file_read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto file_read_only_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kSave);
  auto grants = permission_context()->ConvertObjectsToGrants(
      permission_context()->GetGrantedObjects(kTestOrigin));
  std::vector<base::FilePath> expected_file_write_grants = {kTestPath};
  std::vector<base::FilePath> expected_file_read_grants = {kTestPath,
                                                           kTestPath2};
  EXPECT_EQ(grants.file_write_grants, expected_file_write_grants);
  EXPECT_EQ(grants.file_read_grants, expected_file_read_grants);

  // Persisted permissions are retained after resetting the active grants.
  file_write_grant.reset();
  file_read_grant.reset();
  file_read_only_grant.reset();
  grants = permission_context()->ConvertObjectsToGrants(
      permission_context()->GetGrantedObjects(kTestOrigin));
  EXPECT_EQ(grants.file_write_grants, expected_file_write_grants);
  EXPECT_EQ(grants.file_read_grants, expected_file_read_grants);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetExtendedPersistedObjects) {
  auto kTestPath2 = kTestPath.AppendASCII("foo");
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://www.c.com"));
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin2);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath2, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  ASSERT_THAT(
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin),
      testing::SizeIs(1));
  ASSERT_THAT(
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin2),
      testing::SizeIs(1));

  // Revoke the active grant, but not the persisted permission. The granted
  // object for the given origin is not revoked.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  ASSERT_THAT(
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin),
      testing::SizeIs(1));

  // The granted objects are updated when all of its permissions are revoked.
  permission_context()->RevokeGrants(kTestOrigin);
  ASSERT_THAT(
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin),
      testing::IsEmpty());

  // An empty vector is returned when an origin does not have extended
  // permissions.
  SetContentSettingValueForOrigin(kTestOrigin2,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));
  auto granted_objects_no_persistent_permissions =
      permission_context()->GetExtendedPersistedObjectsForTesting(kTestOrigin2);
  EXPECT_TRUE(granted_objects_no_persistent_permissions.empty());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InitialState_OpenAction_GlobalGuardBlocked) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  grant.reset();

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_InitialState_WritableImplicitState_GlobalGuardBlocked) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  grant.reset();

  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_WriteGrantedChangesExistingGrant_GlobalGuardBlocked) {
  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_BLOCK);

  auto grant1 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  // All grants should be equivalent, and have a `DENIED` state.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedBeforeNewGrant) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  grant.reset();

  // After reset, the grant should go away and the new grant request should
  // have a `DENIED` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextNoPersistenceTest,
       GetGrantedObjects_NoPersistentPermissions) {
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  // Only one permission grant object is recorded when a given origin has both
  // read and write access for a given resource.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_Triggered_AfterTabBackgrounded) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto result =
      TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kGrantedByRestorePrompt);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_Triggered_HandleLoadedFromStorage) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  // Create dormant grants that are eligible to be restored.
  // Use a directory handle, which is not auto-granted, so that later it can be
  // demonstrated that a directory handle is not able to trigger the restore
  // prompt, and will instead trigger the single-file permission prompt.
  auto grant1 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> grant1_future;
  grant1->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            grant1_future.GetCallback());
  EXPECT_EQ(grant1_future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);

  auto grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> grant2_future;
  grant2->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            grant2_future.GetCallback());
  EXPECT_EQ(grant2_future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  // TODO(crbug.com/1011533): Update this test to navigate away from the page,
  // instead of manually resetting the grant.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  grant1.reset();
  grant2.reset();

  // Get the handles from the storage (i.e. IndexedDB).
  auto grant1_from_storage = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory,
      UserAction::kLoadFromStorage);
  auto grant2_from_storage = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kDirectory,
      UserAction::kLoadFromStorage);
  EXPECT_EQ(grant1_from_storage->GetStatus(), PermissionStatus::ASK);
  EXPECT_EQ(grant2_from_storage->GetStatus(), PermissionStatus::ASK);

  // `requestPermission()` on a handle from IndxedDB triggers the Restore
  // Permission prompt.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant1_from_storage->RequestPermission(
      frame_id(), UserActivationState::kNotRequired, future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_EQ(grant1_from_storage->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(grant2_from_storage->GetStatus(), PermissionStatus::GRANTED);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_NotTriggered_HandleNotLoadedFromStorage) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  // Create dormant grants that are eligible to be restored.
  // Use a directory handle, which is not auto-granted, so that later it can be
  // demonstrated that a directory handle is not able to trigger the restore
  // prompt and can only trigger the single-file permission flow.
  auto grant1 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> grant1_future;
  grant1->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            grant1_future.GetCallback());
  EXPECT_EQ(grant1_future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);

  auto grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> grant2_future;
  grant2->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            grant2_future.GetCallback());
  EXPECT_EQ(grant2_future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  // TODO(crbug.com/1011533): Update this test to navigate away from the page,
  // instead of manually resetting the grant.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  grant1.reset();
  grant2.reset();

  // Get the handles from the directory picker.
  auto grant1_not_from_storage = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  auto grant2_not_from_storage = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant1_not_from_storage->GetStatus(), PermissionStatus::ASK);
  EXPECT_EQ(grant2_not_from_storage->GetStatus(), PermissionStatus::ASK);

  // Calling `requestPermission()` on a handle not loaded from IndexedDB will
  // not trigger the restore permission prompt. Only the requested handle is
  // granted permission.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant1_not_from_storage->RequestPermission(
      frame_id(), UserActivationState::kNotRequired, future.GetCallback());
  EXPECT_NE(future.Get(), PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_EQ(grant1_not_from_storage->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(grant2_not_from_storage->GetStatus(), PermissionStatus::ASK);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_NotTriggered_WhenNoDormatGrants) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  // The origin only has active grants, and no dormant grants. Therefore, the
  // origin should not grant permission via the Restore Permission prompt.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  auto permission_request_outcome = future.Get();
  EXPECT_NE(permission_request_outcome,
            PermissionRequestOutcome::kGrantedByRestorePrompt);
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    RestorePermissionPrompt_NotTriggered_WhenRequestingWriteAccessToReadGrant) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  // Dormant grants exist after tabs are backgrounded for the amount of time
  // specified by the extended permissions policy.
  permission_context()->OnAllTabsInBackgroundTimerExpired(
      kTestOrigin,
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  // The origin is not eligible to request permission via the restore prompt if
  // requesting write access to a file which previously had read access.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kLoadFromStorage);
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  auto permission_request_outcome = future.Get();
  EXPECT_NE(permission_request_outcome,
            PermissionRequestOutcome::kGrantedByRestorePrompt);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_NotTriggered_WhenRequestAccessToNewFile) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  // Dormant grants exist after tabs are backgrounded for the amount of time
  // specified by the extended permissions policy.
  permission_context()->OnAllTabsInBackgroundTimerExpired(
      kTestOrigin,
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  // The origin is not eligible to request permission via the restore prompt if
  // requesting access to a new file (`kTestPath2`).
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kLoadFromStorage);
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant2->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            future.GetCallback());
  auto permission_request_outcome = future.Get();
  EXPECT_NE(permission_request_outcome,
            PermissionRequestOutcome::kGrantedByRestorePrompt);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_AllowEveryTime) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  auto result =
      TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_EQ(permission_context()->content_settings()->GetContentSetting(
                kTestOrigin.GetURL(), kTestOrigin.GetURL(),
                ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION),
            ContentSetting::CONTENT_SETTING_ALLOW);

  // The dormant grants are now extended grants, which can be returned from
  // `GetGrantedObjects()`.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));

  // TODO(crbug.com/1011533): Update this test to navigate away from the page,
  // instead of manually resetting the grants.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);

  // TODO(crbug.com/1011533): Update this test to navigate away from the page,
  // instead of manually resetting the grants.
  // The granted permission objects remain, even after navigating away from the
  // page.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_AllowOnce) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED_ONCE);
  auto grant1 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  // Dormant grants exist after tabs are backgrounded for the amount of time
  // specified by the extended permissions policy.
  permission_context()->OnAllTabsInBackgroundTimerExpired(
      kTestOrigin,
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kLongTimeout);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::ASK);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);

  // Restore Permission prompt is triggered by calling
  // `requestPermission()` on the handle of an existing dormant grant.
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant1->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                            future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_NE(permission_context()->content_settings()->GetContentSetting(
                kTestOrigin.GetURL(), kTestOrigin.GetURL(),
                ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION),
            ContentSetting::CONTENT_SETTING_ALLOW);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));

  // TODO(crbug.com/1011533): Update this test to navigate away from the page,
  // instead of manually resetting the grants.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  // The granted permissions are cleared after navigating away from the page.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_Denied) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DENIED);
  EXPECT_EQ(TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin),
            PermissionRequestOutcome::kUserDenied);

  // Persisted grants are cleared as a result of restore prompt rejection, when
  // Extended Permissions is not enabled.
  // The origin is not embargoed when the restore prompt is rejected one time.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
  auto origin_is_embargoed =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(kTestOrigin.GetURL(),
                        ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_FALSE(origin_is_embargoed);

  //  Check that the origin is placed under embargo after being ignored
  // `kDefaultDismissalsBeforeBlock` times.
  EXPECT_EQ(TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin),
            PermissionRequestOutcome::kUserDenied);
  // The origin is not embargoed after being ignored two times, when the limit
  // set by `kDefaultDismissalsBeforeBlock` is three times.
  auto origin_is_embargoed_updated =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(kTestOrigin.GetURL(),
                        ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_FALSE(origin_is_embargoed_updated);
  // The origin is embargoed, after reaching the ignore limit set by
  // `kDefaultDismissalsBeforeBlock`.
  EXPECT_EQ(TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin),
            PermissionRequestOutcome::kUserDenied);
  auto origin_is_embargoed_after_rejection_limit =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(kTestOrigin.GetURL(),
                        ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_TRUE(origin_is_embargoed_after_rejection_limit);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, OnWebAppInstalled) {
  // Create a persisted grant for `kTestOrigin`.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));

  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // When extended permissions is not enabled, the persistent grants are
  // revoked and the grant status is set to current.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kCurrent);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnWebAppInstalled_WithCurrentGrants) {
  // Create current, persisted grants by triggering the restore prompt and
  // accepting it.
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));

  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // When extended permissions is not enabled and a web app is installed when
  // the grant status is current, the persisted grants are not revoked.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnWebAppInstalled_ExtendedPermissionsEnabled) {
  // Enabled extended permissions.
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);

  // Create a persisted grant for `kTestOrigin`.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));

  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // When extended permissions is enabled, the persisted grants are not
  // revoked and the persisted grant type remains 'extended'.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
  EXPECT_EQ(permission_context()->GetPersistedGrantTypeForTesting(kTestOrigin),
            PersistedGrantType::kExtended);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnWebAppUninstalled_WithPeristentAndActiveGrants) {
  const base::FilePath kTestPath2 = base::FilePath(FILE_PATH_LITERAL("/a/b"));
  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  const webapps::AppId app_id =
      web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // Create a grant, then revoke its active permissions.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);

  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);

  // Create another grant, with granted active permissions.
  auto read_grant2 = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kSave);
  auto write_grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath2, HandleType::kFile, UserAction::kSave);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(2));

  // Uninstall the web app for `kTestOrigin`.
  web_app::test::UninstallWebApp(profile(), app_id);

  // After the web app is uninstalled, persistent grants are cleared and
  // re-created off of the granted active grants set.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnWebAppUninstalled_NoGrantedActiveGrants) {
  // Install a web app for `kTestOrigin`.
  const GURL kTestOriginUrl = GURL("https://example.com");
  const webapps::AppId app_id =
      web_app::test::InstallDummyWebApp(profile(), "Test App", kTestOriginUrl);

  // Create a persistent grant, and revoke its active permissions.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kLoaded);

  // Uninstall the web app for `kTestOrigin`, while there are persistent grants
  // and no granted active grants.
  web_app::test::UninstallWebApp(profile(), app_id);

  // The grant status is set to current, and the persistent grants are revoked.
  // The persisted grants are not re-created because there were no granted
  // active grants at the time the web app was uninstalled.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kCurrent);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RestorePermissionPrompt_Ignored) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::IGNORED);
  auto result =
      TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kRequestAborted);

  // Persisted grants are cleared by ignoring the restore prompt.
  // The origin is not embargoed on first ignore.
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
  auto origin_is_embargoed =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(kTestOrigin.GetURL(),
                        ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_FALSE(origin_is_embargoed);

  // The origin is placed under embargo after being ignored
  // `kDefaultIgnoresBeforeBlock` times.
  result = TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kRequestAborted);
  result = TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kRequestAborted);
  // The origin is not embargoed after being ignored three times, when the
  // limit set by `kDefaultIgnoresBeforeBlock` is four times.
  auto origin_is_embargoed_updated =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(kTestOrigin.GetURL(),
                        ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_FALSE(origin_is_embargoed_updated);

  // The origin is embargoed, after reaching the ignore limit set by
  // `kDefaultIgnoresBeforeBlock`.
  result = TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(result, PermissionRequestOutcome::kRequestAborted);
  auto origin_is_embargoed_after_ignore_limit =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(kTestOrigin.GetURL(),
                        ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_TRUE(origin_is_embargoed_after_ignore_limit);
}

// TODO(crbug.com/1011533): Expand upon this test case to cover checking that
// dormant grants are not revoked, when backgrounded dormant grants exist.
// Currently, there is no method to retrieve dormant grants, for testing
// purposes.
TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnLastPageFromOriginClosed) {
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(write_grant->GetStatus(), PermissionStatus::GRANTED);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
  // When the last tab for an origin is closed or navigated away from,
  // while there are loaded grants, both the active and persistent grants
  // are revoked.
  permission_context()->OnLastPageFromOriginClosed(kTestOrigin);
  EXPECT_EQ(write_grant->GetStatus(), PermissionStatus::ASK);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnLastPageFromOriginClosed_PersistedGrantStatusUpdated) {
  // Create a current grant by triggering the restore prompt, and accepting it.
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  TriggerRestorePermissionPromptAfterBeingBackgrounded(kTestOrigin);
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kCurrent);
  // The grant status is updated to loaded, as a result of the last tab being
  // navigated away from.
  permission_context()->OnLastPageFromOriginClosed(kTestOrigin);
  EXPECT_EQ(
      permission_context()->GetPersistedGrantStatusForTesting(kTestOrigin),
      PersistedGrantStatus::kLoaded);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       OnLastPageFromOriginClosed_HasExtendedPermissions) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  base::test::TestFuture<PermissionRequestOutcome> future;
  write_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                 future.GetCallback());
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
  // When the last tab for an origin is closed or navigated away from, while
  // extended permissions is enabled, the grants are not cleared.
  permission_context()->OnLastPageFromOriginClosed(kTestOrigin);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::SizeIs(1));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextNoPersistenceTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedAfterNewGrant_NoPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  // Revoke the active and persisted permissions.
  permission_context()->RevokeGrants(kTestOrigin);
  grant.reset();
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // After resetting the grant, the granted object should not exist, and the
  // new grant request should be in the `ASK` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  ASSERT_THAT(permission_context()->GetGrantedObjects(kTestOrigin),
              testing::IsEmpty());

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for the grant is
  // unchanged.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(
    ChromeFileSystemAccessPermissionContextTest,
    GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed_GlobalGuardBlockedAfterNewGrant_HasPersistentPermissions) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  // Revoke active and persisted permissions.
  permission_context()->RevokeGrants(kTestOrigin);
  grant.reset();
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // After reset, the grant should be cleared and the new grant request should
  // be in the `ASK` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for the `grant` should
  // remain unchanged.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InheritFromAncestor) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // A file in `dir_path`'s directory should have auto-granted permissions.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InheritFromAncestor) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // A file in `dir_path`'s directory should be auto-granted permissions.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       DoNotInheritFromAncestorOfOppositeType) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // `dir_path` already has read permission while we're asking for write
  // permission, so the permission is not auto-granted.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_InheritFromPersistedAncestor) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // A file in `dir_path`'s directory should not be granted permission until
  // permission is explicitly requested.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(future2.Get(),
            PermissionRequestOutcome::kGrantedByAncestorPersistentPermission);
  // Age should not be recorded if granted via an ancestor's permission.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_InheritFromPersistedAncestor) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // A file in `dir_path`'s directory should not be granted permission until
  // permission is explicitly requested.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(future2.Get(),
            PermissionRequestOutcome::kGrantedByAncestorPersistentPermission);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       DoNotInheritFromPersistedAncestorOfOppositeType) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto dir_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future;
  dir_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                               future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(dir_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // Remove the active grant, but not the persisted permission.
  dir_grant.reset();

  // `dir_path` has read permission while we're asking for write permission,
  // so do not auto-grant the permission and do not grant via persisted
  // permission.
  auto file_path = kTestPath.AppendASCII("baz");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, file_path, HandleType::kFile, UserAction::kLoadFromStorage);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::ASK);
  base::test::TestFuture<PermissionRequestOutcome> future2;
  file_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future2.GetCallback());
  EXPECT_EQ(future2.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, file_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_RevokeOnlyActiveGrants) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  // Revoke active grant, but not the persisted permission.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  ChromeFileSystemAccessPermissionContext::Grants grants =
      permission_context()->ConvertObjectsToGrants(
          permission_context()->GetGrantedObjects(kTestOrigin));
  std::vector<base::FilePath> expected_res = {kTestPath};
  EXPECT_EQ(grants.file_write_grants, expected_res);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_RevokeGrantByFilePath) {
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  permission_context()->RevokeGrant(kTestOrigin, kTestPath);
  auto updated_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kNone);
  EXPECT_EQ(updated_grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_NotAccessibleIfContentSettingBlock) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  grant.reset();
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // After reset, the grant should be cleared, and the new grant request should
  // be in the `ASK` state.
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  // After the guard is blocked, the permission status for the `grant` should
  // remain unchanged and the persisted permission should not be accessible.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       PersistedPermission_ReadWriteGrants) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);

  // Grant the write access.
  auto write_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kSave);
  EXPECT_EQ(write_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Deny the read access.
  auto read_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DENIED);
  base::test::TestFuture<PermissionRequestOutcome> future;
  read_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserDenied);
  EXPECT_EQ(read_grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // Denying read access should not remove write access.
  EXPECT_EQ(write_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_ClearOutdatedDormantGrants) {
  // If the restore permission prompt is not triggered when requesting
  // permission, dormant grants should be cleared such that old dormant grants
  // are not carried over to the next session.
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  // Create a persisted grant by granting a directory handle.
  auto grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);

  // Navigate away so that the persisted grant becomes dormant.
  // TODO(crbug.com/1011533): Update this test to navigate away from the page,
  // instead of manually resetting the grant.
  permission_context()->RevokeActiveGrantsForTesting(kTestOrigin);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);

  // On a revisit, requesting permission does not trigger the restore permission
  // prompt, as there is no dormant grant.
  base::test::TestFuture<PermissionRequestOutcome> future2;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future2.GetCallback());
  EXPECT_NE(future2.Get(), PermissionRequestOutcome::kGrantedByRestorePrompt);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_Dismissed) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DISMISSED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserDismissed);
  // The grant status should change as a result of dismissal.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, RequestPermission_Granted) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, RequestPermission_Denied) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::DENIED);
  content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserDenied);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_NoUserActivation) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kNoUserActivation);
  // The status should not change when there is no user activation.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_NoUserActivation_UserActivationNotRequired) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  // The status should not change when there is no user activation.
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_AlreadyGranted) {
  // If the permission has already been granted, a call to `RequestPermission`
  // should call the passed-in callback and return immediately without showing
  // a prompt.
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);

  base::test::TestFuture<PermissionRequestOutcome> future;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_GlobalGuardBlockedBeforeOpenGrant) {
  // If the guard content setting is blocked, a call to `RequestPermission`
  // should update the `PermissionStatus` to `DENIED`, call the passed-in
  // callback, and return immediately without showing a prompt.
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future1;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future1.GetCallback());
  EXPECT_EQ(future1.Get(), PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future2;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future2.GetCallback());
  EXPECT_EQ(future2.Get(), PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant2.reset();
  SetContentSettingValueForOrigin(kTestOrigin2,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);

  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future3;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future3.GetCallback());
  EXPECT_EQ(future3.Get(), PermissionRequestOutcome::kNoUserActivation);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       RequestPermission_GlobalGuardBlockedAfterOpenGrant) {
  // If the guard content setting is blocked, a call to `RequestPermission`
  // should update the `PermissionStatus` to `DENIED`, call the passed-in
  // callback, and return immediately without showing a prompt.
  auto grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  base::test::TestFuture<PermissionRequestOutcome> future1;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future1.GetCallback());
  EXPECT_EQ(future1.Get(), PermissionRequestOutcome::kBlockedByContentSetting);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  base::test::TestFuture<PermissionRequestOutcome> future2;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future2.GetCallback());
  EXPECT_EQ(future2.Get(), PermissionRequestOutcome::kBlockedByContentSetting);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));

  grant.reset();
  grant2.reset();

  SetContentSettingValueForOrigin(kTestOrigin,
                                  ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                  CONTENT_SETTING_ASK);
  grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  grant2 = permission_context()->GetWritePermissionGrant(
      kTestOrigin2, kTestPath, HandleType::kFile, UserAction::kOpen);

  base::test::TestFuture<PermissionRequestOutcome> future3;
  grant->RequestPermission(frame_id(), UserActivationState::kRequired,
                           future3.GetCallback());
  EXPECT_EQ(future3.Get(), PermissionRequestOutcome::kNoUserActivation);
  EXPECT_EQ(grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  base::test::TestFuture<PermissionRequestOutcome> future4;
  grant2->RequestPermission(frame_id(), UserActivationState::kRequired,
                            future4.GetCallback());
  EXPECT_EQ(future4.Get(), PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin2, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_AllowlistedOrigin_InitialState) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context());
  allowlist->RegisterAutoGrantedPermission(
      kChromeOrigin, ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  allowlist->RegisterAutoGrantedPermission(
      kChromeOrigin, ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);

  // An allowlisted origin automatically gets write permission.
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kChromeOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto grant2 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kChromeOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // The other origin should be blocked.
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant3->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto grant4 = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant4->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_AllowlistedOrigin_ExistingGrant) {
  SetDefaultContentSettingValue(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                CONTENT_SETTING_BLOCK);

  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context());
  allowlist->RegisterAutoGrantedPermission(
      kChromeOrigin, ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  allowlist->RegisterAutoGrantedPermission(
      kChromeOrigin, ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);

  // Initial grant (file).
  auto grant1 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant1->GetStatus(), PermissionStatus::GRANTED);
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kChromeOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // Existing grant (file).
  auto grant2 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(grant2->GetStatus(), PermissionStatus::GRANTED);

  // Initial grant (directory).
  auto grant3 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant3->GetStatus(), PermissionStatus::GRANTED);
  // Permissions are not persisted for allowlisted origins.
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kChromeOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Existing grant (directory).
  auto grant4 = permission_context()->GetWritePermissionGrant(
      kChromeOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(grant4->GetStatus(), PermissionStatus::GRANTED);
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetReadPermissionGrant_FileBecomesDirectory) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto file_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));

  auto directory_grant = permission_context()->GetReadPermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(directory_grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kRead));

  // Requesting a permission grant for a directory which was previously a file
  // revokes the original file permission.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kRead));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       GetWritePermissionGrant_FileBecomesDirectory) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  auto directory_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(directory_grant->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Requesting a permission grant for a directory which was previously a file
  // revokes the original file permission.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::DENIED);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest, NotifyEntryMoved_File) {
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  const auto new_path = kTestPath.DirName().AppendASCII("new_name.txt");
  permission_context()->NotifyEntryMoved(kTestOrigin, kTestPath, new_path);

  // Permissions to the old path are revoked.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant_at_old_path->GetStatus(), PermissionStatus::ASK);
  EXPECT_FALSE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kFile, GrantType::kWrite));

  // Permissions to the new path are updated.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(file_grant->GetPath(), new_path);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       NotifyEntryMoved_ChildFileObtainedLater) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto parent_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> future;
  parent_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                  future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(parent_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // The child file should inherit write permission from its parent.
  const auto old_file_path = kTestPath.AppendASCII("old_name.txt");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, old_file_path, HandleType::kFile, GrantType::kWrite));

  const auto new_path = old_file_path.DirName().AppendASCII("new_name.txt");
  permission_context()->NotifyEntryMoved(kTestOrigin, old_file_path, new_path);

  // Permissions to the parent are not affected.
  auto parent_grant_copy = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(parent_grant_copy->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Permissions to the old file path aer not affected.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant_at_old_path->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(file_grant_at_old_path->GetPath(), old_file_path);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, old_file_path, HandleType::kFile, GrantType::kWrite));

  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(file_grant->GetPath(), new_path);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}

TEST_F(ChromeFileSystemAccessPermissionContextTest,
       NotifyEntryMoved_ChildFileObtainedFirst) {
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(PermissionAction::GRANTED);
  permission_context()->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  // Acquire permission to the child file's path.
  const auto old_file_path = kTestPath.AppendASCII("old_name.txt");
  auto file_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path, HandleType::kFile, UserAction::kSave);
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, old_file_path, HandleType::kFile, GrantType::kWrite));

  // Later, acquire permission to the child parent.
  auto parent_grant = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  base::test::TestFuture<PermissionRequestOutcome> future;
  parent_grant->RequestPermission(frame_id(), UserActivationState::kNotRequired,
                                  future.GetCallback());
  EXPECT_EQ(future.Get(), PermissionRequestOutcome::kUserGranted);
  EXPECT_EQ(parent_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  const auto new_path = old_file_path.DirName().AppendASCII("new_name.txt");
  permission_context()->NotifyEntryMoved(kTestOrigin, old_file_path, new_path);

  // Permissions to the parent are not affected.
  auto parent_grant_copy = permission_context()->GetWritePermissionGrant(
      kTestOrigin, kTestPath, HandleType::kDirectory, UserAction::kOpen);
  EXPECT_EQ(parent_grant_copy->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, kTestPath, HandleType::kDirectory, GrantType::kWrite));

  // Permissions to the old file path are not affected.
  auto file_grant_at_old_path = permission_context()->GetWritePermissionGrant(
      kTestOrigin, old_file_path, HandleType::kFile, UserAction::kOpen);
  EXPECT_EQ(file_grant_at_old_path->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, old_file_path, HandleType::kFile, GrantType::kWrite));

  // Permission should still be granted at the new path.
  EXPECT_EQ(file_grant->GetStatus(), PermissionStatus::GRANTED);
  EXPECT_EQ(file_grant->GetPath(), new_path);
  EXPECT_TRUE(permission_context()->HasExtendedPermissionForTesting(
      kTestOrigin, new_path, HandleType::kFile, GrantType::kWrite));
}

#endif  // !BUILDFLAG(IS_ANDROID)
