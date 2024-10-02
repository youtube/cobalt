// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/permissions/features.h"
#include "components/permissions/origin_keyed_permission_action_service.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/permissions/switches.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#endif

namespace permissions {

const char kAbusiveNotificationRequestsEnforcementMessage[] =
    "Chrome is blocking notification permission requests on this site because "
    "the site tends to show permission requests that mislead, trick, or force "
    "users into allowing notifications. You should fix the issues as soon as "
    "possible and submit your site for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048.";

const char kAbusiveNotificationRequestsWarningMessage[] =
    "Chrome might start blocking notification permission requests on this site "
    "in the future because the site tends to show permission requests that "
    "mislead, trick, or force users into allowing notifications. You should "
    "fix the issues as soon as possible and submit your site for another "
    "review. Learn more at https://support.google.com/webtools/answer/9799048.";

constexpr char kAbusiveNotificationContentEnforcementMessage[] =
    "Chrome is blocking notification permission requests on this site because "
    "the site tends to show notifications with content that mislead or trick "
    "users. You should fix the issues as soon as possible and submit your site "
    "for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048";

constexpr char kAbusiveNotificationContentWarningMessage[] =
    "Chrome might start blocking notification permission requests on this site "
    "in the future because the site tends to show notifications with content "
    "that mislead or trick users. You should fix the issues as soon as "
    "possible and submit your site for another review. Learn more at "
    "https://support.google.com/webtools/answer/9799048";

constexpr char kDisruptiveNotificationBehaviorEnforcementMessage[] =
    "Chrome is blocking notification permission requests on this site because "
    "the site exhibits behaviors that may be disruptive to users.";

namespace {

// In case of multiple permission requests that use chip UI, a newly added
// request will preempt the currently showing request, which is put back to the
// queue, and will be shown later. To reduce user annoyance, if a quiet chip
// permission prompt was displayed longer than `kQuietChipIgnoreTimeout`, we
// consider it as shown long enough and it will not be shown again after it is
// preempted.
// TODO(crbug.com/1221083): If a user switched tabs, do not include that time as
// "shown".
bool ShouldShowQuietRequestAgainIfPreempted(
    absl::optional<base::Time> request_display_start_time) {
  if (request_display_start_time->is_null()) {
    return true;
  }

  static constexpr base::TimeDelta kQuietChipIgnoreTimeout = base::Seconds(8.5);
  return base::Time::Now() - request_display_start_time.value() <
         kQuietChipIgnoreTimeout;
}

bool IsMediaRequest(RequestType type) {
#if !BUILDFLAG(IS_ANDROID)
  if (type == RequestType::kCameraPanTiltZoom)
    return true;
#endif
  return type == RequestType::kMicStream || type == RequestType::kCameraStream;
}

bool ShouldGroupRequests(PermissionRequest* a, PermissionRequest* b) {
  if (a->requesting_origin() != b->requesting_origin())
    return false;

  // Group if both requests are media requests.
  if (IsMediaRequest(a->request_type()) && IsMediaRequest(b->request_type())) {
    return true;
  }

  return false;
}

}  // namespace

// PermissionRequestManager ----------------------------------------------------

bool PermissionRequestManager::PermissionRequestSource::
    IsSourceFrameInactiveAndDisallowActivation() const {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(requesting_frame_id);
  return !rfh ||
         rfh->IsInactiveAndDisallowActivation(
             content::DisallowActivationReasonId::kPermissionRequestSource);
}

PermissionRequestManager::~PermissionRequestManager() {
  DCHECK(!IsRequestInProgress());
  DCHECK(duplicate_requests_.empty());
  DCHECK(pending_permission_requests_.IsEmpty());

  for (Observer& observer : observer_list_)
    observer.OnPermissionRequestManagerDestructed();
}

void PermissionRequestManager::AddRequest(
    content::RenderFrameHost* source_frame,
    PermissionRequest* request) {
  DCHECK(source_frame);
  DCHECK_EQ(content::WebContents::FromRenderFrameHost(source_frame),
            web_contents());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDenyPermissionPrompts)) {
    request->PermissionDenied();
    request->RequestFinished();
    return;
  }

  if (source_frame->IsInactiveAndDisallowActivation(
          content::DisallowActivationReasonId::kPermissionAddRequest)) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  if (source_frame->IsNestedWithinFencedFrame()) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  if (request->GetContentSettingsType() == ContentSettingsType::NOTIFICATIONS) {
    bool app_level_settings_allow_site_notifications =
        enabled_app_level_notification_permission_for_testing_.has_value()
            ? enabled_app_level_notification_permission_for_testing_.value()
            : DoesAppLevelSettingsAllowSiteNotifications();
    base::UmaHistogramBoolean(
        "Permissions.Prompt.Notifications.EnabledAppLevel",
        app_level_settings_allow_site_notifications);

    if (!app_level_settings_allow_site_notifications &&
        base::FeatureList::IsEnabled(
            features::kBlockNotificationPromptsIfDisabledOnAppLevel)) {
      // Automatically cancel site Notification requests when Chrome is not able
      // to send notifications in an app level.
      request->Cancelled();
      request->RequestFinished();
      return;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (is_notification_prompt_cooldown_active_ &&
      request->GetContentSettingsType() == ContentSettingsType::NOTIFICATIONS) {
    // Short-circuit by canceling rather than denying to avoid creating a large
    // number of content setting exceptions on Desktop / disabled notification
    // channels on Android.
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  if (!web_contents_supports_permission_requests_) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  // TODO(tsergeant): change the UMA to no longer mention bubble.
  base::RecordAction(base::UserMetricsAction("PermissionBubbleRequest"));

  // TODO(gbillock): is there a race between an early request on a
  // newly-navigated page and the to-be-cleaned-up requests on the previous
  // page? We should maybe listen to DidStartNavigationToPendingEntry (and
  // any other renderer-side nav initiations?). Double-check this for
  // correct behavior on interstitials -- we probably want to basically queue
  // any request for which GetVisibleURL != GetLastCommittedURL.
  CHECK(source_frame->GetMainFrame()->IsInPrimaryMainFrame());
  const GURL main_frame_origin =
      PermissionUtil::GetLastCommittedOriginAsURL(source_frame->GetMainFrame());
  bool is_main_frame =
      url::IsSameOriginWith(main_frame_origin, request->requesting_origin());

  absl::optional<url::Origin> auto_approval_origin =
      PermissionsClient::Get()->GetAutoApprovalOrigin();
  if (auto_approval_origin) {
    if (url::Origin::Create(request->requesting_origin()) ==
        auto_approval_origin.value()) {
      request->PermissionGranted(/*is_one_time=*/false);
    }
    request->RequestFinished();
    return;
  }

  // Don't re-add an existing request or one with a duplicate text request.
  if (auto* existing_request = GetExistingRequest(request)) {
    if (request == existing_request) {
      return;
    }

    // |request| is a duplicate. Add it to |duplicate_requests_| unless it's the
    // same object as |existing_request| or an existing duplicate.
    auto iter = FindDuplicateRequestList(existing_request);
    if (iter == duplicate_requests_.end()) {
      duplicate_requests_.push_back({request->GetWeakPtr()});
      return;
    }

    for (const auto& weak_request : (*iter)) {
      if (weak_request && request == weak_request.get()) {
        return;
      }
    }
    iter->push_back(request->GetWeakPtr());
    return;
  }

  if (is_main_frame) {
    if (IsRequestInProgress()) {
      base::RecordAction(
          base::UserMetricsAction("PermissionBubbleRequestQueued"));
    }
  } else {
    base::RecordAction(
        base::UserMetricsAction("PermissionBubbleIFrameRequestQueued"));
  }

  request->set_requesting_frame_id(source_frame->GetGlobalId());

  QueueRequest(source_frame, request);

  if (!IsRequestInProgress()) {
    ScheduleDequeueRequestIfNeeded();
    return;
  }

  ReprioritizeCurrentRequestIfNeeded();
}

bool PermissionRequestManager::ReprioritizeCurrentRequestIfNeeded() {
  if (pending_permission_requests_.IsEmpty() || !IsRequestInProgress())
    return true;

  auto current_request_fate = CurrentRequestFate::kKeepCurrent;

  if (base::FeatureList::IsEnabled(features::kPermissionQuietChip)) {
    if (ShouldCurrentRequestUseQuietUI() &&
        !ShouldShowQuietRequestAgainIfPreempted(
            current_request_first_display_time_)) {
      current_request_fate = CurrentRequestFate::kFinalize;
    } else if (base::FeatureList::IsEnabled(features::kPermissionChip)) {
      // Preempt current request if it is a quiet UI request.
      if (ShouldCurrentRequestUseQuietUI()) {
        current_request_fate = CurrentRequestFate::kPreempt;
      } else {
        // Pop out all invalid requests in front of the queue.
        while (!pending_permission_requests_.IsEmpty() &&
               !ValidateRequest(pending_permission_requests_.Peek())) {
          pending_permission_requests_.Pop();
        }

        // Here we also try to prioritise the requests. If there's a valid high
        // priority request (high acceptance rate request) in the pending queue,
        // preempt the current request. The valid high priority request, if
        // there's any, is always the front of the queue.
        if (!pending_permission_requests_.IsEmpty() &&
            !PermissionUtil::IsLowPriorityPermissionRequest(
                pending_permission_requests_.Peek())) {
          current_request_fate = CurrentRequestFate::kPreempt;
        }
      }
    } else if (ShouldCurrentRequestUseQuietUI()) {
      current_request_fate = CurrentRequestFate::kPreempt;
    }
  } else {
    if (base::FeatureList::IsEnabled(features::kPermissionChip)) {
      current_request_fate = CurrentRequestFate::kPreempt;
    } else if (ShouldCurrentRequestUseQuietUI()) {
      // If we're displaying a quiet permission request, ignore it in favor of a
      // new permission request.
      current_request_fate = CurrentRequestFate::kFinalize;
    }
  }

  switch (current_request_fate) {
    case CurrentRequestFate::kKeepCurrent:
      return true;
    case CurrentRequestFate::kPreempt: {
      DCHECK(!pending_permission_requests_.IsEmpty());
      auto* next_candidate = pending_permission_requests_.Peek();

      // Consider a case of infinite loop here (eg: 2 low priority requests can
      // preempt each other, causing a loop). We only preempt the current
      // request if the next candidate has just been added to pending queue but
      // not validated yet.
      if (validated_requests_set_.find(next_candidate) !=
          validated_requests_set_.end()) {
        return true;
      }

      pending_permission_requests_.Pop();
      PreemptAndRequeueCurrentRequest();
      pending_permission_requests_.Push(next_candidate);
      ScheduleDequeueRequestIfNeeded();
      return false;
    }
    case CurrentRequestFate::kFinalize:
      // FinalizeCurrentRequests will call ScheduleDequeueRequestIfNeeded on its
      // own.
      FinalizeCurrentRequests(PermissionAction::IGNORED);
      return false;
  }

  return true;
}

bool PermissionRequestManager::ValidateRequest(PermissionRequest* request,
                                               bool should_finalize) {
  const auto iter = request_sources_map_.find(request);
  if (iter == request_sources_map_.end()) {
    return false;
  }

  if (!iter->second.IsSourceFrameInactiveAndDisallowActivation()) {
    return true;
  }

  if (should_finalize) {
    request->Cancelled();
    request->RequestFinished();
    validated_requests_set_.erase(request);
    request_sources_map_.erase(request);
  }

  return false;
}

void PermissionRequestManager::QueueRequest(
    content::RenderFrameHost* source_frame,
    PermissionRequest* request) {
  pending_permission_requests_.Push(request,
                                    true /*reorder_based_on_priority*/);
  request_sources_map_.emplace(
      request, PermissionRequestSource({source_frame->GetGlobalId()}));
}

void PermissionRequestManager::PreemptAndRequeueCurrentRequest() {
  ResetViewStateForCurrentRequest();
  for (auto* current_request : requests_) {
    pending_permission_requests_.Push(current_request);
  }

  // Because the order of the requests is changed, we should not preignore it.
  preignore_timer_.AbandonAndStop();

  requests_.clear();
}

void PermissionRequestManager::UpdateAnchor() {
  if (view_) {
    // When the prompt's anchor is being updated, the prompt view can be
    // recreated for the new browser. Because of that, ignore prompt callbacks
    // while doing that.
    base::AutoReset<bool> ignore(&ignore_callbacks_from_prompt_, true);
    if (!view_->UpdateAnchor())
      RecreateView();
  }
}

void PermissionRequestManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  for (Observer& observer : observer_list_)
    observer.OnNavigation(navigation_handle);

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Cooldown lasts until the next user-initiated navigation, which is defined
  // as either a renderer-initiated navigation with a user gesture, or a
  // browser-initiated navigation.
  //
  // TODO(crbug.com/952347): This check has to be done at DidStartNavigation
  // time, the HasUserGesture state is lost by the time the navigation
  // commits.
  if (!navigation_handle->IsRendererInitiated() ||
      navigation_handle->HasUserGesture()) {
    is_notification_prompt_cooldown_active_ = false;
  }
}

void PermissionRequestManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->IsErrorPage()) {
    permissions::PermissionUmaUtil::
        RecordTopLevelPermissionsHeaderPolicyOnNavigation(
            navigation_handle->GetRenderFrameHost());
  }

  if (!pending_permission_requests_.IsEmpty() || IsRequestInProgress()) {
    // |pending_permission_requests_| and |requests_| will be deleted below,
    // which might be a problem for back-forward cache — the page might be
    // restored later, but the requests won't be. Disable bfcache here if we
    // have any requests here to prevent this from happening.
    content::BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        back_forward_cache::DisabledReason(
            back_forward_cache::DisabledReasonId::kPermissionRequestManager));
  }

  CleanUpRequests();
}

