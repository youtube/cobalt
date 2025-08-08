// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/dom_agent.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/adapters.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_devtools_features.h"
#include "components/ui_devtools/ui_element.h"
#include "ui/base/interaction/element_identifier.h"

namespace {

std::string CreateIdentifier() {
  static int last_used_identifier;
  return base::NumberToString(++last_used_identifier);
}

}  // namespace

namespace ui_devtools {

using ui_devtools::protocol::Array;
using ui_devtools::protocol::DOM::Node;
using ui_devtools::protocol::Response;

namespace {

bool FindMatchInStylesProperty(const std::string& query,
                               ui_devtools::UIElement* node) {
  for (UIElement::ClassProperties& class_properties :
       node->GetCustomPropertiesForMatchedStyle()) {
    for (UIElement::UIProperty& ui_property : class_properties.properties_) {
      protocol::String dataName = ui_property.name_;
      protocol::String dataValue = ui_property.value_;
      dataName = base::ToLowerASCII(dataName);
      dataValue = base::ToLowerASCII(dataValue);
      protocol::String style_property_match = dataName + ": " + dataValue + ";";
      if (style_property_match.find(query) != protocol::String::npos) {
        return true;
      }
    }
  }
  return false;
}

bool FindMatchByID(const std::string& query, ui_devtools::UIElement* node) {
  auto identifier = ui::ElementIdentifier::FromName(query.c_str());
  if (!identifier)
    return false;
  return node->FindMatchByElementID(identifier);
}

bool FindMatchInDomProperties(const std::string& query,
                              const std::string& tag_name_query,
                              const std::string& attribute_query,
                              bool exact_attribute_match,
                              ui_devtools::UIElement* node) {
  protocol::String node_name = node->GetTypeName();
  node_name = base::ToLowerASCII(node_name);
  if (node_name.find(query) != protocol::String::npos ||
      node_name == tag_name_query) {
    return true;
  }
  for (std::string& data : node->GetAttributes()) {
    data = base::ToLowerASCII(data);
    if (data.find(query) != protocol::String::npos) {
      return true;
    }
    if (data.find(attribute_query) != protocol::String::npos &&
        (!exact_attribute_match || data.length() == attribute_query.length())) {
      return true;
    }
  }
  return false;
}

}  // namespace

struct DOMAgent::Query {
  enum QueryType { Normal, Style, ID };
  Query(protocol::String query,
        protocol::String tag_name_query,
        protocol::String attribute_query,
        bool exact_attribute_match,
        QueryType query_type)
      : query_(query),
        tag_name_query_(tag_name_query),
        attribute_query_(attribute_query),
        exact_attribute_match_(exact_attribute_match),
        query_type_(query_type) {}
  protocol::String query_;
  protocol::String tag_name_query_;
  protocol::String attribute_query_;
  bool exact_attribute_match_;
  QueryType query_type_;
};

DOMAgent::DOMAgent() {}

DOMAgent::~DOMAgent() {
  Reset();
}

Response DOMAgent::disable() {
  Reset();
  return Response::Success();
}

Response DOMAgent::getDocument(std::unique_ptr<Node>* out_root) {
  element_root_->ResetNodeId();
  *out_root = BuildInitialTree();
  is_document_created_ = true;
  return Response::Success();
}

Response DOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backend_node_ids,
    std::unique_ptr<protocol::Array<int>>* result) {
  *result = std::move(backend_node_ids);
  return Response::Success();
}

void DOMAgent::OnUIElementAdded(UIElement* parent, UIElement* child) {
  // When parent is null, only need to update |node_id_to_ui_element_|.
  if (!parent) {
    node_id_to_ui_element_[child->node_id()] = child;
    return;
  }

  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  auto* current_parent = parent;
  while (current_parent) {
    if (current_parent->is_updating()) {
      // One of the parents is updating, so no need to update here.
      return;
    }
    current_parent = current_parent->parent();
  }

  child->set_is_updating(true);

  const auto& children = parent->children();
  auto iter = base::ranges::find(children, child);
  int prev_node_id =
      (iter == children.begin()) ? 0 : (*std::prev(iter))->node_id();
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildTreeForUIElement(child));

  child->set_is_updating(false);
  for (auto& observer : observers_)
    observer.OnElementAdded(child);
}

void DOMAgent::OnUIElementReordered(UIElement* parent, UIElement* child) {
  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  const auto& children = parent->children();
  auto iter = base::ranges::find(children, child);
  CHECK(iter != children.end());
  int prev_node_id =
      (iter == children.begin()) ? 0 : (*std::prev(iter))->node_id();
  RemoveDomNode(child, false);
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildDomNodeFromUIElement(child));
}

