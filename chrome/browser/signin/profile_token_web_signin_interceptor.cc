// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_token_web_signin_interceptor.h"

#include <string>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_token_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/token_managed_profile_creator.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"

namespace {
class FakeDelegate : public ProfileTokenWebSigninInterceptor::Delegate {
 public:
  FakeDelegate() = default;

  ~FakeDelegate() override = default;

  void ShowCreateNewProfileBubble(
      const ProfileAttributesEntry* switch_to_profile,
      base::OnceCallback<void(bool)> callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }
};

}  // namespace

ProfileTokenWebSigninInterceptor::ProfileTokenWebSigninInterceptor(
    Profile* profile)
    : ProfileTokenWebSigninInterceptor(profile,
                                       std::make_unique<FakeDelegate>()) {}

ProfileTokenWebSigninInterceptor::ProfileTokenWebSigninInterceptor(
    Profile* profile,
    std::unique_ptr<Delegate> delegate)
    : profile_(profile), delegate_(std::move(delegate)) {
  DCHECK(profile_);
  DCHECK(delegate_);
}

ProfileTokenWebSigninInterceptor::~ProfileTokenWebSigninInterceptor() = default;

void ProfileTokenWebSigninInterceptor::MaybeInterceptSigninProfile(
    content::WebContents* intercepted_contents,
    const std::string& id,
    const std::string& enrollment_token) {
  if (!IsValidEnrollmentToken(enrollment_token)) {
    DVLOG(1) << "Invalid enrollment token";
    return;
  }

  if (!intercepted_contents) {
    DVLOG(1) << "Web contents no longer available, aborting interception";
    return;
  }

  web_contents_ = intercepted_contents->GetWeakPtr();
  enrollment_token_ = enrollment_token;
  intercepted_id_ = id;

  VLOG(1) << "Starting interception for id: " + intercepted_id_ +
                 " with enrollment token : " + enrollment_token_;

  DCHECK(!switch_to_entry_);
  base::FilePath profile_path = profile_->GetPath();
  for (const auto* entry : g_browser_process->profile_manager()
                               ->GetProfileAttributesStorage()
                               .GetAllProfilesAttributes()) {
    if (entry->GetProfileManagementEnrollmentToken() == enrollment_token_ &&
        entry->GetProfileManagementId() == intercepted_id_) {
      switch_to_entry_ = entry;
      break;
    }
  }

  // Same profile
  if (switch_to_entry_ && switch_to_entry_->GetPath() == profile_path) {
    DVLOG(1) << "Intercepted info is already in the right profile";
    Reset();
    return;
  }

  delegate_->ShowCreateNewProfileBubble(
      switch_to_entry_,
      base::BindOnce(&ProfileTokenWebSigninInterceptor::OnProfileCreationChoice,
                     base::Unretained(this)));
}

void ProfileTokenWebSigninInterceptor::Shutdown() {
  Reset();
}

void ProfileTokenWebSigninInterceptor::Reset() {
  web_contents_ = nullptr;
  switch_to_entry_ = nullptr;
  intercepted_id_.clear();
  enrollment_token_.clear();
  profile_creator_.reset();
}

bool ProfileTokenWebSigninInterceptor::IsValidEnrollmentToken(
    const std::string& enrollment_token) const {
  return !enrollment_token.empty();
}

void ProfileTokenWebSigninInterceptor::OnProfileCreationChoice(bool accepted) {
  if (!accepted) {
    if (switch_to_entry_) {
      DVLOG(1) << "Profile switch refused by the user";
    } else {
      DVLOG(1) << "Profile creation refused by the user";
    }
    Reset();
    return;
  }

  DCHECK(!profile_creator_);
  if (switch_to_entry_) {
    // Unretained is fine because the profile creator is owned by this.
    profile_creator_ = std::make_unique<TokenManagedProfileCreator>(
        switch_to_entry_->GetPath(),
        base::BindOnce(
            &ProfileTokenWebSigninInterceptor::OnNewSignedInProfileCreated,
            base::Unretained(this)));
  } else {
    // Unretained is fine because the profile creator is owned by this.
    profile_creator_ = std::make_unique<TokenManagedProfileCreator>(
        intercepted_id_, enrollment_token_,
        profiles::GetDefaultNameForNewEnterpriseProfile(),
        base::BindOnce(
            &ProfileTokenWebSigninInterceptor::OnNewSignedInProfileCreated,
            base::Unretained(this)));
  }
}

void ProfileTokenWebSigninInterceptor::OnNewSignedInProfileCreated(
    Profile* new_profile) {
  DCHECK(profile_creator_);

  if (!new_profile) {
    DVLOG(1) << "Failed to create new profile";
    Reset();
    return;
  }

  // Generate a color theme for new profiles
  if (!switch_to_entry_) {
    DVLOG(1) << "New profile created";
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(new_profile->GetPath());
    SkColor profile_color = GenerateNewProfileColor(entry).color;
    ThemeServiceFactory::GetForProfile(new_profile)
        ->BuildAutogeneratedThemeFromColor(profile_color);
  } else {
    DVLOG(1) << "Profile switched sucessfully";
  }

  if (!disable_browser_creation_after_interception_for_testing_) {
    // Work is done in this profile, the flow continues in the
    // ProfileTokenWebSigninInterceptor that is attached to the new profile.
    // We pass relevant parameters from this instance to the new one.
    ProfileTokenWebSigninInterceptorFactory::GetForProfile(new_profile)
        ->CreateBrowserAfterSigninInterception(web_contents_.get());
  }

  Reset();
}

void ProfileTokenWebSigninInterceptor::CreateBrowserAfterSigninInterception(
    content::WebContents* intercepted_contents) {
  DCHECK(intercepted_contents);

  GURL url_to_open = GURL(chrome::kChromeUINewTabURL);
  if (intercepted_contents) {
    url_to_open = intercepted_contents->GetLastCommittedURL();
    intercepted_contents->Close();
  }

  // Open a new browser.
  NavigateParams params(profile_, url_to_open,
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  Navigate(&params);
  DVLOG(1) << "New browser created";
}
