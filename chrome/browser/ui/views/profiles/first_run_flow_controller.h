// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_toolbar.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"

struct CoreAccountInfo;
enum class IntroChoice;
class Profile;

// Creates a step to represent the intro. Exposed for testing.
std::unique_ptr<ProfileManagementStepController> CreateIntroStep(
    ProfilePickerWebContentsHost* host,
    base::RepeatingCallback<void(IntroChoice)> choice_callback,
    bool enable_animations,
    base::RepeatingCallback<bool()> query_effects_callback);

std::unique_ptr<ProfileManagementStepController> CreateDefaultBrowserStep(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    base::OnceClosure step_completed_callback);

std::unique_ptr<ProfileManagementStepController> CreateFeatureShowcaseStep(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    base::OnceClosure step_completed_callback,
    base::OnceCallback<void(bool)> eligibility_callback);

std::unique_ptr<ProfileManagementStepController> CreateFinishOrContinueStep(
    ProfilePickerWebContentsHost* host,
    base::OnceCallback<bool()> eligibility_callback,
    base::RepeatingCallback<bool()> query_effects_callback,
    base::OnceClosure step_completed_callback);

class FirstRunFlowController : public ProfileManagementFlowControllerImpl {
 public:
  static constexpr audio::SoundsManager::SoundKey kAmbientSoundKey = 0;
  static constexpr audio::SoundsManager::SoundKey kLogoSoundKey = 1;
  static constexpr audio::SoundsManager::SoundKey kWelcomeBackSoundKey = 2;

  // Profile management flow controller that will run the FRE for `profile` in
  // `host`.
  FirstRunFlowController(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback,
      Profile* profile,
      ProfilePicker::FirstRunExitedCallback first_run_exited_callback);
  ~FirstRunFlowController() override;

  // ProfileManagementFlowControllerImpl:
  void Init() override;
  void CancelSigninFlow() override;
  void PickProfile(
      const base::FilePath& profile_path,
      ProfilePicker::ProfilePickingArgs args,
      base::OnceCallback<void(bool)> pick_profile_complete_callback) override;
  void ShowSigninError(Profile* profile, const SigninUIError& error) override;
  ProfilePickerToolbar::Builder CreateToolbarBuilder() override;

  using SoundsManagerFactory =
      base::RepeatingCallback<std::unique_ptr<audio::SoundsManager>(
          audio::SoundsManager::StreamFactoryBinder)>;

  // Sets the factory used to create the `SoundsManager` in tests.
  // The returned `base::AutoReset` automatically restores the previous factory
  // when it goes out of scope.
  [[nodiscard]] static base::AutoReset<SoundsManagerFactory>
  SetSoundsManagerFactoryForTesting(SoundsManagerFactory factory);

 protected:
  // ProfileManagementFlowControllerImpl
  bool PreFinishWithBrowser() override;
  // `account_info` may not be set as the primary account yet.
  std::unique_ptr<ProfilePickerPostSignInAdapter> CreatePostSignInAdapter(
      Profile* signed_in_profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents) override;
  base::queue<ProfileManagementFlowController::Step> RegisterPostIdentitySteps(
      PostHostClearedCallback post_host_cleared_callback) override;

 private:
  void HandleIntroSigninChoice(IntroChoice choice);

  void PlaySignInCelebrationSound();

  void StartBrowsing();

  // Run the `finish_flow_callback_` if it's not empty.
  void RunFinishFlowCallback();

  bool is_feature_showcase_eligible() const {
    return is_feature_showcase_eligible_;
  }

  void SetFeatureShowcaseEligibility(bool is_eligible) {
    is_feature_showcase_eligible_ = is_eligible;
  }

  std::string GetHatsSurveyTrigger() const;

  void ToggleMediaEffects(bool active);

  bool AreEffectsEnabled() const;

  void MaybeTriggerHatsSurvey();

  const raw_ptr<Profile> profile_;
  ProfilePicker::FirstRunExitedCallback first_run_exited_callback_;

  // The callback that will finish the flow and open the browser.
  base::OnceClosure finish_flow_callback_;

  std::unique_ptr<audio::SoundsManager> sounds_manager_;

  bool is_feature_showcase_eligible_ = false;

  base::WeakPtrFactory<FirstRunFlowController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_H_