void PermissionRequestManager::DocumentOnLoadCompletedInPrimaryMainFrame() {
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions prompt.
  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::WebContentsDestroyed() {
  // If the web contents has been destroyed, treat the prompt as cancelled.
  CleanUpRequests();

  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add permission
  // prompts or use us otherwise during the destruction.
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

void PermissionRequestManager::OnVisibilityChanged(
    content::Visibility visibility) {
  bool tab_was_hidden = tab_is_hidden_;
  tab_is_hidden_ = visibility == content::Visibility::HIDDEN;
  if (tab_was_hidden == tab_is_hidden_)
    return;

  if (tab_is_hidden_) {
    if (view_) {
      switch (view_->GetTabSwitchingBehavior()) {
        case PermissionPrompt::TabSwitchingBehavior::
            kDestroyPromptButKeepRequestPending:
          DeletePrompt();
          break;
        case PermissionPrompt::TabSwitchingBehavior::
            kDestroyPromptAndIgnoreRequest:
          FinalizeCurrentRequests(PermissionAction::IGNORED);
          break;
        case PermissionPrompt::TabSwitchingBehavior::kKeepPromptAlive:
          break;
      }
    }

    return;
  }

  if (!web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame())
    return;

  if (!IsRequestInProgress()) {
    ScheduleDequeueRequestIfNeeded();
    return;
  }

  if (view_) {
    // We switched tabs away and back while a prompt was active.
    DCHECK_EQ(view_->GetTabSwitchingBehavior(),
              PermissionPrompt::TabSwitchingBehavior::kKeepPromptAlive);
  } else if (current_request_ui_to_use_.has_value()) {
    ShowPrompt();
  }
}

const std::vector<PermissionRequest*>& PermissionRequestManager::Requests() {
  return requests_;
}

GURL PermissionRequestManager::GetRequestingOrigin() const {
  CHECK(!requests_.empty());
  GURL origin = requests_.front()->requesting_origin();
  if (DCHECK_IS_ON()) {
    for (auto* request : requests_)
      DCHECK_EQ(origin, request->requesting_origin());
  }
  return origin;
}

GURL PermissionRequestManager::GetEmbeddingOrigin() const {
  return PermissionUtil::GetLastCommittedOriginAsURL(
      web_contents()->GetPrimaryMainFrame());
}

void PermissionRequestManager::Accept() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    StorePermissionActionForUMA((*requests_iter)->requesting_origin(),
                                (*requests_iter)->request_type(),
                                PermissionAction::GRANTED);
    PermissionGrantedIncludingDuplicates(*requests_iter,
                                         /*is_one_time=*/false);
  }

  NotifyRequestDecided(PermissionAction::GRANTED);
  FinalizeCurrentRequests(PermissionAction::GRANTED);
}