void DOMAgent::OnUIElementRemoved(UIElement* ui_element) {
  if (node_id_to_ui_element_.count(ui_element->node_id())) {
    // Check if |ui_element| exists in DOM before asking the frontend to remove
    // it. It is possible that this node is already removed along with its
    // ancestor. This happens for example when a bubble closes.
    // The DOM tree is first requested to be removed in
    // |WidgetElement::OnWidgetDestroyed()| then in
    // |ViewElement::OnChildViewRemoved()|.
    RemoveDomNode(ui_element, true);
  }
}

void DOMAgent::OnUIElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

void DOMAgent::AddObserver(DOMAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void DOMAgent::RemoveObserver(DOMAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

UIElement* DOMAgent::GetElementFromNodeId(int node_id) const {
  auto it = node_id_to_ui_element_.find(node_id);
  if (it != node_id_to_ui_element_.end())
    return it->second;
  return nullptr;
}

int DOMAgent::GetParentIdOfNodeId(int node_id) const {
  DCHECK(node_id_to_ui_element_.count(node_id));
  const UIElement* element = node_id_to_ui_element_.at(node_id);
  if (element->parent() && element->parent() != element_root_.get())
    return element->parent()->node_id();
  return 0;
}

std::unique_ptr<Node> DOMAgent::BuildNode(
    const std::string& name,
    std::unique_ptr<std::vector<std::string>> attributes,
    std::unique_ptr<Array<Node>> children,
    int node_ids) {
  constexpr int kDomElementNodeType = 1;
  std::unique_ptr<Node> node = Node::create()
                                   .setNodeId(node_ids)
                                   .setBackendNodeId(node_ids)
                                   .setNodeName(name)
                                   .setNodeType(kDomElementNodeType)
                                   .setAttributes(std::move(attributes))
                                   .build();
  node->setChildNodeCount(static_cast<int>(children->size()));
  node->setChildren(std::move(children));
  return node;
}

std::unique_ptr<Node> DOMAgent::BuildDomNodeFromUIElement(UIElement* root) {
  auto children = std::make_unique<protocol::Array<Node>>();
  for (auto* it : root->children())
    children->emplace_back(BuildDomNodeFromUIElement(it));

  return BuildNode(
      root->GetTypeName(),
      std::make_unique<std::vector<std::string>>(root->GetAttributes()),
      std::move(children), root->node_id());
}

std::unique_ptr<Node> DOMAgent::BuildInitialTree() {
  auto children = std::make_unique<protocol::Array<Node>>();

  element_root_ = std::make_unique<RootElement>(this);
  element_root_->set_is_updating(true);

  for (auto* child : CreateChildrenForRoot()) {
    children->emplace_back(BuildTreeForUIElement(child));
    element_root_->AddChild(child);
  }
  std::unique_ptr<Node> root_node =
      BuildNode("root", nullptr, std::move(children), element_root_->node_id());
  element_root_->set_is_updating(false);
  return root_node;
}

void DOMAgent::OnElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

void DOMAgent::RemoveDomNode(UIElement* ui_element, bool update_node_id_map) {
  for (auto* child_element : ui_element->children())
    RemoveDomNode(child_element, update_node_id_map);
  frontend()->childNodeRemoved(ui_element->parent()->node_id(),
                               ui_element->node_id());
  if (update_node_id_map) {
    node_id_to_ui_element_.erase(ui_element->node_id());
  }
}

void DOMAgent::Reset() {
  element_root_.reset();
  node_id_to_ui_element_.clear();
  observers_.Clear();
  is_document_created_ = false;
  search_results_.clear();
}

DOMAgent::Query DOMAgent::PreprocessQuery(protocol::String query) {
  constexpr char kSearchIDKeyword[] = "id:";
  bool exact_attribute_match = false;
  if (query.find(kSearchIDKeyword) == 0) {
    // remove whitespaces (if they exist) after the 'id:' keyword
    base::TrimWhitespaceASCII(query.substr(strlen(kSearchIDKeyword)),
                              base::TrimPositions::TRIM_ALL, &query);
    return Query(query, query, query, exact_attribute_match,
                 Query::QueryType::ID);
  }

  // If it's not query by id, transform query to lower case.
  query = base::ToLowerASCII(query);

  constexpr char kSearchStylesPanelKeyword[] = "style:";
  // Temporary Solution: search on styles panel if the keyword 'style:'
  // exists in the beginning of the query.
  if (query.find(kSearchStylesPanelKeyword) == 0) {
    // remove whitespaces (if they exist) after the 'style:' keyword
    base::TrimWhitespaceASCII(query.substr(strlen(kSearchStylesPanelKeyword)),
                              base::TrimPositions::TRIM_ALL, &query);
    return Query(query, query, query, exact_attribute_match,
                 Query::QueryType::Style);
  }

  // Preprocessing for normal dom search.
  protocol::String tag_name_query = query;
  protocol::String attribute_query = query;
  unsigned query_length = query.length();
  bool start_tag_found = !query.find('<');
  bool end_tag_found = query.rfind('>') + 1 == query_length;
  bool start_quote_found = !query.find('"');
  bool end_quote_found = query.rfind('"') + 1 == query_length;
  exact_attribute_match = start_quote_found && end_quote_found;

  if (start_tag_found)
    tag_name_query = tag_name_query.substr(1, tag_name_query.length() - 1);
  if (end_tag_found)
    tag_name_query = tag_name_query.substr(0, tag_name_query.length() - 1);
  if (start_quote_found)
    attribute_query = attribute_query.substr(1, attribute_query.length() - 1);
  if (end_quote_found)
    attribute_query = attribute_query.substr(0, attribute_query.length() - 1);
  return Query(query, tag_name_query, attribute_query, exact_attribute_match,
               Query::QueryType::Normal);
}

void DOMAgent::SearchDomTree(const DOMAgent::Query& query_data,
                             std::vector<int>* result_collector) {
  std::vector<UIElement*> stack;
  // Root node from element_root() is not a real node from the DOM tree.
  // The children of the root node are the 'actual' roots of the DOM tree.
  std::vector<UIElement*> root_list = element_root()->children();
  DCHECK(root_list.size());
  // Children are accessed from bottom to top. So iterate backwards.
  for (auto* root : base::Reversed(root_list))
    stack.push_back(root);

  // Manual plain text search. DFS traversal.
  while (!stack.empty()) {
    UIElement* node = stack.back();
    stack.pop_back();
    std::vector<UIElement*> children_array = node->children();
    // Children are accessed from bottom to top. So iterate backwards.
    for (auto* child : base::Reversed(children_array))
      stack.push_back(child);
    bool found_match = false;
    if (query_data.query_type_ == Query::QueryType::Style)
      found_match = FindMatchInStylesProperty(query_data.query_, node);
    else if (query_data.query_type_ == Query::QueryType::ID)
      found_match = FindMatchByID(query_data.query_, node);
    else
      found_match = FindMatchInDomProperties(
          query_data.query_, query_data.tag_name_query_,
          query_data.attribute_query_, query_data.exact_attribute_match_, node);
    if (found_match)
      result_collector->push_back(node->node_id());
  }
}

// Code is based on InspectorDOMAgent::performSearch() from
// src/third_party/blink/renderer/core/inspector/inspector_dom_agent.cc
Response DOMAgent::performSearch(
    const protocol::String& whitespace_trimmed_query,
    protocol::Maybe<bool> optional_include_user_agent_shadow_dom,
    protocol::String* search_id,
    int* result_count) {
  Query query_data = PreprocessQuery(whitespace_trimmed_query);
  if (!query_data.query_.length())
    return Response::Success();

  std::vector<int> result_collector;
  SearchDomTree(query_data, &result_collector);

  *search_id = CreateIdentifier();
  *result_count = result_collector.size();
  search_results_.insert(
      std::make_pair(*search_id, std::move(result_collector)));
  return Response::Success();
}

Response DOMAgent::getSearchResults(
    const protocol::String& search_id,
    int from_index,
    int to_index,
    std::unique_ptr<protocol::Array<int>>* node_ids) {
  SearchResults::iterator it = search_results_.find(search_id);
  if (it == search_results_.end())
    return Response::ServerError("No search session with given id found");

  int size = it->second.size();
  if (from_index < 0 || to_index > size || from_index >= to_index)
    return Response::ServerError("Invalid search result range");

  *node_ids = std::make_unique<protocol::Array<int>>();
  for (int i = from_index; i < to_index; ++i)
    (*node_ids)->emplace_back((it->second)[i]);
  return Response::Success();
}

Response DOMAgent::discardSearchResults(const protocol::String& search_id) {
  search_results_.erase(search_id);
  return Response::Success();
}

protocol::Response DOMAgent::dispatchMouseEvent(
    int node_id,
    std::unique_ptr<protocol::DOM::MouseEvent> event) {
  if (!base::FeatureList::IsEnabled(
          ui_devtools::kUIDebugToolsEnableSyntheticEvents))
    return Response::ServerError("Dispatch mouse events is not enabled.");
  if (node_id_to_ui_element_.count(node_id) == 0)
    return Response::ServerError("Element not found on node id");
  if (!node_id_to_ui_element_[node_id]->DispatchMouseEvent(event.get()))
    return Response::ServerError("Failed to dispatch mouse event for node id");
  return Response::Success();
}

protocol::Response DOMAgent::dispatchKeyEvent(
    int node_id,
    std::unique_ptr<protocol::DOM::KeyEvent> event) {
  if (!base::FeatureList::IsEnabled(
          ui_devtools::kUIDebugToolsEnableSyntheticEvents))
    return Response::ServerError("Dispatch key events is not enabled.");
  if (node_id_to_ui_element_.count(node_id) == 0)
    return Response::ServerError("Element not found on node id");
  if (!node_id_to_ui_element_[node_id]->DispatchKeyEvent(event.get()))
    return Response::ServerError("Failed to dispatch key event for node id");
  return Response::Success();
}

}  // namespace ui_devtools
