/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/global_stats.h"

#include "base/logging.h"

namespace cobalt {
namespace dom {

// static
GlobalStats* GlobalStats::GetInstance() {
  return Singleton<GlobalStats>::get();
}

GlobalStats::GlobalStats()
    : num_attrs("DOM.Attrs", 0, "Total number of currently active attributes."),
      num_dom_string_maps("DOM.StringMaps", 0,
                          "Total number of currently active string maps."),
      num_dom_token_lists("DOM.TokenLists", 0,
                          "Total number of currently active token lists."),
      num_event_listeners("DOM.EventListeners", 0,
                          "Total number of currently active event listeners."),
      num_html_collections(
          "DOM.HtmlCollections", 0,
          "Total number of currently active HTML collections."),
      num_named_node_maps("DOM.NodeMaps", 0,
                          "Total number of currently active node maps."),
      num_nodes("DOM.Nodes", 0, "Total number of currently active nodes."),
      num_node_lists("DOM.NodeLists", 0,
                     "Total number of currently active node lists."),
      num_xhrs("DOM.XHR", 0, "Total number of currently active XHRs."),
      xhr_memory("Memory.XHR", 0, "Memory allocated by XHRs in bytes.") {}

GlobalStats::~GlobalStats() {}

bool GlobalStats::CheckNoLeaks() {
  return num_attrs == 0 && num_dom_string_maps == 0 &&
         num_dom_token_lists == 0 && num_event_listeners == 0 &&
         num_html_collections == 0 && num_named_node_maps == 0 &&
         num_nodes == 0 && num_node_lists == 0 && num_xhrs == 0 &&
         xhr_memory == 0;
}

void GlobalStats::Add(Attr* /*object*/) { ++num_attrs; }

void GlobalStats::Add(DOMStringMap* /*object*/) { ++num_dom_string_maps; }

void GlobalStats::Add(DOMTokenList* /*object*/) { ++num_dom_token_lists; }

void GlobalStats::Add(HTMLCollection* /*object*/) { ++num_html_collections; }

void GlobalStats::Add(NamedNodeMap* /*object*/) { ++num_named_node_maps; }

void GlobalStats::Add(Node* /*object*/) { ++num_nodes; }

void GlobalStats::Add(NodeList* /*object*/) { ++num_node_lists; }

void GlobalStats::Add(xhr::XMLHttpRequest* /* object */) { ++num_xhrs; }

void GlobalStats::AddEventListener() { ++num_event_listeners; }

void GlobalStats::Remove(Attr* /*object*/) { --num_attrs; }

void GlobalStats::Remove(DOMStringMap* /*object*/) { --num_dom_string_maps; }

void GlobalStats::Remove(DOMTokenList* /*object*/) { --num_dom_token_lists; }

void GlobalStats::Remove(HTMLCollection* /*object*/) { --num_html_collections; }

void GlobalStats::Remove(NamedNodeMap* /*object*/) { --num_named_node_maps; }

void GlobalStats::Remove(Node* /*object*/) { --num_nodes; }

void GlobalStats::Remove(NodeList* /*object*/) { --num_node_lists; }

void GlobalStats::Remove(xhr::XMLHttpRequest* /*object*/) { --num_xhrs; }

void GlobalStats::RemoveEventListener() { --num_event_listeners; }

void GlobalStats::IncreaseXHRMemoryUsage(size_t delta) { xhr_memory += delta; }

void GlobalStats::DecreaseXHRMemoryUsage(size_t delta) {
  DCHECK_GE(xhr_memory, delta);
  xhr_memory -= delta;
}

}  // namespace dom
}  // namespace cobalt
