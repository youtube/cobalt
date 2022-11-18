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
      num_html_collections_(
          "Count.DOM.HtmlCollections", 0,
          "Total number of currently active HTML collections."),
      num_named_node_maps_("Count.DOM.NodeMaps", 0,
                           "Total number of currently active node maps."),
      num_nodes_("Count.DOM.Nodes", 0,
                 "Total number of currently active nodes."),
      num_node_lists_("Count.DOM.NodeLists", 0,
                      "Total number of currently active node lists."),
      total_font_request_time_(
          "Time.MainWebModule.DOM.FontCache.TotalFontRequestTime", 0,
          "The time it takes for all fonts requests to complete") {}

GlobalStats::~GlobalStats() {}

bool GlobalStats::CheckNoLeaks() {
  DCHECK(num_attrs_ == 0) << num_attrs_;
  DCHECK(num_dom_string_maps_ == 0) << num_dom_string_maps_;
  DCHECK(num_dom_token_lists_ == 0) << num_dom_token_lists_;
  DCHECK(num_html_collections_ == 0) << num_html_collections_;
  DCHECK(num_named_node_maps_ == 0) << num_named_node_maps_;
  DCHECK(num_nodes_ == 0) << num_nodes_;
  DCHECK(num_node_lists_ == 0) << num_node_lists_;
  return web::GlobalStats::GetInstance()->CheckNoLeaks() &&
         xhr::GlobalStats::GetInstance()->CheckNoLeaks() && num_attrs_ == 0 &&
         num_dom_string_maps_ == 0 && num_dom_token_lists_ == 0 &&
         num_html_collections_ == 0 && num_named_node_maps_ == 0 &&
         num_nodes_ == 0 && num_node_lists_ == 0;
}

void GlobalStats::Add(Attr* object) { ++num_attrs_; }

void GlobalStats::Add(DOMStringMap* object) { ++num_dom_string_maps_; }

void GlobalStats::Add(DOMTokenList* object) { ++num_dom_token_lists_; }

void GlobalStats::Add(HTMLCollection* object) { ++num_html_collections_; }

void GlobalStats::Add(NamedNodeMap* object) { ++num_named_node_maps_; }

void GlobalStats::Add(Node* object) { ++num_nodes_; }

void GlobalStats::Add(NodeList* object) { ++num_node_lists_; }

void GlobalStats::Remove(Attr* object) { --num_attrs_; }

void GlobalStats::Remove(DOMStringMap* object) { --num_dom_string_maps_; }

void GlobalStats::Remove(DOMTokenList* object) { --num_dom_token_lists_; }

void GlobalStats::Remove(HTMLCollection* object) { --num_html_collections_; }

void GlobalStats::Remove(NamedNodeMap* object) { --num_named_node_maps_; }

void GlobalStats::Remove(Node* object) { --num_nodes_; }

void GlobalStats::Remove(NodeList* object) { --num_node_lists_; }

void GlobalStats::OnFontRequestComplete(int64 start_time) {
  total_font_request_time_ +=
      base::TimeTicks::Now().ToInternalValue() - start_time;
}

}  // namespace dom
}  // namespace cobalt
