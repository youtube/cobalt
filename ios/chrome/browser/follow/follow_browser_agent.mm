// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_browser_agent.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/feed/core/shared_prefs/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/follow/follow_service.h"
#import "ios/chrome/browser/follow/follow_service_factory.h"
#import "ios/chrome/browser/follow/web_page_urls.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/feed_commands.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum number of times the First Follow UI must be shown.
constexpr int kFirstFollowModalShownMaxCount = 3;

// Old deprecated key used to store how many time the First Follow UI
// has been displayed in NSUserDefaults. Needs to be removed when the
// migration code in `ShouldShowFirstFollowUI()` is removed.
NSString* const kDisplayedFirstFollowModalCountKey =
    @"DisplayedFirstFollowModalCount";

// Time delay in showing and announcing the notification after a site is
// followed/unfollowed from follow feed management.
const base::TimeDelta kSnackbarMessageVoiceOverDelay = base::Seconds(0.8);

// Returns whether the First Follow UI must be displayed.
bool ShouldShowFirstFollowUI(PrefService* pref_service) {
  // Migrate the old preference from NSUserDefaults if it exists. This code
  // needs to be removed in version M-120.
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  if ([user_defaults objectForKey:kDisplayedFirstFollowModalCountKey]) {
    const NSUInteger count =
        [user_defaults integerForKey:kDisplayedFirstFollowModalCountKey];

    pref_service->SetInteger(prefs::kFirstFollowUIShownCount, count);
    [user_defaults removeObjectForKey:kDisplayedFirstFollowModalCountKey];
  }

  if (experimental_flags::ShouldAlwaysShowFirstFollow())
    return true;

  if (experimental_flags::ShouldResetFirstFollowCount()) {
    pref_service->ClearPref(prefs::kFirstFollowUIShownCount);
    experimental_flags::DidResetFirstFollowCount();
  }

  const int count = pref_service->GetInteger(prefs::kFirstFollowUIShownCount);
  if (count >= kFirstFollowModalShownMaxCount)
    return false;

  return true;
}

// Returns whether the source is from a menu action.
bool IsFollowSourceFromMenu(FollowSource source) {
  switch (source) {
    case FollowSource::OverflowMenu:
    case FollowSource::PopupMenu:
      return true;

    case FollowSource::Management:
    case FollowSource::Retry:
    case FollowSource::Undo:
      return false;
  }
}

}  // namespace

BROWSER_USER_DATA_KEY_IMPL(FollowBrowserAgent)

FollowBrowserAgent::~FollowBrowserAgent() = default;

bool FollowBrowserAgent::IsWebSiteFollowed(WebPageURLs* web_page_urls) {
  return GetFollowService()->IsWebSiteFollowed(web_page_urls);
}

NSURL* FollowBrowserAgent::GetRecommendedSiteURL(WebPageURLs* web_page_urls) {
  return GetFollowService()->GetRecommendedSiteURL(web_page_urls);
}

NSArray<FollowedWebSite*>* FollowBrowserAgent::GetFollowedWebSites() {
  return GetFollowService()->GetFollowedWebSites();
}

void FollowBrowserAgent::LoadFollowedWebSites() {
  return GetFollowService()->LoadFollowedWebSites();
}

void FollowBrowserAgent::FollowWebSite(WebPageURLs* web_page_urls,
                                       FollowSource source) {
  // Record if the source is from a menu.
  if (IsFollowSourceFromMenu(source)) {
    [GetMetricsRecorder() recordFollowFromMenu];
    [GetMetricsRecorder()
        recordFollowRequestedWithType:FollowRequestType::kFollowRequestFollow];
  }

  GetFollowService()->FollowWebSite(
      web_page_urls, source,
      base::BindOnce(&FollowBrowserAgent::OnFollowResponse, AsWeakPtr(),
                     web_page_urls, source));
}

void FollowBrowserAgent::UnfollowWebSite(WebPageURLs* web_page_urls,
                                         FollowSource source) {
  // Record if the source is from a menu.
  if (IsFollowSourceFromMenu(source)) {
    [GetMetricsRecorder() recordUnfollowFromMenu];
    [GetMetricsRecorder() recordFollowRequestedWithType:
                              FollowRequestType::kFollowRequestUnfollow];
  }

  GetFollowService()->UnfollowWebSite(
      web_page_urls, source,
      base::BindOnce(&FollowBrowserAgent::OnUnfollowResponse, AsWeakPtr(),
                     web_page_urls, source));
}

void FollowBrowserAgent::SetUIProviders(
    id<NewTabPageCommands> new_tab_page_commands,
    id<SnackbarCommands> snack_bar_commands,
    id<FeedCommands> feed_commands) {
  new_tab_page_commands_ = new_tab_page_commands;
  snack_bar_commands_ = snack_bar_commands;
  feed_commands_ = feed_commands;
}

void FollowBrowserAgent::ClearUIProviders() {
  new_tab_page_commands_ = nil;
  snack_bar_commands_ = nil;
  feed_commands_ = nil;
}

