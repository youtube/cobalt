// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/action.h"

#include <cstring>
#include <utility>

#include "base/callback_list.h"
#include "base/check_is_test.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/enterprise/idle/action_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/enterprise/idle/dialog_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profile_picker.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace enterprise_idle {

namespace {

#if !BUILDFLAG(IS_ANDROID)
bool ProfileHasBrowsers(const Profile* profile) {
  DCHECK(profile);
  profile = profile->GetOriginalProfile();
  return base::ranges::any_of(
      *BrowserList::GetInstance(), [profile](Browser* browser) {
        return browser->profile()->GetOriginalProfile() == profile;
      });
}

// Wrapper Action for DialogManager. Shows a 30s warning dialog, shared across
// profiles.
//
// Unlike other Actions, this does NOT correspond to the ActionType enum, or a
// value in the IdleTimeoutActions policy. Instead, it's created by
// ActionFactory if appropriate.
class ShowDialogAction : public Action {
 public:
  explicit ShowDialogAction(base::flat_set<ActionType> action_types)
      : Action(/*priority=*/-1), action_types_(action_types) {}

  void Run(Profile* profile, Continuation continuation) override {
    base::TimeDelta timeout =
        profile->GetPrefs()->GetTimeDelta(prefs::kIdleTimeout);
    continuation_ = std::move(continuation);
    // Action object's lifetime extends until it calls `continuation_`, so
    // passing `this` as a raw pointer is safe.
    subscription_ = DialogManager::GetInstance()->ShowDialog(
        timeout, action_types_,
        base::BindOnce(&ShowDialogAction::OnCloseFinished,
                       base::Unretained(this)));
  }

  bool ShouldNotifyUserOfPendingDestructiveAction(Profile* profile) override {
    NOTREACHED();  // Should only be called in ActionFactory::Build().
    return false;
  }

 private:
  void OnCloseFinished(bool expired) {
    std::move(continuation_).Run(/*success=*/expired);
  }

  base::flat_set<ActionType> action_types_;
  Action::Continuation continuation_;
  base::CallbackListSubscription subscription_;
};

// Action that closes all browsers for a Profile.
class CloseBrowsersAction : public Action {
 public:
  CloseBrowsersAction()
      : Action(static_cast<int>(ActionType::kCloseBrowsers)) {}

  // Action:
  void Run(Profile* profile, Continuation continuation) override {
    if (!ProfileHasBrowsers(profile)) {
      // No browsers for this profile, so it'd be a no-op. Finish immediately.
      std::move(continuation).Run(/*success=*/true);
      return;
    }

    continuation_ = std::move(continuation);
    // TODO(crbug.com/1316551): Get customer feedback on whether
    // skip_beforeunload should be true or false.
    BrowserList::CloseAllBrowsersWithProfile(
        profile,
        base::BindRepeating(&CloseBrowsersAction::OnCloseSuccess,
                            base::Unretained(this)),
        base::BindRepeating(&CloseBrowsersAction::OnCloseAborted,
                            base::Unretained(this)),
        /*skip_beforeunload=*/true);
  }

  bool ShouldNotifyUserOfPendingDestructiveAction(Profile* profile) override {
    // If there are no browsers, closing is a no-op. No need for a dialog then.
    return profile && ProfileHasBrowsers(profile);
  }

 private:
  void OnCloseSuccess(const base::FilePath& profile_dir) {
    std::move(continuation_).Run(/*success=*/true);
  }

  void OnCloseAborted(const base::FilePath& profile_dir) {
    std::move(continuation_).Run(/*success=*/false);
  }
  Action::Continuation continuation_;
  base::CallbackListSubscription subscription_;
};

// Action that shows the Profile Picker.
class ShowProfilePickerAction : public Action {
 public:
  ShowProfilePickerAction()
      : Action(static_cast<int>(ActionType::kShowProfilePicker)) {}

  // Action:
  void Run(Profile* profile, Continuation continuation) override {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileIdle));
    std::move(continuation).Run(true);
  }

  bool ShouldNotifyUserOfPendingDestructiveAction(Profile* profile) override {
    return false;
  }
};
#endif  // !BUILDFLAG(IS_ANDROID)

