// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/preview_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

PreviewPageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (started_in_foreground) {
    last_time_shown_ = navigation_handle->NavigationStart();
  }
  currently_in_foreground_ = started_in_foreground;
  CheckPageTransitionType(navigation_handle);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Track only outermost pages.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  CheckPageTransitionType(navigation_handle);
  return CONTINUE_OBSERVING;
}

PreviewPageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordMetrics();
  return STOP_OBSERVING;
}

PreviewPageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (currently_in_foreground_ && !last_time_shown_.is_null()) {
    total_foreground_duration_ += base::TimeTicks::Now() - last_time_shown_;
  }
  currently_in_foreground_ = false;
  return CONTINUE_OBSERVING;
}

PreviewPageLoadMetricsObserver::ObservePolicy
PreviewPageLoadMetricsObserver::OnShown() {
  currently_in_foreground_ = true;
  last_time_shown_ = base::TimeTicks::Now();
  return CONTINUE_OBSERVING;
}

void PreviewPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordMetrics();
}

PreviewPageLoadMetricsObserver::PageVisitType
PreviewPageLoadMetricsObserver::RecordPageVisitType() {
  PageVisitType type;
  const page_load_metrics::PageLoadMetricsObserverDelegate& delegate =
      GetDelegate();

  if (is_history_navigation_) {
    type = PageVisitType::kHistoryVisit;
  } else if (delegate.IsOriginVisit()) {
    if (is_first_navigation_) {
      // PageTransition::PAGE_TRANSITION_FIRST is set only for the first
      // navigation that is not triggered by any UI. So, this means the visit
      // is made via the open-in-new-tab feature.
      // PageTransition::PAGE_TRANSITION_LINK would not be set for the case.
      type = delegate.IsTerminalVisit() ? PageVisitType::kIndependentLinkVisit
                                        : PageVisitType::kOriginLinkVisit;
    } else {
      type = delegate.IsTerminalVisit() ? PageVisitType::kIndependentUIVisit
                                        : PageVisitType::kOriginUIVisit;
    }
  } else {
    type = delegate.IsTerminalVisit() ? PageVisitType::kTerminalVisit
                                      : PageVisitType::kPassingVisit;
  }
  base::UmaHistogramEnumeration("PageLoad.Experimental.PageVisitType2", type);
  return type;
}

void PreviewPageLoadMetricsObserver::RecordMetrics() {
  PageVisitType page_visit_type = RecordPageVisitType();
  if (currently_in_foreground_ && !last_time_shown_.is_null()) {
    total_foreground_duration_ += base::TimeTicks::Now() - last_time_shown_;
  }
  PAGE_LOAD_LONG_HISTOGRAM(
      "PageLoad.Experimental.TotalForegroundDuration.AllVisit",
      total_foreground_duration_);
  switch (page_visit_type) {
    case PageVisitType::kObsoleteIndependentVisit:
    case PageVisitType::kObsoleteOriginVisit:
      NOTREACHED_NORETURN();
    case PageVisitType::kPassingVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.PassingVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kTerminalVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.TerminalVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kHistoryVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.HistoryVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kIndependentLinkVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.IndependentLinkVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kIndependentUIVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.IndependentUIVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kOriginLinkVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.OriginLinkVisit",
          total_foreground_duration_);
      break;
    case PageVisitType::kOriginUIVisit:
      PAGE_LOAD_LONG_HISTOGRAM(
          "PageLoad.Experimental.TotalForegroundDuration.OriginUIVisit",
          total_foreground_duration_);
      break;
  }
}

void PreviewPageLoadMetricsObserver::CheckPageTransitionType(
    content::NavigationHandle* navigation_handle) {
  is_history_navigation_ =
      navigation_handle->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK;
  if (ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_FIRST)) {
    is_first_navigation_ = true;
  }
}
