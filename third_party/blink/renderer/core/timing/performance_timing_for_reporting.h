// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMING_FOR_REPORTING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMING_FOR_REPORTING_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

class DocumentLoadTiming;
class DocumentLoader;
class DocumentParserTiming;
class DocumentTiming;
class InteractiveDetector;
class PaintTiming;
class PaintTimingDetector;

// This class is only used for non-web-exposed reporting purposes (e.g. UKM).
class CORE_EXPORT PerformanceTimingForReporting final
    : public GarbageCollected<PerformanceTimingForReporting>,
      public ExecutionContextClient {
 public:
  struct BackForwardCacheRestoreTiming {
    uint64_t navigation_start;
    uint64_t first_paint;
    std::array<uint64_t,
               WebPerformanceMetricsForReporting::
                   kRequestAnimationFramesToRecordAfterBackForwardCacheRestore>
        request_animation_frames;
    absl::optional<base::TimeDelta> first_input_delay;
  };

  using BackForwardCacheRestoreTimings =
      WTF::Vector<BackForwardCacheRestoreTiming>;

  explicit PerformanceTimingForReporting(ExecutionContext*);

  // These getters are for non-spec timings and used for metrics reporting
  // purposes. They are not to be exposed to JavaScript.

  uint64_t inputStart() const;

  // The time immediately after the user agent finishes prompting to unload the
  // previous document, or if there is no previous document, the same value as
  // fetchStart.  Intended to be used for correlation with other events internal
  // to blink.
  base::TimeTicks NavigationStartAsMonotonicTime() const;

  // The timings after the page is restored from back-forward cache.
  BackForwardCacheRestoreTimings BackForwardCacheRestore() const;

  // The time the first paint operation was performed.
  uint64_t FirstPaintForMetrics() const;

  // The time the first paint operation for image was performed.
  uint64_t FirstImagePaint() const;

  // The first 'contentful' paint as full-resolution monotonic time. This is
  // the point at which blink painted the content for FCP; actual FCP is
  // recorded as the time the generated content makes it to the screen (also
  // known as presentation time). Intended to be used for correlation with other
  // events internal to blink.
  base::TimeTicks FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime()
      const;

  // The time of the first 'contentful' paint. A contentful paint is a paint
  // that includes content of some kind (for example, text or image content).
  uint64_t FirstContentfulPaintIgnoringSoftNavigations() const;

  // The first 'contentful' paint as full-resolution monotonic time. Intended to
  // be used for correlation with other events internal to blink.
  base::TimeTicks FirstContentfulPaintAsMonotonicTimeForMetrics() const;

  // The time of the first 'meaningful' paint, A meaningful paint is a paint
  // where the page's primary content is visible.
  uint64_t FirstMeaningfulPaint() const;

  // The time of the candidate of first 'meaningful' paint, A meaningful paint
  // candidate indicates the first time we considered a paint to qualify as the
  // potential first meaningful paint. But, be careful that it may be an
  // optimistic (i.e., too early) estimate.
  // TODO(crbug.com/848639): This function is exposed as an experiment, and if
  // not useful, this function can be removed.
  uint64_t FirstMeaningfulPaintCandidate() const;

  // Largest Image Paint is the first paint after the largest image within
  // viewport being fully loaded. LargestImagePaint and LargestImagePaintSize
  // are the time and size of it.
  uint64_t LargestImagePaintForMetrics() const;
  uint64_t LargestImagePaintSizeForMetrics() const;
  blink::LargestContentfulPaintType LargestContentfulPaintTypeForMetrics()
      const;
  absl::optional<WebURLRequest::Priority>
  LargestContentfulPaintImageRequestPriorityForMetrics() const;

  // The timestamp when the LCP starts loading. This method is called only when
  // the LCP corresponds to media elements (image or video). For videos, the
  // values are 0 and won't be reported to UKM. Effectively, only LCP
  // corresponding to images have the load start/end reported to UKM.
  absl::optional<base::TimeDelta> LargestContentfulPaintImageLoadStart() const;

  // The timestamp when the LCP element completes loading. This method works in
  // the same way as the above LargestContentfulPaintImageLoadStart.
  absl::optional<base::TimeDelta> LargestContentfulPaintImageLoadEnd() const;

  // The time of the first paint of the largest text within viewport.
  // Largest Text Paint is the first paint after the largest text within
  // viewport being painted. LargestTextPaint and LargestTextPaintSize
  // are the time and size of it.
  double LargestContentfulPaintImageBPPForMetrics() const;
  uint64_t LargestTextPaintForMetrics() const;
  uint64_t LargestTextPaintSizeForMetrics() const;

  // Largest Contentful Paint is the either the largest text paint time or the
  // largest image paint time, whichever has the larger size.
  base::TimeTicks LargestContentfulPaintAsMonotonicTimeForMetrics() const;

  // The time at which the frame is first eligible for painting due to not
  // being throttled. A zero value indicates throttling.
  uint64_t FirstEligibleToPaint() const;

  // The time at which we are notified of the first input or scroll event which
  // causes the largest contentful paint algorithm to stop.
  uint64_t FirstInputOrScrollNotifiedTimestamp() const;

  // The duration between the hardware timestamp and being queued on the main
  // thread for the first click, tap, key press, cancellable touchstart, or
  // pointer down followed by a pointer up.
  absl::optional<base::TimeDelta> FirstInputDelay() const;

  // The timestamp of the event whose delay is reported by FirstInputDelay().
  absl::optional<base::TimeDelta> FirstInputTimestamp() const;

  // The timestamp of the event whose delay is reported by FirstInputDelay().
  // Intended to be used for correlation with other events internal to blink.
  absl::optional<base::TimeTicks> FirstInputTimestampAsMonotonicTime() const;

  // The longest duration between the hardware timestamp and being queued on the
  // main thread for the click, tap, key press, cancellable touchstart, or
  // pointer down followed by a pointer up.
  absl::optional<base::TimeDelta> LongestInputDelay() const;

  // The timestamp of the event whose delay is reported by LongestInputDelay().
  absl::optional<base::TimeDelta> LongestInputTimestamp() const;

  // The duration of event handlers processing the first input event.
  absl::optional<base::TimeDelta> FirstInputProcessingTime() const;

  // The duration between the user's first scroll and display update.
  absl::optional<base::TimeDelta> FirstScrollDelay() const;

  // The hardware timestamp of the first scroll.
  absl::optional<base::TimeDelta> FirstScrollTimestamp() const;

  // TimeTicks for unload start and end.
  absl::optional<base::TimeTicks> UnloadStart() const;
  absl::optional<base::TimeTicks> UnloadEnd() const;

  // The timestamp of when the commit navigation finished in the frame loader.
  absl::optional<base::TimeTicks> CommitNavigationEnd() const;

  // The timestamp of the user timing mark 'mark_fully_loaded', if
  // available.
  absl::optional<base::TimeDelta> UserTimingMarkFullyLoaded() const;

  // The timestamp of the user timing mark 'mark_fully_visible', if
  // available.
  absl::optional<base::TimeDelta> UserTimingMarkFullyVisible() const;

  // The timestamp of the user timing mark 'mark_interactive', if
  // available.
  absl::optional<base::TimeDelta> UserTimingMarkInteractive() const;

  uint64_t ParseStart() const;
  uint64_t ParseStop() const;
  uint64_t ParseBlockedOnScriptLoadDuration() const;
  uint64_t ParseBlockedOnScriptLoadFromDocumentWriteDuration() const;
  uint64_t ParseBlockedOnScriptExecutionDuration() const;
  uint64_t ParseBlockedOnScriptExecutionFromDocumentWriteDuration() const;

  // The time of the first paint after a portal activation.
  absl::optional<base::TimeTicks> LastPortalActivatedPaint() const;

  // The start time of the prerender activation navigation.
  absl::optional<base::TimeDelta> PrerenderActivationStart() const;

  void Trace(Visitor*) const override;

  uint64_t MonotonicTimeToIntegerMilliseconds(base::TimeTicks) const;

  std::unique_ptr<TracedValue> GetNavigationTracingData();

 private:
  const DocumentTiming* GetDocumentTiming() const;
  const DocumentParserTiming* GetDocumentParserTiming() const;
  const PaintTiming* GetPaintTiming() const;
  PaintTimingDetector* GetPaintTimingDetector() const;
  DocumentLoader* GetDocumentLoader() const;
  DocumentLoadTiming* GetDocumentLoadTiming() const;
  InteractiveDetector* GetInteractiveDetector() const;
  absl::optional<base::TimeDelta> MonotonicTimeToPseudoWallTime(
      const absl::optional<base::TimeTicks>&) const;

  bool cross_origin_isolated_capability_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMING_FOR_REPORTING_H_
