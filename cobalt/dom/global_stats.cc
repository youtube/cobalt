// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/global_stats.h"

#include "base/logging.h"

namespace cobalt {
namespace dom {

// static
GlobalStats* GlobalStats::GetInstance() {
  return base::Singleton<GlobalStats>::get();
}

GlobalStats::GlobalStats()
    : num_attrs_("Count.DOM.Attrs", 0,
                 "Total number of currently active attributes."),
      num_dom_string_maps_("Count.DOM.StringMaps", 0,
                           "Total number of currently active string maps."),
      num_dom_token_lists_("Count.DOM.TokenLists", 0,
                           "Total number of currently active token lists."),
      num_event_listeners_("Count.DOM.EventListeners", 0,
                           "Total number of currently active event listeners."),
      num_html_collections_(
          "Count.DOM.HtmlCollections", 0,
          "Total number of currently active HTML collections."),
      num_named_node_maps_("Count.DOM.NodeMaps", 0,
                           "Total number of currently active node maps."),
      num_nodes_("Count.DOM.Nodes", 0,
                 "Total number of currently active nodes."),
      num_node_lists_("Count.DOM.NodeLists", 0,
                      "Total number of currently active node lists."),
      num_active_java_script_events_(
          "Count.DOM.ActiveJavaScriptEvents", 0,
          "Total number of currently active JavaScript events."),
      num_xhrs_("Count.XHR", 0, "Total number of currently active XHRs."),
      xhr_memory_("Memory.XHR", 0, "Memory allocated by XHRs in bytes."),
      total_font_request_time_(
          "Time.MainWebModule.DOM.FontCache.TotalFontRequestTime", 0,
          "The time it takes for all fonts requests to complete") {}

GlobalStats::~GlobalStats() {}

bool GlobalStats::CheckNoLeaks() {
  return num_attrs_ == 0 && num_dom_string_maps_ == 0 &&
         num_dom_token_lists_ == 0 && num_event_listeners_ == 0 &&
         num_html_collections_ == 0 && num_named_node_maps_ == 0 &&
         num_nodes_ == 0 && num_node_lists_ == 0 &&
         num_active_java_script_events_ == 0 && num_xhrs_ == 0 &&
         xhr_memory_ == 0;
}

void GlobalStats::Add(Attr* object) { ++num_attrs_; }

void GlobalStats::Add(DOMStringMap* object) { ++num_dom_string_maps_; }

void GlobalStats::Add(DOMTokenList* object) { ++num_dom_token_lists_; }

void GlobalStats::Add(HTMLCollection* object) { ++num_html_collections_; }

void GlobalStats::Add(NamedNodeMap* object) { ++num_named_node_maps_; }

void GlobalStats::Add(Node* object) { ++num_nodes_; }

void GlobalStats::Add(NodeList* object) { ++num_node_lists_; }

void GlobalStats::AddEventListener() { ++num_event_listeners_; }

void GlobalStats::Remove(Attr* object) { --num_attrs_; }

void GlobalStats::Remove(DOMStringMap* object) { --num_dom_string_maps_; }

void GlobalStats::Remove(DOMTokenList* object) { --num_dom_token_lists_; }

void GlobalStats::Remove(HTMLCollection* object) { --num_html_collections_; }

void GlobalStats::Remove(NamedNodeMap* object) { --num_named_node_maps_; }

void GlobalStats::Remove(Node* object) { --num_nodes_; }

void GlobalStats::Remove(NodeList* object) { --num_node_lists_; }

void GlobalStats::RemoveEventListener() { --num_event_listeners_; }

void GlobalStats::StartJavaScriptEvent() { ++num_active_java_script_events_; }

void GlobalStats::StopJavaScriptEvent() { --num_active_java_script_events_; }

void GlobalStats::Add(xhr::XMLHttpRequest* object) { ++num_xhrs_; }

void GlobalStats::Remove(xhr::XMLHttpRequest* object) { --num_xhrs_; }

void GlobalStats::IncreaseXHRMemoryUsage(size_t delta) { xhr_memory_ += delta; }

void GlobalStats::DecreaseXHRMemoryUsage(size_t delta) {
  DCHECK_GE(xhr_memory_.value(), delta);
  xhr_memory_ -= delta;
}

void GlobalStats::OnFontRequestComplete(int64 start_time) {
  total_font_request_time_ +=
      base::TimeTicks::Now().ToInternalValue() - start_time;
}

}  // namespace dom
}  // namespace cobalt
