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

#ifndef COBALT_DOM_GLOBAL_STATS_H_
#define COBALT_DOM_GLOBAL_STATS_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "cobalt/base/c_val.h"

namespace cobalt {

namespace xhr {
class XMLHttpRequest;
}  // namespace xhr

namespace dom {

class Attr;
class DOMStringMap;
class DOMTokenList;
class HTMLCollection;
class NamedNodeMap;
class Node;
class NodeList;

// This singleton class is used to track DOM-related statistics.
class GlobalStats {
 public:
  static GlobalStats* GetInstance();

  bool CheckNoLeaks();

  void Add(Attr* object);
  void Add(DOMStringMap* object);
  void Add(DOMTokenList* object);
  void Add(HTMLCollection* object);
  void Add(NamedNodeMap* object);
  void Add(Node* object);
  void Add(NodeList* object);
  void Add(xhr::XMLHttpRequest* object);
  void AddEventListener();
  void Remove(Attr* object);
  void Remove(DOMStringMap* object);
  void Remove(DOMTokenList* object);
  void Remove(HTMLCollection* object);
  void Remove(NamedNodeMap* object);
  void Remove(Node* object);
  void Remove(NodeList* object);
  void Remove(xhr::XMLHttpRequest* object);
  void RemoveEventListener();
  void IncreaseXHRMemoryUsage(size_t delta);
  void DecreaseXHRMemoryUsage(size_t delta);

 private:
  GlobalStats();
  ~GlobalStats();

  // Number of live DOM objects.
  base::DebugCVal<int> num_attrs;
  base::DebugCVal<int> num_dom_string_maps;
  base::DebugCVal<int> num_dom_token_lists;
  base::PublicCVal<int> num_event_listeners;
  base::DebugCVal<int> num_html_collections;
  base::DebugCVal<int> num_named_node_maps;
  base::PublicCVal<int> num_nodes;
  base::DebugCVal<int> num_node_lists;
  base::DebugCVal<int> num_xhrs;
  base::DebugCVal<size_t> xhr_memory;

  friend struct DefaultSingletonTraits<GlobalStats>;
  DISALLOW_COPY_AND_ASSIGN(GlobalStats);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_GLOBAL_STATS_H_
