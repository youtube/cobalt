// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test_scope.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/registry.h"
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {

TEST(PersistedDataTest, Simple) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());

  EXPECT_FALSE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_TRUE(metadata->GetFingerprint("someappid").empty());
  EXPECT_TRUE(metadata->GetAppIds().empty());

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());

  metadata->SetFingerprint("someappid", "fp1");
  EXPECT_STREQ("fp1", metadata->GetFingerprint("someappid").c_str());

  // Store some more apps in prefs, in addition to "someappid". Expect only
  // the app ids for apps with valid versions to be returned.
  metadata->SetProductVersion("appid1", base::Version("2.0"));
  metadata->SetFingerprint("appid2-nopv", "somefp");
  EXPECT_FALSE(metadata->GetProductVersion("appid2-nopv").IsValid());
  const auto app_ids = metadata->GetAppIds();
  EXPECT_EQ(2u, app_ids.size());
  EXPECT_TRUE(base::Contains(app_ids, "someappid"));
  EXPECT_TRUE(base::Contains(app_ids, "appid1"));
  EXPECT_FALSE(base::Contains(app_ids, "appid2-nopv"));  // No valid pv.

  const base::Time time1 = base::Time::FromJsTime(10000);
  metadata->SetLastChecked(time1);
  EXPECT_EQ(metadata->GetLastChecked(), time1);
  const base::Time time2 = base::Time::FromJsTime(20000);
  metadata->SetLastStarted(time2);
  EXPECT_EQ(metadata->GetLastStarted(), time2);
}

TEST(PersistedDataTest, MixedCase) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  metadata->SetProductVersion("SOMEAPPID2", base::Version("2.0"));
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someAPPID").GetString().c_str());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
  EXPECT_STREQ("2.0",
               metadata->GetProductVersion("someAPPID2").GetString().c_str());
  EXPECT_STREQ("2.0",
               metadata->GetProductVersion("someappid2").GetString().c_str());
}

TEST(PersistedDataTest, RegistrationRequest) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);
  EXPECT_TRUE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath("someappid").value());
  EXPECT_STREQ("arandom-ap=likethis", metadata->GetAP("someappid").c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode("someappid").c_str());

#if BUILDFLAG(IS_WIN)
  base::win::RegKey key;
  EXPECT_EQ(key.Open(UpdaterScopeToHKeyRoot(GetTestScope()),
                     GetAppClientStateKey(L"someappid").c_str(),
                     Wow6432(KEY_QUERY_VALUE)),
            ERROR_SUCCESS);
  std::wstring ap;
  EXPECT_EQ(key.ReadValue(L"ap", &ap), ERROR_SUCCESS);
  EXPECT_EQ(ap, L"arandom-ap=likethis");
#endif
}

TEST(PersistedDataTest, RegistrationRequestPartial) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));
  metadata->RegisterApp(data);
  EXPECT_TRUE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath("someappid").value());
  EXPECT_STREQ("arandom-ap=likethis", metadata->GetAP("someappid").c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode("someappid").c_str());

  RegistrationRequest data2;
  data2.app_id = data.app_id;
  data2.ap = "different_ap";
  metadata->RegisterApp(data2);
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion(data.app_id).GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath(data.app_id).value());
  EXPECT_STREQ("different_ap", metadata->GetAP(data.app_id).c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode(data.app_id).c_str());

  RegistrationRequest data3;
  data3.app_id = "someappid3";
  data3.brand_code = "somebrand";
  data3.version = base::Version("1.0");
  metadata->RegisterApp(data3);
  EXPECT_TRUE(metadata->GetProductVersion("someappid3").IsValid());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid3").GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL(""),
            metadata->GetExistenceCheckerPath("someappid3").value());
  EXPECT_STREQ("", metadata->GetAP("someappid3").c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode("someappid3").c_str());
}

TEST(PersistedDataTest, SharedPref) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());

  // Now, create a new PersistedData reading from the same path, verify
  // that it loads the value.
  metadata = base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
}

TEST(PersistedDataTest, RemoveAppId) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);

  data.app_id = "someappid2";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("2.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);
  EXPECT_EQ(size_t{2}, metadata->GetAppIds().size());

  metadata->RemoveApp("someappid");
  EXPECT_EQ(size_t{1}, metadata->GetAppIds().size());

  metadata->RemoveApp("someappid2");
  EXPECT_TRUE(metadata->GetAppIds().empty());
}

#if BUILDFLAG(IS_WIN)
TEST(PersistedDataTest, LastOSVersion) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());

  EXPECT_EQ(metadata->GetLastOSVersion(), absl::nullopt);

  // This will persist the current OS version into the persisted data.
  metadata->SetLastOSVersion();
  EXPECT_NE(metadata->GetLastOSVersion(), absl::nullopt);

  // Compare the persisted data OS version to the version from `::GetVersionEx`.
  const OSVERSIONINFOEX metadata_os = metadata->GetLastOSVersion().value();

  OSVERSIONINFOEX os = {};
  os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  EXPECT_TRUE(::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&os)));
#pragma clang diagnostic pop

  EXPECT_EQ(metadata_os.dwOSVersionInfoSize, os.dwOSVersionInfoSize);
  EXPECT_EQ(metadata_os.dwMajorVersion, os.dwMajorVersion);
  EXPECT_EQ(metadata_os.dwMinorVersion, os.dwMinorVersion);
  EXPECT_EQ(metadata_os.dwBuildNumber, os.dwBuildNumber);
  EXPECT_EQ(metadata_os.dwPlatformId, os.dwPlatformId);
  EXPECT_STREQ(metadata_os.szCSDVersion, os.szCSDVersion);
  EXPECT_EQ(metadata_os.wServicePackMajor, os.wServicePackMajor);
  EXPECT_EQ(metadata_os.wServicePackMinor, os.wServicePackMinor);
  EXPECT_EQ(metadata_os.wSuiteMask, os.wSuiteMask);
  EXPECT_EQ(metadata_os.wProductType, os.wProductType);
}
#endif

}  // namespace updater
