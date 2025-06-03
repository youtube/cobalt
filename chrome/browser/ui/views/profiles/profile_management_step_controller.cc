// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "google_apis/gaia/core_account_id.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_provider.h"
#endif

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"
#include "components/search_engines/search_engine_choice_utils.h"
#endif

namespace {
class ProfilePickerAppStepController : public ProfileManagementStepController {
 public:
  explicit ProfilePickerAppStepController(ProfilePickerWebContentsHost* host,
                                          const GURL& initial_url)
      : ProfileManagementStepController(host), initial_url_(initial_url) {}

  ~ProfilePickerAppStepController() override = default;

  void Show(base::OnceCallback<void(bool)> step_shown_callback,
            bool reset_state) override {
    if (!loaded_ui_in_picker_contents_) {
      loaded_ui_in_picker_contents_ = true;
      host()->ShowScreenInPickerContents(
          initial_url_,
          step_shown_callback
              ? base::BindOnce(std::move(step_shown_callback), true)
              : base::OnceClosure());
      return;
    }

    if (reset_state) {
      // Don't do a full reset, just go back to the beginning of the history:
      host()->GetPickerContents()->GetController().GoToIndex(0);
    }
    host()->ShowScreenInPickerContents(GURL());
    if (step_shown_callback) {
      std::move(step_shown_callback).Run(true);
    }
  }

  void OnNavigateBackRequested() override {
    NavigateBackInternal(host()->GetPickerContents());
  }

 private:
  // We want to load the WebUI page exactly once, and do more lightweight
  // transitions if we need to switch back to this step later. So we track
  // whether the UI has been loaded or not.
  // Note that this relies on other steps not clearing the picker contents and
  // using their own instead.
  bool loaded_ui_in_picker_contents_ = false;
  const GURL initial_url_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class DiceSignInStepController : public ProfileManagementStepController {
 public:
  explicit DiceSignInStepController(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerDiceSignInProvider> dice_sign_in_provider,
      ProfilePickerDiceSignInProvider::SignedInCallback signed_in_callback)
      : ProfileManagementStepController(host),
        signed_in_callback_(std::move(signed_in_callback)),
        dice_sign_in_provider_(std::move(dice_sign_in_provider)) {
    DCHECK(signed_in_callback_);
  }

  ~DiceSignInStepController() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    DCHECK(step_shown_callback);
    DCHECK(signed_in_callback_) << "Attempting to show Dice step again while "
                                   "it was previously completed";
    // Unretained ok because the provider is owned by `this`.
    dice_sign_in_provider_->SwitchToSignIn(
        std::move(step_shown_callback),
        base::BindOnce(&DiceSignInStepController::OnStepFinished,
                       base::Unretained(this)));
  }

  void OnHidden() override {
    host()->SetNativeToolbarVisible(false);
    // We don't reset the provider when we navigate back as we want to keep this
    // page and the ephemeral profile around for performance reasons.
    // The caller should delete the step if clearing the provider is needed.
  }

  bool CanPopStep() const override {
    return ProfileManagementStepController::CanPopStep() &&
           dice_sign_in_provider_ && dice_sign_in_provider_->IsInitialized();
  }

  void OnReloadRequested() override {
    // Sign-in may fail due to connectivity issues, allow reloading.
    if (dice_sign_in_provider_)
      dice_sign_in_provider_->ReloadSignInPage();
  }

  void OnNavigateBackRequested() override {
    if (dice_sign_in_provider_)
      NavigateBackInternal(dice_sign_in_provider_->contents());
  }

 private:
  void OnStepFinished(Profile* profile,
                      const CoreAccountInfo& account_info,
                      std::unique_ptr<content::WebContents> contents) {
    std::move(signed_in_callback_)
        .Run(profile, account_info, std::move(contents));
    // The step controller can be destroyed when `signed_in_callback_` runs.
    // Don't interact with members below.
  }

  ProfilePickerDiceSignInProvider::SignedInCallback signed_in_callback_;

