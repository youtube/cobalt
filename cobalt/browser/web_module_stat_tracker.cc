// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/web_module_stat_tracker.h"

#if defined(ENABLE_WEBDRIVER)
#include <sstream>
#endif  // ENABLE_WEBDRIVER

#include "base/stringprintf.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/event.h"
#if defined(ENABLE_WEBDRIVER)
#include "cobalt/dom/global_stats.h"
#endif  // ENABLE_WEBDRIVER

namespace cobalt {
namespace browser {

WebModuleStatTracker::WebModuleStatTracker(const std::string& name,
                                           bool should_track_event_stats)
    : name_(name),
      should_track_event_stats_(should_track_event_stats),
      dom_stat_tracker_(new dom::DomStatTracker(name)),
      layout_stat_tracker_(new layout::LayoutStatTracker(name)),
      event_is_processing_(StringPrintf("Event.%s.IsProcessing", name.c_str()),
                           false, "Nonzero when an event is being processed."),
      current_event_type_(kEventTypeInvalid),
      current_event_dispatched_event_(nullptr) {
  if (should_track_event_stats_) {
    event_stats_list_.reserve(kNumEventTypes);
    for (int i = 0; i < kNumEventTypes; ++i) {
      EventType event_type = static_cast<EventType>(i);
      event_stats_list_.push_back(new EventStats(StringPrintf(
          "%s.%s", name.c_str(), GetEventTypeName(event_type).c_str())));
    }
  }

  stop_watches_.reserve(kNumStopWatchTypes);
  for (int i = 0; i < kNumStopWatchTypes; ++i) {
    stop_watches_.push_back(
        base::StopWatch(i, base::StopWatch::kAutoStartOff, this));
  }
  stop_watch_durations_.resize(kNumStopWatchTypes, base::TimeDelta());
}

void WebModuleStatTracker::OnStartDispatchEvent(
    const scoped_refptr<dom::Event>& event) {
  if (!should_track_event_stats_) {
    return;
  }

  // If an event is already being tracked, then don't track this event. It needs
  // to be allowed to finish.
  if (current_event_type_ != kEventTypeInvalid) {
    return;
  }

  // Determine the event type.
  if (event->type() == base::Tokens::keydown()) {
    current_event_type_ = kEventTypeKeyDown;
  } else if (event->type() == base::Tokens::keyup()) {
    current_event_type_ = kEventTypeKeyUp;
  } else if (event->type() == base::Tokens::pointerdown()) {
    current_event_type_ = kEventTypePointerDown;
  } else if (event->type() == base::Tokens::pointerup()) {
    current_event_type_ = kEventTypePointerUp;
  } else {
    current_event_type_ = kEventTypeInvalid;
  }

  // If this is a valid event type, then start tracking it.
  if (current_event_type_ != kEventTypeInvalid) {
    DCHECK(!event_is_processing_);

    event_is_processing_ = true;
    current_event_dispatched_event_ = event;
    current_event_start_time_ = base::TimeTicks::Now();
    current_event_render_tree_produced_time_ = base::TimeTicks();

    dom_stat_tracker_->StartTrackingEvent();
    layout_stat_tracker_->StartTrackingEvent();

    stop_watch_durations_[kStopWatchTypeDispatchEvent] = base::TimeDelta();
    stop_watches_[kStopWatchTypeDispatchEvent].Start();
  }
}

void WebModuleStatTracker::OnStopDispatchEvent(
    const scoped_refptr<dom::Event>& event,
    bool are_animation_frame_callbacks_pending,
    bool is_new_render_tree_pending) {
  // Verify that this dispatched event is the one currently being tracked.
  if (event != current_event_dispatched_event_) {
    return;
  }

  current_event_dispatched_event_ = nullptr;
  stop_watches_[kStopWatchTypeDispatchEvent].Stop();

  if (!are_animation_frame_callbacks_pending && !is_new_render_tree_pending &&
      current_event_render_tree_produced_time_.is_null()) {
    EndCurrentEvent(base::TimeTicks::Now());
  }
}

void WebModuleStatTracker::OnRanAnimationFrameCallbacks(
    bool is_new_render_tree_pending) {
  if (current_event_type_ == kEventTypeInvalid) {
    return;
  }

  if (!is_new_render_tree_pending &&
      current_event_render_tree_produced_time_.is_null()) {
    EndCurrentEvent(base::TimeTicks::Now());
  }
}

void WebModuleStatTracker::OnRenderTreeProduced(
    const base::TimeTicks& produced_time) {
  // Flush the periodic tracking regardless of whether or not there is a current
  // event. Periodic tracking is not tied to events.
  dom_stat_tracker_->FlushPeriodicTracking();
  layout_stat_tracker_->FlushPeriodicTracking();

  if (current_event_type_ == kEventTypeInvalid) {
    return;
  }

  // Event tracking stops when the first render tree being produced. At that
  // point, processing switches to the rasterizer thread and any subsequent
  // dom/layout work that occurs will not be associated with the event's first
  // render tree.
  if (current_event_render_tree_produced_time_.is_null()) {
    current_event_render_tree_produced_time_ = produced_time;
    dom_stat_tracker_->StopTrackingEvent();
    layout_stat_tracker_->StopTrackingEvent();
  }
}

void WebModuleStatTracker::OnRenderTreeRasterized(
    const base::TimeTicks& produced_time,
    const base::TimeTicks& rasterized_time) {
  if (current_event_type_ == kEventTypeInvalid) {
    return;
  }

  // End the event if the event's render tree has already been produced and
  // the rasterized render tree is not older than the event's render tree.
  if (!current_event_render_tree_produced_time_.is_null() &&
      produced_time >= current_event_render_tree_produced_time_) {
    EndCurrentEvent(rasterized_time);
  }
}

WebModuleStatTracker::EventStats::EventStats(const std::string& name)
    : produced_render_tree(
          StringPrintf("Event.%s.ProducedRenderTree", name.c_str()), false,
          "Nonzero when the event produced a render tree."),
      count_dom_html_element(
          StringPrintf("Event.Count.%s.DOM.HtmlElement", name.c_str()), 0,
          "Total number of HTML elements."),
      count_dom_html_element_created(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Created", name.c_str()),
          0, "Total number of HTML elements created."),
      count_dom_html_element_destroyed(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Destroyed",
                       name.c_str()),
          0, "Total number of HTML elements destroyed."),
      count_dom_html_element_document(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Document", name.c_str()),
          0, "Number of HTML elements in document."),
      count_dom_html_element_document_added(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Document.Added",
                       name.c_str()),
          0, "Number of HTML elements added to document."),
      count_dom_html_element_document_removed(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Document.Removed",
                       name.c_str()),
          0, "Number of HTML elements removed from document."),
      count_dom_update_matching_rules(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.UpdateMatchingRules",
                       name.c_str()),
          0, "Number of HTML elements that had their matching rules updated."),
      count_dom_update_computed_style(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.UpdateComputedStyle",
                       name.c_str()),
          0, "Number of HTML elements that had their computed style updated."),
      count_dom_generate_html_element_computed_style(
          StringPrintf(
              "Event.Count.%s.DOM.HtmlElement.GenerateHtmlElementComputedStyle",
              name.c_str()),
          0,
          "Number of HTML elements that had their computed style generated."),
      count_dom_generate_pseudo_element_computed_style(
          StringPrintf("Event.Count.%s.DOM.HtmlElement."
                       "GeneratePseudoElementComputedStyle",
                       name.c_str()),
          0,
          "Number of pseudo elements that had their computed style generated."),
      count_layout_box(StringPrintf("Event.Count.%s.Layout.Box", name.c_str()),
                       0, "Number of layout boxes."),
      count_layout_box_created(
          StringPrintf("Event.Count.%s.Layout.Box.Created", name.c_str()), 0,
          "Number of layout boxes created."),
      count_layout_box_destroyed(
          StringPrintf("Event.Count.%s.Layout.Box.Destroyed", name.c_str()), 0,
          "Number of layout boxes destroyed."),
      count_layout_update_size(
          StringPrintf("Event.Count.%s.Layout.Box.UpdateSize", name.c_str()), 0,
          "Number of layout boxes that had their size updated."),
      count_layout_render_and_animate(
          StringPrintf("Event.Count.%s.Layout.Box.RenderAndAnimate",
                       name.c_str()),
          0, "Number of layout boxes that had their render tree node updated."),
      count_layout_update_cross_references(
          StringPrintf("Event.Count.%s.Layout.Box.UpdateCrossReferences",
                       name.c_str()),
          0, "Number of layout boxes that had their cross references updated."),
      duration_total(StringPrintf("Event.Duration.%s", name.c_str()),
                     base::TimeDelta(),
                     "Total duration of the event (in microseconds). This is "
                     "the time elapsed from the event dispatch until the "
                     "render tree is produced."),
      duration_dom_dispatch_event(
          StringPrintf("Event.Duration.%s.DOM.DispatchEvent", name.c_str()),
          base::TimeDelta(),
          "Dispatch duration, which includes JS, for event (in "
          "microseconds). This does not include subsequent DOM and Layout "
          "processing."),
      duration_dom_run_animation_frame_callbacks(
          StringPrintf("Event.Duration.%s.DOM.RunAnimationFrameCallbacks",
                       name.c_str()),
          base::TimeDelta(),
          "Run animation frame callbacks duration for event (in "
          "microseconds)."),
      duration_dom_update_computed_style(
          StringPrintf("Event.Duration.%s.DOM.UpdateComputedStyle",
                       name.c_str()),
          base::TimeDelta(),
          "UpdateComputedStyle duration for event (in microseconds)."),
      duration_layout_box_tree(
          StringPrintf("Event.Duration.%s.Layout.BoxTree", name.c_str()),
          base::TimeDelta(),
          "Layout box tree duration for event (in microseconds)."),
      duration_layout_box_generation(
          StringPrintf("Event.Duration.%s.Layout.BoxTree.BoxGeneration",
                       name.c_str()),
          base::TimeDelta(),
          "BoxGeneration duration for event (in microseconds)."),
      duration_layout_update_used_sizes(
          StringPrintf("Event.Duration.%s.Layout.BoxTree.UpdateUsedSizes",
                       name.c_str()),
          base::TimeDelta(),
          "UpdateUsedSizes duration for event (in microseconds)."),
      duration_layout_render_and_animate(
          StringPrintf("Event.Duration.%s.Layout.RenderAndAnimate",
                       name.c_str()),
          base::TimeDelta(),
          "RenderAndAnimate duration for event (in microseconds)."),
      duration_renderer_rasterize(
          StringPrintf("Event.Duration.%s.Renderer.Rasterize", name.c_str()),
          base::TimeDelta(), "Rasterize duration for event (in microseconds).")