// Action that clears one or more types of data via BrowsingDataRemover.
// Multiple data types may be grouped into a single ClearBrowsingDataAction
// object.
//
// TODO(crbug.com/1326685): Call ChromeBrowsingDataLifetimeManager, instead of
// BrowsingDataRemover directly? Especially if we add a keepalive, or use
// kClearBrowsingDataOnExitDeletionPending...
class ClearBrowsingDataAction : public Action,
                                content::BrowsingDataRemover::Observer {
 public:
  explicit ClearBrowsingDataAction(
      base::flat_set<ActionType> action_types,
      content::BrowsingDataRemover* browsing_data_remover_for_testing)
      : Action(static_cast<int>(ActionType::kClearBrowsingHistory)),
        action_types_(action_types),
        browsing_data_remover_for_testing_(browsing_data_remover_for_testing) {}

  ~ClearBrowsingDataAction() override = default;

  // Action:
  void Run(Profile* profile, Continuation continuation) override {
    auto* remover = profile->GetBrowsingDataRemover();
    if (browsing_data_remover_for_testing_) {
      remover = browsing_data_remover_for_testing_.get();
    }
    observation_.Observe(remover);
    continuation_ = std::move(continuation);
    // Action object's lifetime extends until it calls `continuation_`, so
    // passing `this` as a raw pointer is safe.
    remover->RemoveAndReply(base::Time(), base::Time::Max(), GetRemoveMask(),
                            GetOriginTypeMask(), this);
    // TODO(crbug.com/1326685): Add a pair of keepalives?
  }

  bool ShouldNotifyUserOfPendingDestructiveAction(Profile* profile) override {
#if BUILDFLAG(IS_ANDROID)
    return true;
#else
    return profile && ProfileHasBrowsers(profile);
#endif
  }

  // content::BrowsingDataRemoverObserver::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    bool success = failed_data_types == 0;
    observation_.Reset();
    std::move(continuation_).Run(success);
  }

 private:
  uint64_t GetRemoveMask() const {
    using content::BrowsingDataRemover;
    static const std::pair<ActionType, uint64_t> entries[] = {
        {ActionType::kClearBrowsingHistory,
         chrome_browsing_data_remover::DATA_TYPE_HISTORY},
        {ActionType::kClearDownloadHistory,
         BrowsingDataRemover::DATA_TYPE_DOWNLOADS},
        {ActionType::kClearCookiesAndOtherSiteData,
         chrome_browsing_data_remover::DATA_TYPE_SITE_DATA},
        {ActionType::kClearCachedImagesAndFiles,
         BrowsingDataRemover::DATA_TYPE_CACHE},
        {ActionType::kClearPasswordSignin,
         chrome_browsing_data_remover::DATA_TYPE_PASSWORDS},
        {ActionType::kClearAutofill,
         chrome_browsing_data_remover::DATA_TYPE_FORM_DATA},
        {ActionType::kClearSiteSettings,
         chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS},
        {ActionType::kClearHostedAppData,
         chrome_browsing_data_remover::DATA_TYPE_SITE_DATA}};
    uint64_t result = 0;
    for (const auto& [action_type, mask] : entries) {
      if (base::Contains(action_types_, action_type)) {
        result |= mask;
      }
    }
    return result;
  }

  uint64_t GetOriginTypeMask() const {
    using content::BrowsingDataRemover;
    uint64_t result = 0;
    if (base::Contains(action_types_,
                       ActionType::kClearCookiesAndOtherSiteData)) {
      result |= BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
    }
    if (base::Contains(action_types_, ActionType::kClearHostedAppData)) {
      result |= BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    }
    return result;
  }

  base::flat_set<ActionType> action_types_;
  raw_ptr<content::BrowsingDataRemover> browsing_data_remover_for_testing_;
  base::ScopedObservation<content::BrowsingDataRemover,
                          content::BrowsingDataRemover::Observer>
      observation_{this};
  Continuation continuation_;
};

class ReloadPagesAction : public Action {
 public:
  ReloadPagesAction() : Action(static_cast<int>(ActionType::kReloadPages)) {}

