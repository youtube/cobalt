// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/highest_pmf_reporter.h"

#include <limits>
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_COBALT)
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "third_party/blink/public/common/features.h"
#endif

namespace blink {

namespace {

#if BUILDFLAG(IS_COBALT)
constexpr size_t kBaselineReportCount = 4;

constexpr std::array<const char*, kBaselineReportCount> kBaselineMetricNames = {
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.2to4min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.4to8min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.8to16min"};

constexpr std::array<base::TimeDelta, kBaselineReportCount>
    kBaselineTimeToReport = {base::Minutes(2), base::Minutes(4),
                             base::Minutes(8), base::Minutes(16)};
#else
constexpr size_t kMaxReportCount = 4;

constexpr std::array<const char*, kMaxReportCount> kHighestPmfMetricNames = {
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.2to4min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.4to8min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.8to16min"};

constexpr std::array<base::TimeDelta, kMaxReportCount> kTimeToReport = {
    base::Minutes(2), base::Minutes(4), base::Minutes(8), base::Minutes(16)};
#endif

}  // namespace

void HighestPmfReporter::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DEFINE_STATIC_LOCAL(HighestPmfReporter, reporter, (std::move(task_runner)));
  (void)reporter;
}

HighestPmfReporter::HighestPmfReporter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : HighestPmfReporter(std::move(task_runner),
                         base::DefaultTickClock::GetInstance()) {}

HighestPmfReporter::HighestPmfReporter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* clock)
    : task_runner_(std::move(task_runner)), clock_(clock) {
  MemoryUsageMonitor::Instance().AddObserver(this);

#if BUILDFLAG(IS_COBALT)
  bool use_baseline = true;
  if (base::FeatureList::IsEnabled(features::kHighestPmfReporterConfigurable)) {
    std::string intervals = features::kHighestPmfReporterIntervals.Get();
    std::string suffixes = features::kHighestPmfReporterMetricSuffixes.Get();

    std::vector<std::string> interval_strs = base::SplitString(
        intervals, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::vector<std::string> suffix_strs = base::SplitString(
        suffixes, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (interval_strs.size() == suffix_strs.size() && !interval_strs.empty()) {
      bool success = true;
      int previous_interval = -1;
      for (size_t i = 0; i < interval_strs.size(); ++i) {
        int interval_min;
        if (!base::StringToInt(interval_strs[i], &interval_min)) {
          success = false;
          break;
        }

        if (interval_min <= 0 || interval_min <= previous_interval) {
          success = false;
          break;
        }
        previous_interval = interval_min;

        time_to_report_.push_back(base::Minutes(interval_min));
        metric_names_.push_back(
            "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint." +
            suffix_strs[i]);
      }
      if (success) {
        use_baseline = false;
      } else {
        time_to_report_.clear();
        metric_names_.clear();
      }
    }
  }

  if (use_baseline) {
    for (size_t i = 0; i < kBaselineReportCount; ++i) {
      time_to_report_.push_back(kBaselineTimeToReport[i]);
      metric_names_.push_back(kBaselineMetricNames[i]);
    }
  }
#endif
}

bool HighestPmfReporter::FirstNavigationStarted() {
  if (first_navigation_detected_)
    return false;

  for (Page* page : Page::OrdinaryPages()) {
    Frame* frame = page->MainFrame();
    if (!frame)
      continue;

    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;

    DocumentLoader* loader = local_frame->Loader().GetDocumentLoader();
    if (!loader)
      continue;

    if (!loader->GetTiming().NavigationStart().is_null()) {
      first_navigation_detected_ = true;
      return true;
    }
  }
  return false;
}

void HighestPmfReporter::OnMemoryPing(MemoryUsage usage) {
  DCHECK(IsMainThread());
  if (FirstNavigationStarted()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&HighestPmfReporter::OnReportMetrics,
                      WTF::Unretained(this)),
#if BUILDFLAG(IS_COBALT)
        time_to_report_[0]);
#else
        kTimeToReport[0]);
#endif
  }

  if (current_highest_pmf_ > usage.private_footprint_bytes)
    return;

  current_highest_pmf_ = usage.private_footprint_bytes;
  peak_resident_bytes_at_current_highest_pmf_ = usage.peak_resident_bytes;
#if BUILDFLAG(IS_COBALT)
  webpage_counts_at_current_highest_pmf_ = 1;
#else
  webpage_counts_at_current_highest_pmf_ = Page::OrdinaryPages().size();
#endif

  // TODO(tasak): Report the highest memory footprint throughout renderer's
  // lifetime.
}

void HighestPmfReporter::OnReportMetrics() {
  DCHECK(IsMainThread());
  ReportMetrics();

  // The following code is not accurate, because OnReportMetrics will be late
  // when renderer is slow (e.g. caused by near-OOM or heavy tasks is running
  // or ...). However such signal getting late by minutes is unlikely, so it's
  // ok to say "this is good enough".
  current_highest_pmf_ = 0.0;
  peak_resident_bytes_at_current_highest_pmf_ = 0.0;
  webpage_counts_at_current_highest_pmf_ = 0;
  report_count_++;
#if BUILDFLAG(IS_COBALT)
  if (report_count_ >= time_to_report_.size()) {
#else
  if (report_count_ >= kMaxReportCount) {
#endif
    // Stop observing the MemoryUsageMonitor once there's no more histogram to
    // report.
    MemoryUsageMonitor::Instance().RemoveObserver(this);
    return;
  }

#if BUILDFLAG(IS_COBALT)
  const base::TimeDelta delay =
      time_to_report_[report_count_] - time_to_report_[report_count_ - 1];
#else
  const base::TimeDelta delay =
      kTimeToReport[report_count_] - kTimeToReport[report_count_ - 1];
#endif
  task_runner_->PostDelayedTask(
      FROM_HERE,
      WTF::BindOnce(&HighestPmfReporter::OnReportMetrics,
                    WTF::Unretained(this)),
      delay);
}

void HighestPmfReporter::ReportMetrics() {
#if BUILDFLAG(IS_COBALT)
  base::UmaHistogramMemoryMB(metric_names_[report_count_],
                             base::saturated_cast<base::Histogram::Sample32>(
                                 current_highest_pmf_ / 1024 / 1024));
#else
  base::UmaHistogramMemoryMB(kHighestPmfMetricNames[report_count_],
                             base::saturated_cast<base::Histogram::Sample32>(
                                 current_highest_pmf_ / 1024 / 1024));
#endif
}

}  // namespace blink