#if defined(ENABLE_WEBDRIVER)
      ,
      value_dictionary(
          StringPrintf("Event.%s.ValueDictionary", name.c_str()), "{}",
          "All event values represented as a dictionary in a string.")
#endif  // ENABLE_WEBDRIVER
{
}

bool WebModuleStatTracker::IsStopWatchEnabled(int /*id*/) const { return true; }

void WebModuleStatTracker::OnStopWatchStopped(int id,
                                              base::TimeDelta time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

void WebModuleStatTracker::EndCurrentEvent(base::TimeTicks event_end_time) {
  if (current_event_type_ == kEventTypeInvalid) {
    return;
  }

  DCHECK(event_is_processing_);
  DCHECK(!current_event_start_time_.is_null());

  // If no render tree was produced by this event, then tracking stops at the
  // end of the event; otherwise, it already stopped when the render tree was
  // produced.
  if (current_event_render_tree_produced_time_.is_null()) {
    dom_stat_tracker_->StopTrackingEvent();
    layout_stat_tracker_->StopTrackingEvent();
  }

  // If a render tree was produced by this event, then the event is ending with
  // the render tree's rasterization; otherwise, there was no rasterization.
  base::TimeDelta renderer_rasterize_duration =
      !current_event_render_tree_produced_time_.is_null()
          ? event_end_time - current_event_render_tree_produced_time_
          : base::TimeDelta();

  EventStats* event_stats = event_stats_list_[current_event_type_];
  event_stats->produced_render_tree =
      !current_event_render_tree_produced_time_.is_null();

  // Update event counts
  event_stats->count_dom_html_element =
      dom_stat_tracker_->EventCountHtmlElement();
  event_stats->count_dom_html_element_created =
      dom_stat_tracker_->event_count_html_element_created();
  event_stats->count_dom_html_element_destroyed =
      dom_stat_tracker_->event_count_html_element_destroyed();
  event_stats->count_dom_html_element_document =
      dom_stat_tracker_->EventCountHtmlElementDocument();
  event_stats->count_dom_html_element_document_added =
      dom_stat_tracker_->event_count_html_element_document_added();
  event_stats->count_dom_html_element_document_removed =
      dom_stat_tracker_->event_count_html_element_document_removed();
  event_stats->count_dom_update_matching_rules =
      dom_stat_tracker_->event_count_update_matching_rules();
  event_stats->count_dom_update_computed_style =
      dom_stat_tracker_->event_count_update_computed_style();
  event_stats->count_dom_generate_html_element_computed_style =
      dom_stat_tracker_->event_count_generate_html_element_computed_style();
  event_stats->count_dom_generate_pseudo_element_computed_style =
      dom_stat_tracker_->event_count_generate_pseudo_element_computed_style();
  event_stats->count_layout_box = layout_stat_tracker_->EventCountBox();
  event_stats->count_layout_box_created =
      layout_stat_tracker_->event_count_box_created();
  event_stats->count_layout_box_destroyed =
      layout_stat_tracker_->event_count_box_destroyed();
  event_stats->count_layout_update_size =
      layout_stat_tracker_->event_count_update_size();
  event_stats->count_layout_render_and_animate =
      layout_stat_tracker_->event_count_render_and_animate();
  event_stats->count_layout_update_cross_references =
      layout_stat_tracker_->event_count_update_cross_references();

  // Update event durations
  event_stats->duration_total = event_end_time - current_event_start_time_;
  event_stats->duration_dom_dispatch_event =
      stop_watch_durations_[kStopWatchTypeDispatchEvent];
  event_stats->duration_dom_run_animation_frame_callbacks =
      dom_stat_tracker_->GetStopWatchTypeDuration(
          dom::DomStatTracker::kStopWatchTypeRunAnimationFrameCallbacks);
  event_stats->duration_dom_update_computed_style =
      dom_stat_tracker_->GetStopWatchTypeDuration(
          dom::DomStatTracker::kStopWatchTypeUpdateComputedStyle);
  event_stats->duration_layout_box_tree =
      layout_stat_tracker_->GetStopWatchTypeDuration(
          layout::LayoutStatTracker::kStopWatchTypeLayoutBoxTree);
  event_stats->duration_layout_box_generation =
      layout_stat_tracker_->GetStopWatchTypeDuration(
          layout::LayoutStatTracker::kStopWatchTypeBoxGeneration);
  event_stats->duration_layout_update_used_sizes =
      layout_stat_tracker_->GetStopWatchTypeDuration(
          layout::LayoutStatTracker::kStopWatchTypeUpdateUsedSizes);
  event_stats->duration_layout_render_and_animate =
      layout_stat_tracker_->GetStopWatchTypeDuration(
          layout::LayoutStatTracker::kStopWatchTypeRenderAndAnimate);
  event_stats->duration_renderer_rasterize = renderer_rasterize_duration;

#if defined(ENABLE_WEBDRIVER)
  // When the Webdriver is enabled, all of the event's values are stored
  // within a single string representing a dictionary of key-value pairs.
  // This allows the Webdriver to query a single CVal to retrieve all of the
  // event's values.
  std::ostringstream oss;
  oss << "{"
      << "\"StartTime\":" << current_event_start_time_.ToInternalValue() << ", "
      << "\"ProducedRenderTree\":"
      << !current_event_render_tree_produced_time_.is_null() << ", "
      << "\"CntDomEventListeners\":"
      << dom::GlobalStats::GetInstance()->GetNumEventListeners() << ", "
      << "\"CntDomNodes\":" << dom::GlobalStats::GetInstance()->GetNumNodes()
      << ", "
      << "\"CntDomHtmlElements\":" << dom_stat_tracker_->EventCountHtmlElement()
      << ", "
      << "\"CntDomDocumentHtmlElements\":"
      << dom_stat_tracker_->EventCountHtmlElementDocument() << ", "
      << "\"CntDomHtmlElementsCreated\":"
      << dom_stat_tracker_->event_count_html_element_created() << ", "
      << "\"CntDomUpdateMatchingRules\":"
      << dom_stat_tracker_->event_count_update_matching_rules() << ", "
      << "\"CntDomUpdateComputedStyle\":"
      << dom_stat_tracker_->event_count_update_computed_style() << ", "
      << "\"CntDomGenerateHtmlComputedStyle\":"
      << dom_stat_tracker_->event_count_generate_html_element_computed_style()
      << ", "
      << "\"CntDomGeneratePseudoComputedStyle\":"
      << dom_stat_tracker_->event_count_generate_pseudo_element_computed_style()
      << ", "
      << "\"CntLayoutBoxes\":" << layout_stat_tracker_->EventCountBox() << ", "
      << "\"CntLayoutBoxesCreated\":"
      << layout_stat_tracker_->event_count_box_created() << ", "
      << "\"CntLayoutUpdateSize\":"
      << layout_stat_tracker_->event_count_update_size() << ", "
      << "\"CntLayoutRenderAndAnimate\":"
      << layout_stat_tracker_->event_count_render_and_animate() << ", "
      << "\"CntLayoutUpdateCrossReferences\":"
      << layout_stat_tracker_->event_count_update_cross_references() << ", "
      << "\"DurTotalUs\":"
      << (event_end_time - current_event_start_time_).InMicroseconds() << ", "
      << "\"DurDomInjectEventUs\":"
      << stop_watch_durations_[kStopWatchTypeDispatchEvent].InMicroseconds()
      << ", "
      << "\"DurDomRunAnimationFrameCallbacksUs\":"
      << dom_stat_tracker_
             ->GetStopWatchTypeDuration(
                 dom::DomStatTracker::kStopWatchTypeRunAnimationFrameCallbacks)
             .InMicroseconds()
      << ", "
      << "\"DurDomUpdateComputedStyleUs\":"
      << dom_stat_tracker_
             ->GetStopWatchTypeDuration(
                 dom::DomStatTracker::kStopWatchTypeUpdateComputedStyle)
             .InMicroseconds()
      << ", "
      << "\"DurLayoutBoxTreeUs\":"
      << layout_stat_tracker_
             ->GetStopWatchTypeDuration(
                 layout::LayoutStatTracker::kStopWatchTypeLayoutBoxTree)
             .InMicroseconds()
      << ", "
      << "\"DurLayoutBoxTreeBoxGenerationUs\":"
      << layout_stat_tracker_
             ->GetStopWatchTypeDuration(
                 layout::LayoutStatTracker::kStopWatchTypeBoxGeneration)
             .InMicroseconds()
      << ", "
      << "\"DurLayoutBoxTreeUpdateUsedSizesUs\":"
      << layout_stat_tracker_
             ->GetStopWatchTypeDuration(
                 layout::LayoutStatTracker::kStopWatchTypeUpdateUsedSizes)
             .InMicroseconds()
      << ", "
      << "\"DurLayoutRenderAndAnimateUs\":"
      << layout_stat_tracker_
             ->GetStopWatchTypeDuration(
                 layout::LayoutStatTracker::kStopWatchTypeRenderAndAnimate)
             .InMicroseconds()
      << ", "
      << "\"DurRendererRasterizeUs\":"
      << renderer_rasterize_duration.InMicroseconds() << "}";
  event_stats->value_dictionary = oss.str();
#endif  // ENABLE_WEBDRIVER

  event_is_processing_ = false;
  current_event_type_ = kEventTypeInvalid;
}

std::string WebModuleStatTracker::GetEventTypeName(
    WebModuleStatTracker::EventType event_type) {
  switch (event_type) {
    case WebModuleStatTracker::kEventTypeKeyDown:
      return "KeyDown";
    case WebModuleStatTracker::kEventTypeKeyUp:
      return "KeyUp";
    case WebModuleStatTracker::kEventTypePointerDown:
      return "PointerDown";
    case WebModuleStatTracker::kEventTypePointerUp:
      return "PointerUp";
    case WebModuleStatTracker::kEventTypeInvalid:
    case WebModuleStatTracker::kNumEventTypes:
    default:
      NOTREACHED();
      return "Invalid";
  }
}

}  // namespace browser
}  // namespace cobalt