  void Run(Profile* profile, Continuation continuation) override {
#if BUILDFLAG(IS_ANDROID)
    // This covers regular tabs, PWAs, and CCTs.
    for (TabModel* model : TabModelList::models()) {
#else
    // This covers regular tabs and PWAs.
    for (Browser* browser : *BrowserList::GetInstance()) {
      TabStripModel* model = browser->tab_strip_model();
#endif  // BUILDFLAG(IS_ANDROID)
      if (model->GetProfile() != profile) {
        continue;  // Deliberately ignore incognito.
      }
      for (int i = 0; i < model->GetTabCount(); i++) {
        model->GetWebContentsAt(i)->GetController().Reload(
            content::ReloadType::NORMAL,
            /*check_for_repost=*/true);
      }
    }
    std::move(continuation).Run(/*success=*/true);
  }

  bool ShouldNotifyUserOfPendingDestructiveAction(Profile* profile) override {
#if BUILDFLAG(IS_ANDROID)
    return true;
#else
    return profile && ProfileHasBrowsers(profile);
#endif
  }
};

}  // namespace

Action::Action(int priority) : priority_(priority) {}

Action::~Action() = default;

bool ActionFactory::CompareActionsByPriority::operator()(
    const std::unique_ptr<Action>& a,
    const std::unique_ptr<Action>& b) const {
  return a->priority() > b->priority();
}

// static
ActionFactory* ActionFactory::GetInstance() {
  static base::NoDestructor<ActionFactory> instance;
  return instance.get();
}

ActionFactory::ActionQueue ActionFactory::Build(
    Profile* profile,
    const std::vector<ActionType>& action_types) {
  std::vector<std::unique_ptr<Action>> actions;

  base::flat_set<ActionType> clear_actions;
  for (auto action_type : action_types) {
    switch (action_type) {
#if !BUILDFLAG(IS_ANDROID)
      case ActionType::kCloseBrowsers:
        actions.push_back(std::make_unique<CloseBrowsersAction>());
        break;
      case ActionType::kShowProfilePicker:
        actions.push_back(std::make_unique<ShowProfilePickerAction>());
        break;
#endif  // !BUILDFLAG(IS_ANDROID)

      // "clear_*" actions are all grouped into a single Action object. Collect
      // them in a flat_set<>, and create the shared object once we have the
      // entire collection.
      case ActionType::kClearBrowsingHistory:
      case ActionType::kClearDownloadHistory:
      case ActionType::kClearCookiesAndOtherSiteData:
      case ActionType::kClearCachedImagesAndFiles:
      case ActionType::kClearPasswordSignin:
      case ActionType::kClearAutofill:
      case ActionType::kClearSiteSettings:
      case ActionType::kClearHostedAppData:
        clear_actions.insert(action_type);
        break;

      case ActionType::kReloadPages:
        actions.push_back(std::make_unique<ReloadPagesAction>());
        break;

      default:
        // TODO(crbug.com/1316551): Perform validation in the `PolicyHandler`.
        NOTREACHED();
    }
  }

  if (!clear_actions.empty()) {
    // Merge "clear_*" actions into a single Action.
    actions.push_back(std::make_unique<ClearBrowsingDataAction>(
        std::move(clear_actions), browsing_data_remover_for_testing_));
  }

#if !BUILDFLAG(IS_ANDROID)
  bool needs_dialog = base::ranges::any_of(actions, [profile](const auto& a) {
    return a->ShouldNotifyUserOfPendingDestructiveAction(profile);
  });
  if (needs_dialog) {
    actions.push_back(std::make_unique<ShowDialogAction>(
        base::flat_set<ActionType>(action_types)));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return ActionQueue(ActionQueue::value_compare(), std::move(actions));
}

ActionFactory::ActionFactory() = default;
ActionFactory::~ActionFactory() = default;

void ActionFactory::SetBrowsingDataRemoverForTesting(
    content::BrowsingDataRemover* remover) {
  CHECK_IS_TEST();
  browsing_data_remover_for_testing_ = remover;
}

}  // namespace enterprise_idle