void FollowBrowserAgent::AddObserver(Observer* observer) {
  GetFollowService()->AddObserver(observer);
}

void FollowBrowserAgent::RemoveObserver(Observer* observer) {
  GetFollowService()->RemoveObserver(observer);
}

base::WeakPtr<FollowBrowserAgent> FollowBrowserAgent::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

FollowBrowserAgent::FollowBrowserAgent(Browser* browser) : browser_(browser) {}

void FollowBrowserAgent::ShowOverlayMessage(FollowSource source,
                                            NSString* message,
                                            NSString* button_text,
                                            MessageBlock message_action,
                                            CompletionBlock completion_action) {
  base::WeakPtr<FollowBrowserAgent> weak_ptr = AsWeakPtr();
  base::OnceClosure closure =
      base::BindOnce(&FollowBrowserAgent::ShowOverlayMessageHelper, weak_ptr,
                     message, button_text, message_action, completion_action);

  // Delay showing the snackbar message when voice over is on and the user has
  // followed/unfollowed the site through feed management. This is to avoid the
  // announcement being cut off by the addition of a new row to the feed
  // management table.
  // TODO(crbug.com/1398955): Temporary solution. A permanent solution should be
  // in place to make sure that the agent verifies that the feed management UI
  // is updated before showing the snackbar message.
  if (UIAccessibilityIsVoiceOverRunning() &&
      source == FollowSource::Management) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(closure), kSnackbarMessageVoiceOverDelay);
    return;
  }

  std::move(closure).Run();
}

void FollowBrowserAgent::ShowOverlayMessageHelper(
    NSString* message,
    NSString* button_text,
    MessageBlock message_action,
    CompletionBlock completion_action) {
  [snack_bar_commands_ showSnackbarWithMessage:message
                                    buttonText:button_text
                                 messageAction:message_action
                              completionAction:completion_action];
}

void FollowBrowserAgent::OnFollowResponse(WebPageURLs* web_page_urls,
                                          FollowSource source,
                                          FollowResult result,
                                          FollowedWebSite* web_site) {
  switch (result) {
    case FollowResult::Success:
      OnFollowSuccess(web_page_urls, source, web_site);
      break;

    case FollowResult::Failure:
      OnFollowFailure(web_page_urls, source, web_site);
      break;
  }
}

void FollowBrowserAgent::OnUnfollowResponse(WebPageURLs* web_page_urls,
                                            FollowSource source,
                                            FollowResult result,
                                            FollowedWebSite* web_site) {
  switch (result) {
    case FollowResult::Success:
      OnUnfollowSuccess(web_page_urls, source, web_site);
      break;

    case FollowResult::Failure:
      OnUnfollowFailure(web_page_urls, source, web_site);
      break;
  }
}

void FollowBrowserAgent::OnFollowSuccess(WebPageURLs* web_page_urls,
                                         FollowSource source,
                                         FollowedWebSite* web_site) {
  // Record if the source is from a menu.
  if (IsFollowSourceFromMenu(source)) {
    const NSUInteger count = GetFollowService()->GetFollowedWebSites().count;
    [GetMetricsRecorder() recordFollowCount:count
                               forLogReason:FollowCountLogReasonAfterFollow];
  }

  base::UmaHistogramBoolean(
      "ContentSuggestions.Feed.WebFeed.NewFollow.IsRecommended",
      GetFollowService()->GetRecommendedSiteURL(web_page_urls) ? 1 : 0);

  // Enable the feed prefs to show the feed and to expand it if they
  // are disabled.
  PrefService* const pref_service = browser_->GetBrowserState()->GetPrefs();
  if (!pref_service->GetBoolean(prefs::kArticlesForYouEnabled))
    pref_service->SetBoolean(prefs::kArticlesForYouEnabled, true);

  if (!pref_service->GetBoolean(feed::prefs::kArticlesListVisible))
    pref_service->SetBoolean(feed::prefs::kArticlesListVisible, true);

  // Display the First Follow modal UI if needed.
  const bool is_overflow_menu_source = source == FollowSource::OverflowMenu;
  if (is_overflow_menu_source && ShouldShowFirstFollowUI(pref_service)) {
    pref_service->SetInteger(
        prefs::kFirstFollowUIShownCount,
        pref_service->GetInteger(prefs::kFirstFollowUIShownCount) + 1);

    [feed_commands_ showFirstFollowUIForWebSite:web_site];
    return;
  }

  NSString* message =
      l10n_util::GetNSStringF(IDS_IOS_SNACKBAR_MESSAGE_FOLLOW_SUCCEED,
                              base::SysNSStringToUTF16(web_site.title));

  NSString* button_text =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_ACTION_GO_TO_FEED);

  __weak FeedMetricsRecorder* metrics_recorder = GetMetricsRecorder();
  __weak id<NewTabPageCommands> new_tab_page_command = new_tab_page_commands_;

  auto message_action = ^{
    [new_tab_page_command openNTPScrolledIntoFeedType:FeedTypeFollowing];
    [metrics_recorder recordFollowSnackbarTappedWithAction:
                          FollowSnackbarActionType::kSnackbarActionGoToFeed];
  };

  auto completion_action = ^(BOOL success) {
    if (success) {
      [metrics_recorder
          recordFollowConfirmationShownWithType:
              FollowConfirmationType::kFollowSucceedSnackbarShown];
    }
  };
  ShowOverlayMessage(source, message, button_text, message_action,
                     completion_action);
}