void PermissionRequestManager::AcceptThisTime() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    StorePermissionActionForUMA((*requests_iter)->requesting_origin(),
                                (*requests_iter)->request_type(),
                                PermissionAction::GRANTED_ONCE);
    PermissionGrantedIncludingDuplicates(*requests_iter,
                                         /*is_one_time=*/true);
  }

  NotifyRequestDecided(PermissionAction::GRANTED_ONCE);
  FinalizeCurrentRequests(PermissionAction::GRANTED_ONCE);
}

void PermissionRequestManager::Deny() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);

  // Suppress any further prompts in this WebContents, from any origin, until
  // there is a user-initiated navigation. This stops users from getting
  // trapped in request loops where the website automatically navigates
  // cross-origin (e.g. to another subdomain) to be able to prompt again after
  // a rejection.
  if (base::FeatureList::IsEnabled(
          features::kBlockRepeatedNotificationPermissionPrompts) &&
      base::Contains(requests_, ContentSettingsType::NOTIFICATIONS,
                     &PermissionRequest::GetContentSettingsType)) {
    is_notification_prompt_cooldown_active_ = true;
  }

  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    StorePermissionActionForUMA((*requests_iter)->requesting_origin(),
                                (*requests_iter)->request_type(),
                                PermissionAction::DENIED);
    PermissionDeniedIncludingDuplicates(*requests_iter);
  }

  NotifyRequestDecided(PermissionAction::DENIED);
  FinalizeCurrentRequests(PermissionAction::DENIED);
}

