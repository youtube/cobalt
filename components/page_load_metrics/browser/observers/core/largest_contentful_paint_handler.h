// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_

#include <map>

#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "url/gurl.h"

namespace content {

class NavigationHandle;
class RenderFrameHost;

}  // namespace content

namespace page_load_metrics {

class ContentfulPaintTimingInfo {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LargestContentTextOrImage {
    kImage = 0,
    kText = 1,
    kMaxValue = kText,
  };

  ContentfulPaintTimingInfo(LargestContentTextOrImage largest_content_type,
                            bool in_main_frame,
                            blink::LargestContentfulPaintType type);
  ContentfulPaintTimingInfo(
      const absl::optional<base::TimeDelta>&,
      const uint64_t& size,
      const LargestContentTextOrImage largest_content_type,
      double image_bpp,
      const absl::optional<net::RequestPriority>& image_request_priority,
      bool in_main_frame,
      blink::LargestContentfulPaintType type);
  ContentfulPaintTimingInfo(const ContentfulPaintTimingInfo& other);
  void Reset(const absl::optional<base::TimeDelta>& time,
             const uint64_t& size,
             blink::LargestContentfulPaintType type,
             double image_bpp,
             const absl::optional<net::RequestPriority>& image_request_priority,
             const absl::optional<base::TimeDelta>& image_load_start,
             const absl::optional<base::TimeDelta>& image_load_end);
  absl::optional<base::TimeDelta> Time() const { return time_; }
  absl::optional<base::TimeDelta> ImageLoadStart() const {
    return image_load_start_;
  }
  absl::optional<base::TimeDelta> ImageLoadEnd() const {
    return image_load_end_;
  }
  bool InMainFrame() const { return in_main_frame_; }
  blink::LargestContentfulPaintType Type() const { return type_; }
  uint64_t Size() const { return size_; }
  LargestContentTextOrImage TextOrImage() const { return text_or_image_; }
  double ImageBPP() const { return image_bpp_; }
  absl::optional<net::RequestPriority> ImageRequestPriority() const {
    return image_request_priority_;
  }

  // Returns true iff this object does not represent any paint.
  bool Empty() const {
    // size_ and time_ should both be set or both be unset.
    DCHECK((size_ != 0u && time_) || (size_ == 0u && !time_));
    return !time_;
  }

  // Returns true iff this object does not represent any paint OR represents an
  // image that has not finished loading.
  bool ContainsValidTime() const {
    return time_ && *time_ != base::TimeDelta();
  }

  std::unique_ptr<base::trace_event::TracedValue> DataAsTraceValue() const;

  ContentfulPaintTimingInfo() = delete;

 private:
  std::string TextOrImageInString() const;
  absl::optional<base::TimeDelta> time_;
  uint64_t size_;
  LargestContentTextOrImage text_or_image_;
  blink::LargestContentfulPaintType type_ =
      blink::LargestContentfulPaintType::kNone;
  double image_bpp_ = 0.0;
  absl::optional<net::RequestPriority> image_request_priority_;
  bool in_main_frame_;
  absl::optional<base::TimeDelta> image_load_start_;
  absl::optional<base::TimeDelta> image_load_end_;
};

class ContentfulPaint {
 public:
  explicit ContentfulPaint(bool in_main_frame,
                           blink::LargestContentfulPaintType type);
  ContentfulPaintTimingInfo& Text() { return text_; }
  const ContentfulPaintTimingInfo& Text() const { return text_; }
  ContentfulPaintTimingInfo& Image() { return image_; }
  const ContentfulPaintTimingInfo& Image() const { return image_; }
  const ContentfulPaintTimingInfo& MergeTextAndImageTiming() const;

 private:
  ContentfulPaintTimingInfo text_;
  ContentfulPaintTimingInfo image_;
};

class LargestContentfulPaintHandler {
 public:
  using FrameTreeNodeId = int;
  static void SetTestMode(bool enabled);
  LargestContentfulPaintHandler();

  LargestContentfulPaintHandler(const LargestContentfulPaintHandler&) = delete;
  LargestContentfulPaintHandler& operator=(
      const LargestContentfulPaintHandler&) = delete;

  ~LargestContentfulPaintHandler();

  // Returns true if the out parameters are assigned values.
  static bool AssignTimeAndSizeForLargestContentfulPaint(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      absl::optional<base::TimeDelta>* largest_content_paint_time,
      uint64_t* largest_content_paint_size,
      ContentfulPaintTimingInfo::LargestContentTextOrImage*
          largest_content_type);

  void RecordMainFrameTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const absl::optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp);
  void RecordSubFrameTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const absl::optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp,
      content::RenderFrameHost* subframe_rfh,
      const GURL& main_frame_url);
  inline void RecordMainFrameTreeNodeId(int main_frame_tree_node_id) {
    main_frame_tree_node_id_.emplace(main_frame_tree_node_id);
  }

  inline int MainFrameTreeNodeId() const {
    return main_frame_tree_node_id_.value();
  }

  // We merge the candidates from text side and image side to get the largest
  // candidate across both types of content.
  const ContentfulPaintTimingInfo& MainFrameLargestContentfulPaint() const {
    return main_frame_contentful_paint_.MergeTextAndImageTiming();
  }
  const ContentfulPaintTimingInfo& CrossSiteSubframesLargestContentfulPaint()
      const {
    return cross_site_subframe_contentful_paint_.MergeTextAndImageTiming();
  }

  // We merge the candidates from main frame and subframe to get the largest
  // candidate across all frames.
  const ContentfulPaintTimingInfo& MergeMainFrameAndSubframes() const;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle,
      base::TimeTicks navigation_start);
  void OnSubFrameDeleted(int frame_tree_node_id);

 private:
  void RecordSubFrameTimingInternal(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const absl::optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp,
      const base::TimeDelta& navigation_start_offset);
  // RecordCrossSiteSubframeTiming updates
  // `cross_site_subframe_contentful_paint_` with a new lcp candidate.
  void RecordCrossSiteSubframeTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const base::TimeDelta& navigation_start_offset);
  void UpdateFirstInputOrScrollNotified(
      const absl::optional<base::TimeDelta>& candidate_new_time,
      const base::TimeDelta& navigation_start_offset);
  bool IsValid(const absl::optional<base::TimeDelta>& time) {
    // When |time| is not present, this means that there is no current
    // candidate. If |time| is 0, it corresponds to an image that has not
    // finished loading. In both cases, we do not know the timestamp at which
    // |time| was determned. Therefore, we just assume that the time is valid
    // only if we have not yet received a notification for first input or scroll
    if (!time.has_value() || (*time).is_zero())
      return first_input_or_scroll_notified_ == base::TimeDelta::Max();
    return *time < first_input_or_scroll_notified_;
  }
  ContentfulPaint main_frame_contentful_paint_;
  ContentfulPaint subframe_contentful_paint_;
  // `cross_site_subframe_contentful_paint_` keeps track of the most plausible
  // LCP candidate computed from the cross-site subframes.
  ContentfulPaint cross_site_subframe_contentful_paint_;

  // Used for Telemetry to distinguish the LCP events from different
  // navigations.
  absl::optional<int> main_frame_tree_node_id_;

  // The first input or scroll across all frames in the page. Used to filter out
  // paints that occur on other frames but after this time.
  base::TimeDelta first_input_or_scroll_notified_ = base::TimeDelta::Max();

  // Navigation start offsets for the most recently committed document in each
  // frame.
  std::map<FrameTreeNodeId, base::TimeDelta> subframe_navigation_start_offset_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
