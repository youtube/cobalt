// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browsing_data_remover.h"

class Profile;

namespace base {
class AtomicFlag;
}

namespace {
FORWARD_DECLARE_TEST(ProfileResetterTest, ResetNTPCustomizationsTest);
}

// This class allows resetting certain aspects of a profile to default values.
// It is used in case the profile has been damaged due to malware or bad user
// settings.
class ProfileResetter : public content::BrowsingDataRemover::Observer {
 public:
  // Flags indicating what aspects of a profile shall be reset.
  enum Resettable {
    DEFAULT_SEARCH_ENGINE = 1 << 0,
    HOMEPAGE = 1 << 1,
    CONTENT_SETTINGS = 1 << 2,
    COOKIES_AND_SITE_DATA = 1 << 3,
    EXTENSIONS = 1 << 4,
    STARTUP_PAGES = 1 << 5,
    PINNED_TABS = 1 << 6,
    SHORTCUTS = 1 << 7,
    NTP_CUSTOMIZATIONS = 1 << 8,
    LANGUAGES = 1 << 9,
    // Update ALL if you add new values and check whether the type of
    // ResettableFlags needs to be enlarged.
    ALL = DEFAULT_SEARCH_ENGINE | HOMEPAGE | CONTENT_SETTINGS |
          COOKIES_AND_SITE_DATA | EXTENSIONS | STARTUP_PAGES | PINNED_TABS |
          SHORTCUTS | NTP_CUSTOMIZATIONS | LANGUAGES
  };

  // Bit vector for Resettable enum.
  typedef uint32_t ResettableFlags;

  static_assert(sizeof(ResettableFlags) == sizeof(Resettable),
                "ResettableFlags should be the same size as Resettable");

  explicit ProfileResetter(Profile* profile);

  ProfileResetter(const ProfileResetter&) = delete;
  ProfileResetter& operator=(const ProfileResetter&) = delete;

  ~ProfileResetter() override;

  // Resets |resettable_flags| and calls |callback| on the UI thread on
  // completion. |default_settings| allows the caller to specify some default
  // settings. |default_settings| shouldn't be NULL.
  virtual void Reset(ResettableFlags resettable_flags,
                     std::unique_ptr<BrandcodedDefaultSettings> master_settings,
                     base::OnceClosure callback);

  virtual bool IsActive() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(::ProfileResetterTest, ResetNTPCustomizationsTest);

  // Marks |resettable| as done and triggers |callback_| if all pending jobs
  // have completed.
  void MarkAsDone(Resettable resettable);

  void ResetDefaultSearchEngine();
  void ResetHomepage();
  void ResetContentSettings();
  void ResetCookiesAndSiteData();
  void ResetExtensions();
  void ResetStartupPages();
  void ResetPinnedTabs();
  void ResetShortcuts();
  void ResetNtpCustomizations();
  void ResetLanguages();

  // BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

  // Callback for when TemplateURLService has loaded.
  void OnTemplateURLServiceLoaded();

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<BrandcodedDefaultSettings> master_settings_;
  raw_ptr<TemplateURLService, DanglingUntriaged> template_url_service_;

  // Flags of a Resetable indicating which reset operations we are still waiting
  // for.
  ResettableFlags pending_reset_flags_;

  // Called on UI thread when reset has been completed.
  base::OnceClosure callback_;

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  raw_ptr<content::BrowsingDataRemover> cookies_remover_;

  base::CallbackListSubscription template_url_service_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ProfileResetter> weak_ptr_factory_{this};
};

// Path to shortcut and command line arguments.
typedef std::pair<base::FilePath, std::wstring> ShortcutCommand;

typedef base::RefCountedData<base::AtomicFlag> SharedCancellationFlag;

#if BUILDFLAG(IS_WIN)
// On Windows returns all the shortcuts which launch Chrome and corresponding
// arguments. |cancel| can be passed to abort the operation earlier.
// Call on COM task runner that may block.
std::vector<ShortcutCommand> GetChromeLaunchShortcuts(
    const scoped_refptr<SharedCancellationFlag>& cancel);
#endif

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_
