// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/declarative_performance_observer_coordinator.h"

#include "content/browser/network/declarative_performance_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

DeclarativePerformanceObserverCoordinator::
    DeclarativePerformanceObserverCoordinator(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<DeclarativePerformanceObserverCoordinator>(
          *web_contents) {}

DeclarativePerformanceObserverCoordinator::
    ~DeclarativePerformanceObserverCoordinator() = default;

void DeclarativePerformanceObserverCoordinator::OnVisibilityChanged(
    Visibility visibility) {
  auto* observer = DeclarativePerformanceObserver::GetForCurrentDocument(
      web_contents()->GetPrimaryMainFrame());
  if (observer) {
    observer->OnVisibilityChanged(visibility);
  }
}

void DeclarativePerformanceObserverCoordinator::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  auto* observer =
      DeclarativePerformanceObserver::GetForCurrentDocument(render_frame_host);
  if (observer) {
    observer->OnFrameDeleted();
  }
}

void DeclarativePerformanceObserverCoordinator::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (old_state == RenderFrameHost::LifecycleState::kActive &&
      new_state == RenderFrameHost::LifecycleState::kInBackForwardCache) {
    auto* observer = DeclarativePerformanceObserver::GetForCurrentDocument(
        render_frame_host);
    if (observer) {
      observer->OnEnterBFCache();
    }
  }
}

void DeclarativePerformanceObserverCoordinator::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted()) {
    return;
  }
  if (navigation_handle->IsServedFromBackForwardCache()) {
    auto* observer = DeclarativePerformanceObserver::GetForCurrentDocument(
        navigation_handle->GetRenderFrameHost());
    if (observer) {
      observer->OnDidFinishNavigation(navigation_handle);
    }
  } else if (navigation_handle->IsPrerenderedPageActivation()) {
    auto* observer = DeclarativePerformanceObserver::GetForCurrentDocument(
        navigation_handle->GetRenderFrameHost());
    if (observer) {
      observer->OnPrerenderActivation(navigation_handle);
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DeclarativePerformanceObserverCoordinator);

}  // namespace content