void PermissionRequestManager::Dismiss() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    StorePermissionActionForUMA((*requests_iter)->requesting_origin(),
                                (*requests_iter)->request_type(),
                                PermissionAction::DISMISSED);
    CancelledIncludingDuplicates(*requests_iter);
  }

  NotifyRequestDecided(PermissionAction::DISMISSED);
  FinalizeCurrentRequests(PermissionAction::DISMISSED);
}

void PermissionRequestManager::Ignore() {
  if (ignore_callbacks_from_prompt_)
    return;
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    StorePermissionActionForUMA((*requests_iter)->requesting_origin(),
                                (*requests_iter)->request_type(),
                                PermissionAction::IGNORED);
    CancelledIncludingDuplicates(*requests_iter);
  }

  NotifyRequestDecided(PermissionAction::IGNORED);
  FinalizeCurrentRequests(PermissionAction::IGNORED);
}

void PermissionRequestManager::PreIgnoreQuietPrompt() {
  // Random number of seconds in the range [1.0, 2.0).
  double delay_seconds = 1.0 + 1.0 * base::RandDouble();
  preignore_timer_.Start(
      FROM_HERE, base::Seconds(delay_seconds), this,
      &PermissionRequestManager::PreIgnoreQuietPromptInternal);
}

void PermissionRequestManager::PreIgnoreQuietPromptInternal() {
  DCHECK(!requests_.empty());

  if (requests_.empty()) {
    // If `requests_` was cleared then there is nothing preignore.
    return;
  }

  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    CancelledIncludingDuplicates(*requests_iter, /*is_final_decision=*/false);
  }

  blink::PermissionType permission;
  bool success = PermissionUtil::GetPermissionType(
      requests_[0]->GetContentSettingsType(), &permission);
  DCHECK(success);

  PermissionUmaUtil::PermissionRequestPreignored(permission);
}

bool PermissionRequestManager::WasCurrentRequestAlreadyDisplayed() {
  return current_request_already_displayed_;
}

void PermissionRequestManager::SetDismissOnTabClose() {
  should_dismiss_current_request_ = true;
}

void PermissionRequestManager::SetPromptShown() {
  did_show_prompt_ = true;
}

void PermissionRequestManager::SetDecisionTime() {
  current_request_decision_time_ = base::Time::Now();
}

void PermissionRequestManager::SetManageClicked() {
  set_manage_clicked();
}

void PermissionRequestManager::SetLearnMoreClicked() {
  set_learn_more_clicked();
}

base::WeakPtr<PermissionPrompt::Delegate>
PermissionRequestManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

content::WebContents* PermissionRequestManager::GetAssociatedWebContents() {
  content::WebContents& web_contents = GetWebContents();
  return &web_contents;
}

bool PermissionRequestManager::RecreateView() {
  view_ = view_factory_.Run(web_contents(), this);
  if (!view_) {
    current_request_prompt_disposition_ =
        PermissionPromptDisposition::NONE_VISIBLE;
    if (ShouldDropCurrentRequestIfCannotShowQuietly()) {
      FinalizeCurrentRequests(PermissionAction::IGNORED);
    }
    NotifyPromptRecreateFailed();
    return false;
  }

  current_request_prompt_disposition_ = view_->GetPromptDisposition();
  return true;
}

PermissionRequestManager::PermissionRequestManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PermissionRequestManager>(*web_contents),
      view_factory_(base::BindRepeating(&PermissionPrompt::Create)),
      tab_is_hidden_(web_contents->GetVisibility() ==
                     content::Visibility::HIDDEN),
      auto_response_for_test_(NONE),
      permission_ui_selectors_(
          PermissionsClient::Get()->CreatePermissionUiSelectors(
              web_contents->GetBrowserContext())) {}

