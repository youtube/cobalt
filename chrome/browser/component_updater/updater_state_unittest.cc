// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/updater_state.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace component_updater {

class UpdaterStateTest : public testing::Test {};

TEST_F(UpdaterStateTest, SerializeChromium) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_STREQ("0", UpdaterState::GetState(false).at("ismachine").c_str());
  EXPECT_STREQ("1", UpdaterState::GetState(true).at("ismachine").c_str());
#else
  EXPECT_TRUE(UpdaterState::GetState(false).empty());
  EXPECT_TRUE(UpdaterState::GetState(true).empty());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

// Returns the path to the global prefs.json file of the Chromium updater.
absl::optional<base::FilePath> GetUpdaterGlobalPrefsPath(bool is_machine) {
  const absl::optional<base::FilePath> global_prefs_dir =
      updater::GetInstallDirectory(is_machine ? updater::UpdaterScope::kSystem
                                              : updater::UpdaterScope::kUser);
  return global_prefs_dir
             ? absl::make_optional(global_prefs_dir->AppendASCII("prefs.json"))
             : absl::nullopt;
}

}  // namespace

TEST_F(UpdaterStateTest, SerializeChromePerUser) {
  {
    UpdaterState updater_state(false);

    updater_state.state_->updater_name = "the updater";
    updater_state.state_->updater_version = base::Version("1.0");
    updater_state.state_->last_autoupdate_started =
        base::Time::NowFromSystemTime();
    updater_state.state_->last_checked = base::Time::NowFromSystemTime();
    updater_state.state_->is_autoupdate_check_enabled = true;
    updater_state.state_->update_policy = 1;

    UpdaterState::Attributes attributes = updater_state.Serialize();

    EXPECT_STREQ("the updater", attributes.at("name").c_str());
    EXPECT_STREQ("1.0", attributes.at("version").c_str());
    EXPECT_STREQ("0", attributes.at("laststarted").c_str());
    EXPECT_STREQ("0", attributes.at("lastchecked").c_str());
    EXPECT_STREQ("1", attributes.at("autoupdatecheckenabled").c_str());
    EXPECT_STREQ("1", attributes.at("updatepolicy").c_str());
  }

  {
    UpdaterState updater_state = UpdaterState(false);

    // Don't serialize an invalid version if it could not be read.
    updater_state.state_->updater_version = base::Version();
    UpdaterState::Attributes attributes = updater_state.Serialize();
    EXPECT_EQ(0u, attributes.count("version"));

    updater_state.state_->updater_version = base::Version("0.0.0.0");
    attributes = updater_state.Serialize();
    EXPECT_STREQ("0.0.0.0", attributes.at("version").c_str());

    updater_state.state_->last_autoupdate_started =
        base::Time::NowFromSystemTime() - base::Days(15);
    attributes = updater_state.Serialize();
    EXPECT_STREQ("336", attributes.at("laststarted").c_str());

    updater_state.state_->last_autoupdate_started =
        base::Time::NowFromSystemTime() - base::Days(58);
    attributes = updater_state.Serialize();
    EXPECT_STREQ("1344", attributes.at("laststarted").c_str());

    updater_state.state_->last_autoupdate_started =
        base::Time::NowFromSystemTime() - base::Days(90);
    attributes = updater_state.Serialize();
    EXPECT_STREQ("1344", attributes.at("laststarted").c_str());

    // Don't serialize the time if it could not be read.
    updater_state.state_->last_autoupdate_started = base::Time();
    attributes = updater_state.Serialize();
    EXPECT_EQ(0u, attributes.count("laststarted"));

    updater_state.state_->last_checked =
        base::Time::NowFromSystemTime() - base::Days(15);
    attributes = updater_state.Serialize();
    EXPECT_STREQ("336", attributes.at("lastchecked").c_str());

    updater_state.state_->last_checked =
        base::Time::NowFromSystemTime() - base::Days(90);
    attributes = updater_state.Serialize();
    EXPECT_STREQ("1344", attributes.at("lastchecked").c_str());

    // Don't serialize the time if it could not be read (the value is invalid).
    updater_state.state_->last_checked = base::Time();
    attributes = updater_state.Serialize();
    EXPECT_EQ(0u, attributes.count("lastchecked"));

    updater_state.state_->is_autoupdate_check_enabled = false;
    attributes = updater_state.Serialize();
    EXPECT_STREQ("0", attributes.at("autoupdatecheckenabled").c_str());

    updater_state.state_->update_policy = 0;
    attributes = updater_state.Serialize();
    EXPECT_STREQ("0", attributes.at("updatepolicy").c_str());

    updater_state.state_->update_policy = -1;
    attributes = updater_state.Serialize();
    EXPECT_STREQ("-1", attributes.at("updatepolicy").c_str());
  }
}

TEST_F(UpdaterStateTest, UpdaterNamePerUser) {
  absl::optional<base::FilePath> prefs_path = GetUpdaterGlobalPrefsPath(false);
  base::DeleteFile(*prefs_path);

#if BUILDFLAG(IS_WIN)
  EXPECT_STREQ("Omaha", UpdaterState::GetState(false).at("name").c_str());
#elif BUILDFLAG(IS_MAC)
  EXPECT_STREQ("Keystone", UpdaterState::GetState(false).at("name").c_str());
#endif  // BUILDFLAG(IS_WIN)

  // Create an empty updater prefs file to mock a detection of the updater.
  EXPECT_TRUE(base::CreateDirectory(prefs_path->DirName()));
  EXPECT_TRUE(base::WriteFile(*prefs_path, "{}"));
  EXPECT_STREQ("ChromiumUpdater",
               UpdaterState::GetState(false).at("name").c_str());

  base::DeleteFile(*prefs_path);
}

#endif  // (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)) &&
        // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace component_updater