void FollowBrowserAgent::OnFollowFailure(WebPageURLs* web_page_urls,
                                         FollowSource source,
                                         FollowedWebSite* web_site) {
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_FOLLOW_FAILED);

  NSString* button_text =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_ACTION_TRY_AGAIN);

  __weak FeedMetricsRecorder* metrics_recorder = GetMetricsRecorder();
  base::WeakPtr<FollowBrowserAgent> weak_ptr = AsWeakPtr();

  auto message_action = ^{
    [metrics_recorder recordFollowSnackbarTappedWithAction:
                          FollowSnackbarActionType::kSnackbarActionRetryFollow];

    // Retry following the website.
    if (weak_ptr)
      weak_ptr->FollowWebSite(web_page_urls, FollowSource::Retry);
  };

  auto completion_action = ^(BOOL success) {
    if (success) {
      [metrics_recorder recordFollowConfirmationShownWithType:
                            FollowConfirmationType::kFollowErrorSnackbarShown];
    }
  };
  ShowOverlayMessage(source, message, button_text, message_action,
                     completion_action);
}

void FollowBrowserAgent::OnUnfollowSuccess(WebPageURLs* web_page_urls,
                                           FollowSource source,
                                           FollowedWebSite* web_site) {
  // Record if the source is from a menu.
  if (IsFollowSourceFromMenu(source)) {
    const NSUInteger count = GetFollowService()->GetFollowedWebSites().count;
    [GetMetricsRecorder() recordFollowCount:count
                               forLogReason:FollowCountLogReasonAfterUnfollow];
  }

  NSString* message =
      l10n_util::GetNSStringF(IDS_IOS_SNACKBAR_MESSAGE_UNFOLLOW_SUCCEED,
                              base::SysNSStringToUTF16(web_site.title));

  NSString* button_text = l10n_util::GetNSString(IDS_IOS_SNACKBAR_ACTION_UNDO);

  __weak FeedMetricsRecorder* metrics_recorder = GetMetricsRecorder();
  base::WeakPtr<FollowBrowserAgent> weak_ptr = AsWeakPtr();

  auto message_action = ^{
    [metrics_recorder recordFollowSnackbarTappedWithAction:
                          FollowSnackbarActionType::kSnackbarActionUndo];

    // Undo unfollowing the website.
    if (weak_ptr)
      weak_ptr->FollowWebSite(web_page_urls, FollowSource::Undo);
  };

  auto completion_action = ^(BOOL success) {
    if (success) {
      [metrics_recorder
          recordFollowConfirmationShownWithType:
              FollowConfirmationType::kUnfollowSucceedSnackbarShown];
    }
  };
  ShowOverlayMessage(source, message, button_text, message_action,
                     completion_action);
}

void FollowBrowserAgent::OnUnfollowFailure(WebPageURLs* web_page_urls,
                                           FollowSource source,
                                           FollowedWebSite* web_site) {
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_UNFOLLOW_FAILED);

  NSString* button_text =
      l10n_util::GetNSString(IDS_IOS_SNACKBAR_ACTION_TRY_AGAIN);

  __weak FeedMetricsRecorder* metrics_recorder = GetMetricsRecorder();
  base::WeakPtr<FollowBrowserAgent> weak_ptr = AsWeakPtr();

  auto message_action = ^{
    [metrics_recorder
        recordFollowSnackbarTappedWithAction:FollowSnackbarActionType::
                                                 kSnackbarActionRetryUnfollow];

    // Retry unfollowing the website.
    if (weak_ptr)
      weak_ptr->UnfollowWebSite(web_page_urls, FollowSource::Retry);
  };

  auto completion_action = ^(BOOL success) {
    if (success) {
      [metrics_recorder
          recordFollowConfirmationShownWithType:
              FollowConfirmationType::kUnfollowErrorSnackbarShown];
    }
  };
  ShowOverlayMessage(source, message, button_text, message_action,
                     completion_action);
}

raw_ptr<FollowService> FollowBrowserAgent::GetFollowService() {
  if (!service_) {
    ChromeBrowserState* browser_state = browser_->GetBrowserState();
    service_ = FollowServiceFactory::GetForBrowserState(browser_state);
    DCHECK(service_);
  }
  return service_;
}

FeedMetricsRecorder* FollowBrowserAgent::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    ChromeBrowserState* browser_state = browser_->GetBrowserState();
    metrics_recorder_ =
        DiscoverFeedServiceFactory::GetForBrowserState(browser_state)
            ->GetFeedMetricsRecorder();
  }
  return metrics_recorder_;
}