void PermissionRequestManager::DequeueRequestIfNeeded() {
  // TODO(olesiamarukhno): Media requests block other media requests from
  // pre-empting them. For example, when a camera request is pending and mic
  // is requested, the camera request remains pending and mic request appears
  // only after the camera request is resolved. This is caused by code in
  // PermissionBubbleMediaAccessHandler and UserMediaClient. We probably don't
  // need two permission queues, so resolve the duplication.

  if (!web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame() || view_ ||
      IsRequestInProgress()) {
    return;
  }

  // Find first valid request.
  while (!pending_permission_requests_.IsEmpty()) {
    auto* next = pending_permission_requests_.Pop();
    if (ValidateRequest(next)) {
      validated_requests_set_.insert(next);
      requests_.push_back(next);
      break;
    }
  }

  if (requests_.empty()) {
    return;
  }

  // Find additional requests that can be grouped with the first one.
  for (; !pending_permission_requests_.IsEmpty();
       pending_permission_requests_.Pop()) {
    auto* front = pending_permission_requests_.Peek();
    if (!ValidateRequest(front))
      continue;

    validated_requests_set_.insert(front);
    if (!ShouldGroupRequests(requests_.front(), front))
      break;

    requests_.push_back(front);
  }

  // Mark the remaining pending requests as validated, so only the "new and has
  // not been validated" requests added to the queue could have effect to
  // priority order
  for (auto* request : pending_permission_requests_) {
    if (ValidateRequest(request, /* should_finalize */ false)) {
      validated_requests_set_.insert(request);
    }
  }

  if (permission_ui_selectors_.empty()) {
    current_request_ui_to_use_ =
        UiDecision(UiDecision::UseNormalUi(), UiDecision::ShowNoWarning());
    ShowPrompt();
    return;
  }

  DCHECK(!current_request_ui_to_use_.has_value());
  // Initialize the selector decisions vector.
  DCHECK(selector_decisions_.empty());
  selector_decisions_.resize(permission_ui_selectors_.size());

  for (size_t selector_index = 0;
       selector_index < permission_ui_selectors_.size(); ++selector_index) {
    // Skip if we have already made a decision due to a higher priority
    // selector
    if (current_request_ui_to_use_.has_value() || !IsRequestInProgress()) {
      break;
    }

    if (permission_ui_selectors_[selector_index]->IsPermissionRequestSupported(
            requests_.front()->request_type())) {
      permission_ui_selectors_[selector_index]->SelectUiToUse(
          requests_.front(),
          base::BindOnce(&PermissionRequestManager::OnPermissionUiSelectorDone,
                         weak_factory_.GetWeakPtr(), selector_index));
      continue;
    }

    OnPermissionUiSelectorDone(
        selector_index,
        PermissionUiSelector::Decision::UseNormalUiAndShowNoWarning());
  }
}

void PermissionRequestManager::ScheduleDequeueRequestIfNeeded() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PermissionRequestManager::DequeueRequestIfNeeded,
                     weak_factory_.GetWeakPtr()));
}

void PermissionRequestManager::ShowPrompt() {
  // There is a race condition where the request might have been removed
  // already so double-checking that there is a request in progress.
  //
  // There is no need to show a new prompt if the previous one still exists.
  if (!IsRequestInProgress() || view_)
    return;

  DCHECK(web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame());
  DCHECK(current_request_ui_to_use_);

  if (tab_is_hidden_) {
    NotifyPromptCreationFailedHiddenTab();
    return;
  }

  if (!ReprioritizeCurrentRequestIfNeeded())
    return;

  if (!RecreateView())
    return;

  if (!current_request_already_displayed_) {
    PermissionUmaUtil::PermissionPromptShown(requests_);

    auto quiet_ui_reason = ReasonForUsingQuietUi();
    if (quiet_ui_reason) {
      switch (*quiet_ui_reason) {
        case QuietUiReason::kEnabledInPrefs:
        case QuietUiReason::kTriggeredByCrowdDeny:
        case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
        case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
          break;
        case QuietUiReason::kTriggeredDueToAbusiveRequests:
          LogWarningToConsole(kAbusiveNotificationRequestsEnforcementMessage);
          break;
        case QuietUiReason::kTriggeredDueToAbusiveContent:
          LogWarningToConsole(kAbusiveNotificationContentEnforcementMessage);
          break;
        case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
          LogWarningToConsole(
              kDisruptiveNotificationBehaviorEnforcementMessage);
          break;
      }
      base::RecordAction(base::UserMetricsAction(
          "Notifications.Quiet.PermissionRequestShown"));
    }

#if !BUILDFLAG(IS_ANDROID)
    PermissionsClient::Get()->TriggerPromptHatsSurveyIfEnabled(
        web_contents()->GetBrowserContext(), requests_[0]->request_type(),
        absl::nullopt, DetermineCurrentRequestUIDisposition(),
        DetermineCurrentRequestUIDispositionReasonForUMA(),
        requests_[0]->GetGestureType(), absl::nullopt, false,
        web_contents()->GetLastCommittedURL(),
        hats_shown_callback_.has_value()
            ? std::move(hats_shown_callback_.value())
            : base::DoNothing());

    hats_shown_callback_.reset();
#endif
  }
  current_request_already_displayed_ = true;
  current_request_first_display_time_ = base::Time::Now();

  NotifyPromptAdded();

  // If in testing mode, automatically respond to the bubble that was shown.
  if (auto_response_for_test_ != NONE) {
    DoAutoResponseForTesting();
  }
}

void PermissionRequestManager::SetHatsShownCallback(
    base::OnceCallback<void()> callback) {
  hats_shown_callback_ = std::move(callback);
}