  std::unique_ptr<ProfilePickerDiceSignInProvider> dice_sign_in_provider_;
};

class FinishSamlSignInStepController : public ProfileManagementStepController {
 public:
  explicit FinishSamlSignInStepController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      base::OnceCallback<void(PostHostClearedCallback)>
          finish_picker_section_callback)
      : ProfileManagementStepController(host),
        profile_(profile),
        contents_(std::move(contents)),
        finish_picker_section_callback_(
            std::move(finish_picker_section_callback)) {
    DCHECK(finish_picker_section_callback_);
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile_, ProfileKeepAliveOrigin::kProfileCreationSamlFlow);
  }

  ~FinishSamlSignInStepController() override {
    if (finish_picker_section_callback_) {
      finish_picker_section_callback_.Reset();
    }
  }

  void Show(base::OnceCallback<void(bool)> step_shown_callback,
            bool reset_state) override {
    // First, stop showing `contents_` to free it up so it can be moved to a new
    // browser window.
    host()->ShowScreenInPickerContents(
        GURL(url::kAboutBlankURL),
        /*navigation_finished_closure=*/
        base::BindOnce(&FinishSamlSignInStepController::OnSignInContentsFreedUp,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNavigateBackRequested() override {
    // Not supported here
  }

 private:
  // Note: This will be executed after the profile management view closes, so
  // the step instance will already be deleted.
  static void ContinueSAMLSignin(std::unique_ptr<content::WebContents> contents,
                                 Browser* browser) {
    DCHECK(browser);
    browser->tab_strip_model()->ReplaceWebContentsAt(0, std::move(contents));

    ProfileMetrics::LogProfileAddSignInFlowOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kSAML);
  }

  void OnSignInContentsFreedUp() {
    DCHECK(finish_picker_section_callback_);

    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

    // Second, ensure the profile creation and set up is complete.
    FinalizeNewProfileSetup(profile_,
                            profiles::GetDefaultNameForNewEnterpriseProfile(),
                            /*is_default_name=*/false);

    // Finally, instruct the flow terminate in the picker and continue in a full
    // browser window.
    auto continue_callback = PostHostClearedCallback(
        base::BindOnce(&FinishSamlSignInStepController::ContinueSAMLSignin,
                       std::move(contents_)));
    std::move(finish_picker_section_callback_)
        .Run(std::move(continue_callback));
  }

  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<content::WebContents> contents_;

  // Callback to be executed to close the flow host, when it is ready to
  // continue the SAML sign-in in the full browser.
  base::OnceCallback<void(PostHostClearedCallback)>
      finish_picker_section_callback_;

  base::WeakPtrFactory<FinishSamlSignInStepController> weak_ptr_factory_{this};
};

#endif

class PostSignInStepController : public ProfileManagementStepController {
 public:
  explicit PostSignInStepController(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerSignedInFlowController> signed_in_flow)
      : ProfileManagementStepController(host),
        signed_in_flow_(std::move(signed_in_flow)) {}

  ~PostSignInStepController() override = default;

  void Show(base::OnceCallback<void(bool)> step_shown_callback,
            bool reset_state) override {
    signed_in_flow_->Init();
    if (step_shown_callback) {
      std::move(step_shown_callback).Run(true);
    }
  }
  void OnHidden() override { signed_in_flow_->Cancel(); }

  void OnNavigateBackRequested() override {
    // Do nothing, navigating back is not allowed.
  }

 private:
  std::unique_ptr<ProfilePickerSignedInFlowController> signed_in_flow_;
  base::WeakPtrFactory<PostSignInStepController> weak_ptr_factory_{this};
};

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)

