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
    : dom_stat_tracker_(new dom::DomStatTracker(name)),
      layout_stat_tracker_(new layout::LayoutStatTracker(name)),
      should_track_event_stats_(should_track_event_stats),
      current_event_type_(kEventTypeInvalid),
      name_(name),
      event_is_processing_(StringPrintf("Event.%s.IsProcessing", name.c_str()),
                           false, "Nonzero when an event is being processed.") {
  if (should_track_event_stats_) {
    event_stats_.reserve(kNumEventTypes);
    for (int i = 0; i < kNumEventTypes; ++i) {
      EventType event_type = static_cast<EventType>(i);
      event_stats_.push_back(new EventStats(StringPrintf(
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

WebModuleStatTracker::~WebModuleStatTracker() { EndCurrentEvent(false); }

void WebModuleStatTracker::OnStartInjectEvent(
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
  } else {
    current_event_type_ = kEventTypeInvalid;
  }

  // If this is a valid event type, then start tracking it.
  if (current_event_type_ != kEventTypeInvalid) {
    event_is_processing_ = true;

    dom_stat_tracker_->OnStartEvent();
    layout_stat_tracker_->OnStartEvent();

    stop_watch_durations_[kStopWatchTypeEvent] = base::TimeDelta();
    stop_watch_durations_[kStopWatchTypeInjectEvent] = base::TimeDelta();

    stop_watches_[kStopWatchTypeEvent].Start();
    stop_watches_[kStopWatchTypeInjectEvent].Start();
  }
}

void WebModuleStatTracker::OnEndInjectEvent(
    bool are_animation_frame_callbacks_pending,
    bool is_new_render_tree_pending) {
  // If the injection isn't currently being timed, then this event injection
  // isn't being tracked. Simply return.
  if (!stop_watches_[kStopWatchTypeInjectEvent].IsCounting()) {
    return;
  }

  stop_watches_[kStopWatchTypeInjectEvent].Stop();

  if (!are_animation_frame_callbacks_pending && !is_new_render_tree_pending) {
    EndCurrentEvent(false);
  }
}

void WebModuleStatTracker::OnRanAnimationFrameCallbacks(
    bool is_new_render_tree_pending) {
  if (!is_new_render_tree_pending) {
    EndCurrentEvent(false);
  }
}

void WebModuleStatTracker::OnRenderTreeProduced() { EndCurrentEvent(true); }

WebModuleStatTracker::EventStats::EventStats(const std::string& name)
    : produced_render_tree_(
          StringPrintf("Event.%s.ProducedRenderTree", name.c_str()), false,
          "Nonzero when the event produced a render tree."),
      count_dom_html_elements_created(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Created", name.c_str()),
          0, "Number of HTML elements created."),
      count_dom_html_elements_destroyed(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Destroyed",
                       name.c_str()),
          0, "Number of HTML elements destroyed."),
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
      count_layout_boxes_created(
          StringPrintf("Event.Count.%s.Layout.Box.Created", name.c_str()), 0,
          "Number of boxes created."),
      count_layout_boxes_destroyed(
          StringPrintf("Event.Count.%s.Layout.Box.Destroyed", name.c_str()), 0,
          "Number of boxes destroyed."),
      count_layout_update_size(
          StringPrintf("Event.Count.%s.Layout.Box.UpdateSize", name.c_str()), 0,
          "Number of boxes that had their size updated."),
      count_layout_render_and_animate(
          StringPrintf("Event.Count.%s.Layout.Box.RenderAndAnimate",
                       name.c_str()),
          0, "Number of boxes that had their render tree node updated."),
      count_layout_update_cross_references(
          StringPrintf("Event.Count.%s.Layout.Box.UpdateCrossReferences",
                       name.c_str()),
          0, "Number of boxes that had their cross references updated."),
      duration_total(StringPrintf("Event.Duration.%s", name.c_str()),
                     base::TimeDelta(),
                     "Total duration of the event (in microseconds). This is "
                     "the time elapsed from the event injection until the "
                     "render tree is produced."),
      duration_dom_inject_event(
          StringPrintf("Event.Duration.%s.DOM.InjectEvent", name.c_str()),
          base::TimeDelta(),
          "Injection duration, which includes JS, for event (in "
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
          "RenderAndAnimate duration for event (in microseconds).")
#if defined(ENABLE_WEBDRIVER)
      ,
      value_dictionary(
          StringPrintf("Event.%s.ValueDictionary", name.c_str()),
          "{}"
          "All event values represented as a dictionary in a string.")
#endif  // ENABLE_WEBDRIVER
{
}

bool WebModuleStatTracker::IsStopWatchEnabled(int /*id*/) const { return true; }

void WebModuleStatTracker::OnStopWatchStopped(int id,
                                              base::TimeDelta time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

void WebModuleStatTracker::EndCurrentEvent(bool was_render_tree_produced) {
  if (current_event_type_ == kEventTypeInvalid) {
    return;
  }

  event_is_processing_ = false;

  stop_watches_[kStopWatchTypeEvent].Stop();

  EventStats* event_stats = event_stats_[current_event_type_];
  event_stats->produced_render_tree_ = was_render_tree_produced;

  // Update event counts
  event_stats->count_dom_html_elements_created =
      dom_stat_tracker_->html_elements_created_count();
  event_stats->count_dom_html_elements_destroyed =
      dom_stat_tracker_->html_elements_destroyed_count();
  event_stats->count_dom_update_matching_rules =
      dom_stat_tracker_->update_matching_rules_count();
  event_stats->count_dom_update_computed_style =
      dom_stat_tracker_->update_computed_style_count();
  event_stats->count_dom_generate_html_element_computed_style =
      dom_stat_tracker_->generate_html_element_computed_style_count();
  event_stats->count_dom_generate_pseudo_element_computed_style =
      dom_stat_tracker_->generate_pseudo_element_computed_style_count();
  event_stats->count_layout_boxes_created =
      layout_stat_tracker_->boxes_created_count();
  event_stats->count_layout_boxes_destroyed =
      layout_stat_tracker_->boxes_destroyed_count();
  event_stats->count_layout_update_size =
      layout_stat_tracker_->update_size_count();
  event_stats->count_layout_render_and_animate =
      layout_stat_tracker_->render_and_animate_count();
  event_stats->count_layout_update_cross_references =
      layout_stat_tracker_->update_cross_references_count();

  // Update event durations
  event_stats->duration_total = stop_watch_durations_[kStopWatchTypeEvent];
  event_stats->duration_dom_inject_event =
      stop_watch_durations_[kStopWatchTypeInjectEvent];
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

#if defined(ENABLE_WEBDRIVER)
  // When the Webdriver is enabled, all of the event's values are stored within
  // a single string representing a dictionary of key-value pairs. This allows
  // the Webdriver to query a single CVal to retrieve all of the event's values.
  std::ostringstream oss;
  oss << "{"
      << "\"ProducedRenderTree\":" << was_render_tree_produced << ", "
      << "\"CntDomEventListeners\":"
      << dom::GlobalStats::GetInstance()->GetNumEventListeners() << ", "
      << "\"CntDomNodes\":" << dom::GlobalStats::GetInstance()->GetNumNodes()
      << ", "
      << "\"CntDomHtmlElements\":" << dom_stat_tracker_->total_html_elements()
      << ", "
      << "\"CntDomHtmlElementsCreated\":"
      << dom_stat_tracker_->html_elements_created_count() << ", "
      << "\"CntDomHtmlElementsDestroyed\":"
      << dom_stat_tracker_->html_elements_destroyed_count() << ", "
      << "\"CntDomUpdateMatchingRules\":"
      << dom_stat_tracker_->update_matching_rules_count() << ", "
      << "\"CntDomUpdateComputedStyle\":"
      << dom_stat_tracker_->update_computed_style_count() << ", "
      << "\"CntDomGenerateHtmlComputedStyle\":"
      << dom_stat_tracker_->generate_html_element_computed_style_count() << ", "
      << "\"CntDomGeneratePseudoComputedStyle\":"
      << dom_stat_tracker_->generate_pseudo_element_computed_style_count()
      << ", "
      << "\"CntLayoutBoxes\":" << layout_stat_tracker_->total_boxes() << ", "
      << "\"CntLayoutBoxesCreated\":"
      << layout_stat_tracker_->boxes_created_count() << ", "
      << "\"CntLayoutBoxesDestroyed\":"
      << layout_stat_tracker_->boxes_destroyed_count() << ", "
      << "\"CntLayoutUpdateSize\":" << layout_stat_tracker_->update_size_count()
      << ", "
      << "\"CntLayoutRenderAndAnimate\":"
      << layout_stat_tracker_->render_and_animate_count() << ", "
      << "\"CntLayoutUpdateCrossReferences\":"
      << layout_stat_tracker_->update_cross_references_count() << ", "
      << "\"DurTotalUs\":"
      << stop_watch_durations_[kStopWatchTypeEvent].InMicroseconds() << ", "
      << "\"DurDomInjectEventUs\":"
      << stop_watch_durations_[kStopWatchTypeInjectEvent].InMicroseconds()
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
      << "}";
  event_stats->value_dictionary = oss.str();
#endif  // ENABLE_WEBDRIVER

  current_event_type_ = kEventTypeInvalid;

  dom_stat_tracker_->OnEndEvent();
  layout_stat_tracker_->OnEndEvent();
}

std::string WebModuleStatTracker::GetEventTypeName(
    WebModuleStatTracker::EventType event_type) {
  switch (event_type) {
    case WebModuleStatTracker::kEventTypeKeyDown:
      return "KeyDown";
    case WebModuleStatTracker::kEventTypeKeyUp:
      return "KeyUp";
    case WebModuleStatTracker::kEventTypeInvalid:
    case WebModuleStatTracker::kNumEventTypes:
    default:
      NOTREACHED();
      return "Invalid";
  }
}

}  // namespace browser
}  // namespace cobalt