void PermissionRequestManager::DeletePrompt() {
  DCHECK(view_);
  {
    base::AutoReset<bool> deleting(&ignore_callbacks_from_prompt_, true);
    view_.reset();
  }
  NotifyPromptRemoved();
}

void PermissionRequestManager::ResetViewStateForCurrentRequest() {
  for (const auto& selector : permission_ui_selectors_)
    selector->Cancel();

  current_request_already_displayed_ = false;
  current_request_first_display_time_ = base::Time();
  current_request_decision_time_ = base::Time();
  current_request_prompt_disposition_.reset();
  prediction_grant_likelihood_.reset();
  current_request_ui_to_use_.reset();
  was_decision_held_back_.reset();
  selector_decisions_.clear();
  should_dismiss_current_request_ = false;
  did_show_prompt_ = false;
  did_click_manage_ = false;
  did_click_learn_more_ = false;
  hats_shown_callback_.reset();
  if (view_)
    DeletePrompt();
}

void PermissionRequestManager::FinalizeCurrentRequests(
    PermissionAction permission_action) {
  DCHECK(IsRequestInProgress());
  base::TimeDelta time_to_decision;
  if (!current_request_first_display_time_.is_null() &&
      permission_action != PermissionAction::IGNORED) {
    if (current_request_decision_time_.is_null()) {
      current_request_decision_time_ = base::Time::Now();
    }
    time_to_decision =
        current_request_decision_time_ - current_request_first_display_time_;
  }

  if (time_to_decision_for_test_.has_value()) {
    time_to_decision = time_to_decision_for_test_.value();
    time_to_decision_for_test_.reset();
  }

  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  PermissionUmaUtil::PermissionPromptResolved(
      requests_, web_contents(), permission_action, time_to_decision,
      DetermineCurrentRequestUIDisposition(),
      DetermineCurrentRequestUIDispositionReasonForUMA(),
      prediction_grant_likelihood_, was_decision_held_back_,
      permission_action == PermissionAction::IGNORED
          ? absl::make_optional(
                PermissionsClient::Get()->DetermineIgnoreReason(web_contents()))
          : absl::nullopt,
      did_show_prompt_, did_click_manage_, did_click_learn_more_);

  PermissionDecisionAutoBlocker* autoblocker =
      PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser_context);

  absl::optional<QuietUiReason> quiet_ui_reason;
  if (ShouldCurrentRequestUseQuietUI())
    quiet_ui_reason = ReasonForUsingQuietUi();

  for (PermissionRequest* request : requests_) {
    // TODO(timloh): We only support dismiss and ignore embargo for
    // permissions which use PermissionRequestImpl as the other subclasses
    // don't support GetContentSettingsType.
    if (request->GetContentSettingsType() == ContentSettingsType::DEFAULT)
      continue;

    auto time_since_shown =
        current_request_first_display_time_.is_null()
            ? base::TimeDelta::Max()
            : base::Time::Now() - current_request_first_display_time_;
    PermissionsClient::Get()->OnPromptResolved(
        request->request_type(), permission_action,
        request->requesting_origin(), DetermineCurrentRequestUIDisposition(),
        DetermineCurrentRequestUIDispositionReasonForUMA(),
        request->GetGestureType(), quiet_ui_reason, time_since_shown,
        web_contents());

    PermissionEmbargoStatus embargo_status =
        PermissionEmbargoStatus::NOT_EMBARGOED;
    if (permission_action == PermissionAction::DISMISSED) {
      if (autoblocker->RecordDismissAndEmbargo(
              request->requesting_origin(), request->GetContentSettingsType(),
              ShouldCurrentRequestUseQuietUI())) {
        embargo_status = PermissionEmbargoStatus::REPEATED_DISMISSALS;
      }
    } else if (permission_action == PermissionAction::IGNORED) {
      if (autoblocker->RecordIgnoreAndEmbargo(
              request->requesting_origin(), request->GetContentSettingsType(),
              ShouldCurrentRequestUseQuietUI())) {
        embargo_status = PermissionEmbargoStatus::REPEATED_IGNORES;
      }
    }
    PermissionUmaUtil::RecordEmbargoStatus(embargo_status);
  }
  ResetViewStateForCurrentRequest();
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    RequestFinishedIncludingDuplicates(*requests_iter);
    validated_requests_set_.erase(*requests_iter);
    request_sources_map_.erase(*requests_iter);
  }

  // No need to execute the preignore logic as we canceling currently active
  // requests anyway.
  preignore_timer_.AbandonAndStop();

  requests_.clear();

  for (Observer& observer : observer_list_)
    observer.OnRequestsFinalized();

  ScheduleDequeueRequestIfNeeded();
}

void PermissionRequestManager::CleanUpRequests() {
  // No need to execute the preignore logic as we canceling currently active
  // requests anyway.
  preignore_timer_.AbandonAndStop();

  for (; !pending_permission_requests_.IsEmpty();
       pending_permission_requests_.Pop()) {
    auto* pending_request = pending_permission_requests_.Peek();
    CancelledIncludingDuplicates(pending_request);
    RequestFinishedIncludingDuplicates(pending_request);
    validated_requests_set_.erase(pending_request);
    request_sources_map_.erase(pending_request);
  }

  if (IsRequestInProgress()) {
    std::vector<PermissionRequest*>::iterator requests_iter;
    for (requests_iter = requests_.begin(); requests_iter != requests_.end();
         requests_iter++) {
      CancelledIncludingDuplicates(*requests_iter);
    }

    FinalizeCurrentRequests(should_dismiss_current_request_
                                ? PermissionAction::DISMISSED
                                : PermissionAction::IGNORED);
    should_dismiss_current_request_ = false;
  }
}