class SearchEngineChoiceStepController
    : public ProfileManagementStepController {
 public:
  SearchEngineChoiceStepController(
      ProfilePickerWebContentsHost* host,
      SearchEngineChoiceService* search_engine_choice_service,
      base::OnceClosure step_completed_callback)
      : ProfileManagementStepController(host),
        search_engine_choice_service_(search_engine_choice_service),
        step_completed_callback_(std::move(step_completed_callback)) {}

  void Show(base::OnceCallback<void(bool success)> step_shown_callback,
            bool reset_state) override {
    CHECK(reset_state);

    bool should_show_search_engine_choice_step =
        search_engine_choice_service_ &&
        search_engines::IsChoiceScreenFlagEnabled(
            search_engines::ChoicePromo::kFre);

    if (!should_show_search_engine_choice_step) {
      std::move(step_completed_callback_).Run();
      return;
    }

    base::OnceClosure navigation_finished_closure =
        base::BindOnce(&SearchEngineChoiceStepController::OnLoadFinished,
                       base::Unretained(this));
    if (step_shown_callback) {
      // Notify the caller first.
      navigation_finished_closure =
          base::BindOnce(std::move(step_shown_callback), true)
              .Then(std::move(navigation_finished_closure));
    }

    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kFreChoiceScreenWasDisplayed);
    host()->ShowScreenInPickerContents(
        GURL(chrome::kChromeUISearchEngineChoiceURL),
        std::move(navigation_finished_closure));
  }

  void OnNavigateBackRequested() override {
    // Do nothing, navigating back is not allowed.
    NOTREACHED_NORETURN();
  }

 private:
  void OnLoadFinished() {
    auto* search_engine_choice_ui = host()
                                        ->GetPickerContents()
                                        ->GetWebUI()
                                        ->GetController()
                                        ->GetAs<SearchEngineChoiceUI>();
    CHECK(search_engine_choice_ui);
    CHECK(step_completed_callback_);
    search_engine_choice_ui->Initialize(
        /*display_dialog_callback=*/base::OnceClosure(),
        /*on_choice_made_callback=*/std::move(step_completed_callback_),
        SearchEngineChoiceService::EntryPoint::kProfilePicker);
  }

  // May be nullptr.
  raw_ptr<SearchEngineChoiceService> search_engine_choice_service_;

  // Callback to be executed when the step is completed.
  base::OnceClosure step_completed_callback_;
};

#endif  // BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)

}  // namespace

// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForProfilePickerApp(
    ProfilePickerWebContentsHost* host,
    const GURL& initial_url) {
  return std::make_unique<ProfilePickerAppStepController>(host, initial_url);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForDiceSignIn(
    ProfilePickerWebContentsHost* host,
    std::unique_ptr<ProfilePickerDiceSignInProvider> dice_sign_in_provider,
    ProfilePickerDiceSignInProvider::SignedInCallback signed_in_callback) {
  return std::make_unique<DiceSignInStepController>(
      host, std::move(dice_sign_in_provider), std::move(signed_in_callback));
}

// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForFinishSamlSignIn(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    std::unique_ptr<content::WebContents> contents,
    base::OnceCallback<void(PostHostClearedCallback)>
        finish_picker_section_callback) {
  return std::make_unique<FinishSamlSignInStepController>(
      host, profile, std::move(contents),
      std::move(finish_picker_section_callback));
}

#endif

// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForPostSignInFlow(
    ProfilePickerWebContentsHost* host,
    std::unique_ptr<ProfilePickerSignedInFlowController> signed_in_flow) {
  return std::make_unique<PostSignInStepController>(host,
                                                    std::move(signed_in_flow));
}

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForSearchEngineChoice(
    ProfilePickerWebContentsHost* host,
    SearchEngineChoiceService* search_engine_choice_service,
    base::OnceClosure callback) {
  return std::make_unique<SearchEngineChoiceStepController>(
      host, search_engine_choice_service, std::move(callback));
}
#endif

ProfileManagementStepController::ProfileManagementStepController(
    ProfilePickerWebContentsHost* host)
    : host_(host) {}

ProfileManagementStepController::~ProfileManagementStepController() = default;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfileManagementStepController::OnReloadRequested() {}
#endif

void ProfileManagementStepController::NavigateBackInternal(
    content::WebContents* contents) {
  if (contents && contents->GetController().CanGoBack()) {
    contents->GetController().GoBack();
    return;
  }

  if (CanPopStep()) {
    DCHECK(pop_step_callback_);
    std::move(pop_step_callback_).Run();
  }
}

bool ProfileManagementStepController::CanPopStep() const {
  return !pop_step_callback_.is_null();
}