PermissionRequest* PermissionRequestManager::GetExistingRequest(
    PermissionRequest* request) const {
  for (PermissionRequest* existing_request : requests_) {
    if (request->IsDuplicateOf(existing_request)) {
      return existing_request;
    }
  }
  return pending_permission_requests_.FindDuplicate(request);
}

PermissionRequestManager::WeakPermissionRequestList::iterator
PermissionRequestManager::FindDuplicateRequestList(PermissionRequest* request) {
  for (auto request_list = duplicate_requests_.begin();
       request_list != duplicate_requests_.end(); ++request_list) {
    for (auto iter = request_list->begin(); iter != request_list->end();) {
      // Remove any requests that have been destroyed.
      const auto& weak_request = (*iter);
      if (!weak_request) {
        iter = request_list->erase(iter);
        continue;
      }

      // The first valid request in the list will indicate whether all other
      // members are duplicate or not.
      if (weak_request->IsDuplicateOf(request)) {
        return request_list;
      }

      break;
    }
  }

  return duplicate_requests_.end();
}

PermissionRequestManager::WeakPermissionRequestList::iterator
PermissionRequestManager::VisitDuplicateRequests(
    DuplicateRequestVisitor visitor,
    PermissionRequest* request) {
  auto request_list = FindDuplicateRequestList(request);
  if (request_list == duplicate_requests_.end()) {
    return request_list;
  }

  for (auto iter = request_list->begin(); iter != request_list->end();) {
    if (auto& weak_request = (*iter)) {
      visitor.Run(weak_request);
      ++iter;
    } else {
      // Remove any requests that have been destroyed.
      iter = request_list->erase(iter);
    }
  }

  return request_list;
}

void PermissionRequestManager::PermissionGrantedIncludingDuplicates(
    PermissionRequest* request,
    bool is_one_time) {
  DCHECK_EQ(1ul, base::ranges::count(requests_, request) +
                     pending_permission_requests_.Count(request))
      << "Only requests in [pending_permission_]requests_ can have duplicates";
  request->PermissionGranted(is_one_time);
  VisitDuplicateRequests(
      base::BindRepeating(
          [](bool is_one_time,
             const base::WeakPtr<PermissionRequest>& weak_request) {
            weak_request->PermissionGranted(is_one_time);
          },
          is_one_time),
      request);
}

void PermissionRequestManager::PermissionDeniedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1ul, base::ranges::count(requests_, request) +
                     pending_permission_requests_.Count(request))
      << "Only requests in [pending_permission_]requests_ can have duplicates";
  request->PermissionDenied();
  VisitDuplicateRequests(
      base::BindRepeating(
          [](const base::WeakPtr<PermissionRequest>& weak_request) {
            weak_request->PermissionDenied();
          }),
      request);
}

void PermissionRequestManager::CancelledIncludingDuplicates(
    PermissionRequest* request,
    bool is_final_decision) {
  DCHECK_EQ(1ul, base::ranges::count(requests_, request) +
                     pending_permission_requests_.Count(request))
      << "Only requests in [pending_permission_]requests_ can have duplicates";
  request->Cancelled(is_final_decision);
  VisitDuplicateRequests(
      base::BindRepeating(
          [](bool is_final,
             const base::WeakPtr<PermissionRequest>& weak_request) {
            weak_request->Cancelled(is_final);
          },
          is_final_decision),
      request);
}

void PermissionRequestManager::RequestFinishedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(1ul, base::ranges::count(requests_, request) +
                     pending_permission_requests_.Count(request))
      << "Only requests in [pending_permission_]requests_ can have duplicates";
  auto duplicate_list = VisitDuplicateRequests(
      base::BindRepeating(
          [](const base::WeakPtr<PermissionRequest>& weak_request) {
            weak_request->RequestFinished();
          }),
      request);

  // Note: beyond this point, |request| has probably been deleted, any
  // dereference of |request| must be done prior this point.
  request->RequestFinished();

  // Additionally, we can now remove the duplicates.
  if (duplicate_list != duplicate_requests_.end()) {
    duplicate_requests_.erase(duplicate_list);
  }
}

void PermissionRequestManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionRequestManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool PermissionRequestManager::ShouldCurrentRequestUseQuietUI() const {
  // ContentSettingImageModel might call into this method if the user switches
  // between tabs while the |notification_permission_ui_selectors_| are
  // pending.
  return ReasonForUsingQuietUi() != absl::nullopt;
}

absl::optional<PermissionRequestManager::QuietUiReason>
PermissionRequestManager::ReasonForUsingQuietUi() const {
  if (!IsRequestInProgress() || !current_request_ui_to_use_ ||
      !current_request_ui_to_use_->quiet_ui_reason)
    return absl::nullopt;

  return *(current_request_ui_to_use_->quiet_ui_reason);
}

bool PermissionRequestManager::IsRequestInProgress() const {
  return !requests_.empty();
}

bool PermissionRequestManager::CanRestorePrompt() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return IsRequestInProgress() &&
         current_request_prompt_disposition_.has_value() && !view_;
#endif
}

void PermissionRequestManager::RestorePrompt() {
  if (CanRestorePrompt())
    ShowPrompt();
}

bool PermissionRequestManager::ShouldDropCurrentRequestIfCannotShowQuietly()
    const {
  absl::optional<QuietUiReason> quiet_ui_reason = ReasonForUsingQuietUi();
  if (quiet_ui_reason.has_value()) {
    switch (quiet_ui_reason.value()) {
      case QuietUiReason::kEnabledInPrefs:
      case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
      case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
      case QuietUiReason::kTriggeredByCrowdDeny:
        return false;
      case QuietUiReason::kTriggeredDueToAbusiveRequests:
      case QuietUiReason::kTriggeredDueToAbusiveContent:
      case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
        return true;
    }
  }

  return false;
}

void PermissionRequestManager::NotifyPromptAdded() {
  for (Observer& observer : observer_list_)
    observer.OnPromptAdded();
}

void PermissionRequestManager::NotifyPromptRemoved() {
  for (Observer& observer : observer_list_)
    observer.OnPromptRemoved();
}

void PermissionRequestManager::NotifyPromptRecreateFailed() {
  for (Observer& observer : observer_list_)
    observer.OnPromptRecreateViewFailed();
}

void PermissionRequestManager::NotifyPromptCreationFailedHiddenTab() {
  for (Observer& observer : observer_list_)
    observer.OnPromptCreationFailedHiddenTab();
}

void PermissionRequestManager::NotifyRequestDecided(
    permissions::PermissionAction permission_action) {
  for (Observer& observer : observer_list_)
    observer.OnRequestDecided(permission_action);
}

void PermissionRequestManager::StorePermissionActionForUMA(
    const GURL& origin,
    RequestType request_type,
    PermissionAction permission_action) {
  absl::optional<ContentSettingsType> content_settings_type =
      RequestTypeToContentSettingsType(request_type);
  if (content_settings_type.has_value()) {
    PermissionsClient::Get()
        ->GetOriginKeyedPermissionActionService(
            web_contents()->GetBrowserContext())
        ->RecordAction(PermissionUtil::GetLastCommittedOriginAsURL(
                           web_contents()->GetPrimaryMainFrame()),
                       content_settings_type.value(), permission_action);
  }
}

void PermissionRequestManager::OnPermissionUiSelectorDone(
    size_t selector_index,
    const UiDecision& decision) {
  if (decision.warning_reason) {
    switch (*(decision.warning_reason)) {
      case WarningReason::kAbusiveRequests:
        LogWarningToConsole(kAbusiveNotificationRequestsWarningMessage);
        break;
      case WarningReason::kAbusiveContent:
        LogWarningToConsole(kAbusiveNotificationContentWarningMessage);
        break;
      case WarningReason::kDisruptiveBehavior:
        break;
    }
  }

  // We have already made a decision because of a higher priority selector
  // therefore this selector's decision can be discarded.
  if (current_request_ui_to_use_.has_value())
    return;

  CHECK_LT(selector_index, selector_decisions_.size());
  selector_decisions_[selector_index] = decision;

  size_t decision_index = 0;
  while (decision_index < selector_decisions_.size() &&
         selector_decisions_[decision_index].has_value()) {
    const UiDecision& current_decision =
        selector_decisions_[decision_index].value();

    if (!prediction_grant_likelihood_.has_value()) {
      prediction_grant_likelihood_ = permission_ui_selectors_[decision_index]
                                         ->PredictedGrantLikelihoodForUKM();
    }

    if (!was_decision_held_back_.has_value()) {
      was_decision_held_back_ = permission_ui_selectors_[decision_index]
                                    ->WasSelectorDecisionHeldback();
    }

    if (current_decision.quiet_ui_reason.has_value()) {
      current_request_ui_to_use_ = current_decision;
      break;
    }

    ++decision_index;
  }

  // All decisions have been considered and none was conclusive.
  if (decision_index == selector_decisions_.size() &&
      !current_request_ui_to_use_.has_value()) {
    current_request_ui_to_use_ = UiDecision::UseNormalUiAndShowNoWarning();
  }

  if (current_request_ui_to_use_.has_value()) {
    ShowPrompt();
  }
}

PermissionPromptDisposition
PermissionRequestManager::DetermineCurrentRequestUIDisposition() {
  if (current_request_prompt_disposition_.has_value())
    return current_request_prompt_disposition_.value();
  return PermissionPromptDisposition::NONE_VISIBLE;
}

PermissionPromptDispositionReason
PermissionRequestManager::DetermineCurrentRequestUIDispositionReasonForUMA() {
  auto quiet_ui_reason = ReasonForUsingQuietUi();
  if (!quiet_ui_reason)
    return PermissionPromptDispositionReason::DEFAULT_FALLBACK;
  switch (*quiet_ui_reason) {
    case QuietUiReason::kEnabledInPrefs:
      return PermissionPromptDispositionReason::USER_PREFERENCE_IN_SETTINGS;
    case QuietUiReason::kTriggeredByCrowdDeny:
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
    case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
      return PermissionPromptDispositionReason::SAFE_BROWSING_VERDICT;
    case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
      return PermissionPromptDispositionReason::PREDICTION_SERVICE;
    case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
      return PermissionPromptDispositionReason::ON_DEVICE_PREDICTION_MODEL;
  }
}

void PermissionRequestManager::LogWarningToConsole(const char* message) {
  web_contents()->GetPrimaryMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning, message);
}

void PermissionRequestManager::DoAutoResponseForTesting() {
  switch (auto_response_for_test_) {
    case ACCEPT_ONCE:
      AcceptThisTime();
      break;
    case ACCEPT_ALL:
      Accept();
      break;
    case DENY_ALL:
      Deny();
      break;
    case DISMISS:
      Dismiss();
      break;
    case NONE:
      NOTREACHED();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PermissionRequestManager);

}  // namespace permissions
