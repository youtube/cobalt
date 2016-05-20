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

#include "cobalt/dom/element.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "cobalt/base/tokens.h"
#include "cobalt/base/user_log.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_rect.h"
#include "cobalt/dom/dom_rect_list.h"
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/parser.h"
#include "cobalt/dom/serializer.h"
#include "cobalt/dom/text.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace dom {

namespace {

const char kStyleAttributeName[] = "style";

// This struct manages the user log information for Node count.
struct ElementCountLog {
 public:
  ElementCountLog() : count(0) {
    base::UserLog::Register(base::UserLog::kElementCountIndex, "ElementCnt",
                            &count, sizeof(count));
  }
  ~ElementCountLog() {
    base::UserLog::Deregister(base::UserLog::kElementCountIndex);
  }

  int count;

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementCountLog);
};

base::LazyInstance<ElementCountLog> element_count_log =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

Element::Element(Document* document, base::Token tag_name)
    : Node(document),
      tag_name_(tag_name),
      animations_(new web_animations::AnimationSet()) {
  ++(element_count_log.Get().count);
}

base::optional<std::string> Element::text_content() const {
  std::string content;

  const Node* child = first_child();
  while (child) {
    if (child->IsText() || child->IsElement()) {
      content.append(child->text_content().value());
    }
    child = child->next_sibling();
  }

  return content;
}

void Element::set_text_content(
    const base::optional<std::string>& text_content) {
  // Remove all children and replace them with a single Text node.
  while (HasChildNodes()) {
    RemoveChild(first_child());
  }
  std::string new_text_content = text_content.value_or("");
  if (!new_text_content.empty()) {
    AppendChild(new Text(node_document(), new_text_content));
  }
}

bool Element::HasAttributes() const { return !attribute_map_.empty(); }

scoped_refptr<NamedNodeMap> Element::attributes() {
  scoped_refptr<NamedNodeMap> named_node_map = named_node_map_.get();
  if (!named_node_map) {
    // Create a new instance and store a weak reference.
    named_node_map = new NamedNodeMap(this);
    named_node_map_ = named_node_map->AsWeakPtr();
  }
  return named_node_map;
}

scoped_refptr<DOMTokenList> Element::class_list() {
  if (!class_list_) {
    // Create a new instance and store a reference to it. Because of the
    // negative performance impact of having to constantly recreate DomTokenList
    // objects, they are being kept in memory.
    class_list_ = new DOMTokenList(this, "class");
  }
  return class_list_;
}

// Algorithm for GetAttribute:
//   https://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-getattribute
base::optional<std::string> Element::GetAttribute(
    const std::string& name) const {
  Document* document = node_document();

  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string attr_name = name;
  if (document && !document->IsXMLDocument()) {
    StringToLowerASCII(&attr_name);
  }

  // 2. Return the value of the attribute in element's attribute list whose
  //    namespace is namespace and local name is localName, if it has one, and
  //    null otherwise.
  switch (attr_name.size()) {
    case 5:
      if (attr_name == kStyleAttributeName) {
        return GetStyleAttribute();
      }
    // fall-through if not style attribute name
    default: {
      AttributeMap::const_iterator iter = attribute_map_.find(attr_name);
      if (iter != attribute_map_.end()) {
        return iter->second;
      }
    }
  }

  return base::nullopt;
}

// Algorithm for SetAttribute:
//   https://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-setattribute
void Element::SetAttribute(const std::string& name, const std::string& value) {
  Document* document = node_document();

  // 1. Not needed by Cobalt.

  // 2. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string attr_name = name;
  if (document && !document->IsXMLDocument()) {
    StringToLowerASCII(&attr_name);
  }

  // 3. Let attribute be the first attribute in the context object's attribute
  //    list whose name is name, or null if there is no such attribute.
  // 4. If attribute is null, create an attribute whose local name is name and
  //    value is value, and then append this attribute to the context object and
  //    terminate these steps.
  // 5. Change attribute from context object to value.
  switch (attr_name.size()) {
    case 5:
      if (attr_name == kStyleAttributeName) {
        SetStyleAttribute(value);
        if (named_node_map_) {
          named_node_map_->SetAttributeInternal(attr_name, value);
        }
        OnSetAttribute(name, value);
        // Return now as SetStyleAttribute() will call OnDOMMutation() when
        // necessary.
        return;
      }
    // fall-through if not style attribute name
    default: {
      AttributeMap::iterator attribute_iterator =
          attribute_map_.find(attr_name);
      if (attribute_iterator != attribute_map_.end() &&
          attribute_iterator->second == value) {
        // Attribute did not change.
        return;
      }
      attribute_map_[attr_name] = value;
      break;
    }
  }

  // Custom, not in any spec.
  // Check for specific attributes that require additional caching and update
  // logic.
  switch (attr_name.size()) {
    case 2:
      if (attr_name == "id") {
        id_attribute_ = base::Token(value);
      }
      break;
    case 5:
      if (attr_name == "class") {
        // Changing the class name may affect the contents of proxy objects.
        UpdateNodeGeneration();
      }
      break;
  }
  if (named_node_map_) {
    named_node_map_->SetAttributeInternal(attr_name, value);
  }

  if (document) {
    document->OnDOMMutation();
  }
  OnSetAttribute(name, value);
}

// Algorithm for RemoveAttribute:
//   https://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-removeattribute
void Element::RemoveAttribute(const std::string& name) {
  Document* document = node_document();

  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string attr_name = name;
  if (document && !document->IsXMLDocument()) {
    StringToLowerASCII(&attr_name);
  }

  // 2. Remove the first attribute from the context object whose name is name,
  //    if any.
  switch (attr_name.size()) {
    case 5:
      if (attr_name == kStyleAttributeName) {
        RemoveStyleAttribute();
        break;
      }
    // fall-through if not style attribute name
    default: {
      AttributeMap::iterator iter = attribute_map_.find(attr_name);
      if (iter == attribute_map_.end()) {
        return;
      }
      attribute_map_.erase(iter);
      break;
    }
  }

  // Custom, not in any spec.
  // Check for specific attributes that require additional caching and update
  // logic.
  switch (attr_name.size()) {
    case 2:
      if (attr_name == "id") {
        id_attribute_ = base::Token("");
      }
      break;
    case 5:
      if (attr_name == "class") {
        // Changing the class name may affect the contents of proxy objects.
        UpdateNodeGeneration();
      }
      break;
  }
  if (named_node_map_) {
    named_node_map_->RemoveAttributeInternal(attr_name);
  }

  if (document) {
    document->OnDOMMutation();
  }
  OnRemoveAttribute(name);
}

// Algorithm for HasAttribute:
//   https://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-hasattribute
bool Element::HasAttribute(const std::string& name) const {
  Document* document = node_document();

  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string attr_name = name;
  if (document && !document->IsXMLDocument()) {
    StringToLowerASCII(&attr_name);
  }

  // 2. Return true if the context object has an attribute whose name is name,
  //    and false otherwise.
  AttributeMap::const_iterator iter = attribute_map_.find(attr_name);
  return iter != attribute_map_.end();
}

scoped_refptr<HTMLCollection> Element::GetElementsByTagName(
    const std::string& tag_name) const {
  return HTMLCollection::CreateWithElementsByTagName(this, tag_name);
}

scoped_refptr<HTMLCollection> Element::GetElementsByClassName(
    const std::string& class_name) const {
  return HTMLCollection::CreateWithElementsByClassName(this, class_name);
}

namespace {

// Returns the bounding rectangle of the given DOMrect. A DOMRect can have a
// negative width or height. This function ensures that the width and height of
// the returned rectangle are positive, allowing RectF::Union() to function
// correctly.
math::RectF GetBoundingRectangle(const scoped_refptr<DOMRect>& dom_rect) {
  math::RectF bounding_rectangle;
  // This handles the case where DOMRect::width() or DOMRect::height() can be
  // negative.
  float dom_rect_x2 = dom_rect->x() + dom_rect->width();
  float rect_x = std::min(dom_rect->x(), dom_rect_x2);
  bounding_rectangle.set_x(rect_x);
  bounding_rectangle.set_width(std::max(dom_rect->x(), dom_rect_x2) - rect_x);
  float dom_rect_y2 = dom_rect->y() + dom_rect->height();
  float rect_y = std::min(dom_rect->y(), dom_rect_y2);
  bounding_rectangle.set_y(rect_y);
  bounding_rectangle.set_height(std::max(dom_rect->y(), dom_rect_y2) - rect_y);
  return bounding_rectangle;
}

}  // namespace

// Algorithm for getBoundingClientRect:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-getboundingclientrect
scoped_refptr<DOMRect> Element::GetBoundingClientRect() {
  // 1. Let list be the result of invoking getClientRects() on the same element
  // this method was invoked on.
  scoped_refptr<DOMRectList> list = GetClientRects();
  // 2. If the list is empty return a DOMRect object whose x, y, width and
  // height members are zero.
  if (list->length() == 0) {
    return make_scoped_refptr(new DOMRect());
  }
  // 3. Otherwise, return a DOMRect object describing the smallest rectangle
  // that includes the first rectangle in list and all of the remaining
  // rectangles of which the height or width is not zero.
  math::RectF bounding_rect = GetBoundingRectangle(list->Item(0));

  for (unsigned int item_number = 1; item_number < list->length();
       ++item_number) {
    const scoped_refptr<DOMRect>& box_rect = list->Item(item_number);
    if (box_rect->height() != 0.0f || box_rect->width() != 0.0f) {
      bounding_rect.Union(GetBoundingRectangle(box_rect));
    }
  }
  return make_scoped_refptr(new DOMRect(bounding_rect));
}

// Algorithm for GetClientRects:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-getclientrects
scoped_refptr<DOMRectList> Element::GetClientRects() {
  // 1. If the element on which it was invoked does not have an associated
  // layout box return an empty DOMRectList object and stop this algorithm.
  return make_scoped_refptr(new DOMRectList());
}

// Algorithm for client_top:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clienttop
float Element::client_top() {
  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  return 0.0f;
}

// Algorithm for client_left:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientleft
float Element::client_left() {
  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  return 0.0f;
}

// Algorithm for client_width:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientwidth
float Element::client_width() {
  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  return 0.0f;
}

// Algorithm for client_height:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientheight
float Element::client_height() {
  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  return 0.0f;
}

// Algorithm for inner_html:
//   https://www.w3.org/TR/DOM-Parsing/#widl-Element-innerHTML
std::string Element::inner_html() const {
  std::ostringstream oss;
  Serializer serializer(&oss);
  serializer.SerializeDescendantsOnly(this);
  return oss.str();
}

// Algorithm for set_inner_html:
//   https://www.w3.org/TR/DOM-Parsing/#widl-Element-innerHTML
void Element::set_inner_html(const std::string& inner_html) {
  // 1. Let fragment be the result of invoking the fragment parsing algorithm
  // with the new value as markup, and the context object as the context
  // element.
  // 2. Replace all with fragment within the context object.
  // Remove all children.
  scoped_refptr<Node> child = first_child();
  while (child) {
    scoped_refptr<Node> next_child = child->next_sibling();
    RemoveChild(child);
    child = next_child;
  }

  // Use the DOM parser to parse the HTML input and generate children nodes.
  Document* document = node_document();
  if (document) {
    document->html_element_context()->dom_parser()->ParseDocumentFragment(
        inner_html, document, this, NULL, GetInlineSourceLocation());
  }
}

// Algorithm for outer_html:
//   https://www.w3.org/TR/DOM-Parsing/#widl-Element-innerHTML
std::string Element::outer_html(
    script::ExceptionState* /*exception_state*/) const {
  std::ostringstream oss;
  Serializer serializer(&oss);
  serializer.Serialize(this);
  return oss.str();
}

// Algorithm for set_outer_html:
//   https://www.w3.org/TR/DOM-Parsing/#widl-Element-outerHTML
void Element::set_outer_html(const std::string& outer_html,
                             script::ExceptionState* exception_state) {
  // 1. Let parent be the context object's parent.
  scoped_refptr<Node> parent = parent_node();

  // 2. If parent is null, terminate these steps. There would be no way to
  // obtain a reference to the nodes created even if the remaining steps were
  // run.
  if (!parent) {
    return;
  }

  // 3. If parent is a Document, throw a DOMException with name
  // "NoModificationAllowedError" exception.
  if (parent->IsDocument()) {
    DOMException::Raise(dom::DOMException::kInvalidAccessErr, exception_state);
    return;
  }

  // 4. Not needed by Cobalt.

  // 5. Let fragment be the result of invoking the fragment parsing algorithm
  // with the new value as markup, and parent as the context element.
  // 6. Replace the context object with fragment within the context object's
  // parent.
  // Remove this node from its parent.
  scoped_refptr<Node> reference = next_sibling();
  parent->RemoveChild(this);

  // Use the DOM parser to parse the HTML input and generate children nodes.
  // TODO(***REMOVED***): Replace "Element" in the source location with the name
  //               of actual class, like "HTMLDivElement".
  Document* document = node_document();
  if (document) {
    document->html_element_context()->dom_parser()->ParseDocumentFragment(
        outer_html, document, parent, reference, GetInlineSourceLocation());
  }
}

void Element::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

void Element::Accept(ConstNodeVisitor* visitor) const { visitor->Visit(this); }

scoped_refptr<Node> Element::Duplicate() const {
  Element* new_element = new Element(node_document(), tag_name());
  new_element->CopyAttributes(*this);
  return new_element;
}

bool Element::IsEmpty() {
  scoped_refptr<Node> child = first_child();
  while (child) {
    if (!child->IsComment()) {
      return false;
    }
    child = child->next_sibling();
  }
  return true;
}

bool Element::HasFocus() {
  Document* document = node_document();
  return document ? (document->active_element() == this) : false;
}

base::optional<std::string> Element::GetStyleAttribute() const {
  AttributeMap::const_iterator iter = attribute_map_.find(kStyleAttributeName);
  if (iter != attribute_map_.end()) {
    return iter->second;
  }
  return base::nullopt;
}

void Element::SetStyleAttribute(const std::string& value) {
  attribute_map_[kStyleAttributeName] = value;
}

void Element::RemoveStyleAttribute() {
  attribute_map_.erase(kStyleAttributeName);
}

scoped_refptr<HTMLElement> Element::AsHTMLElement() { return NULL; }

Element::~Element() { --(element_count_log.Get().count); }

bool Element::GetBooleanAttribute(const std::string& name) const {
  return HasAttribute(name);
}

void Element::SetBooleanAttribute(const std::string& name, bool value) {
  if (value) {
    SetAttribute(name, "");
  } else {
    RemoveAttribute(name);
  }
}

void Element::CopyAttributes(const Element& other) {
  attribute_map_ = other.attribute_map_;
  id_attribute_ = other.id_attribute_;
}

HTMLElementContext* Element::html_element_context() {
  Document* document = node_document();
  return document ? document->html_element_context() : NULL;
}

std::string Element::GetDebugName() {
  std::string name = tag_name_.c_str();
  if (HasAttribute("id")) {
    name += "#";
    name += id_attribute_.c_str();
  }
  return name;
}

void Element::HTMLParseError(const std::string& error) {
  // TODO(***REMOVED***): Report line / column number.
  LOG(WARNING) << "Error when parsing inner HTML or outer HTML: " << error;
}

}  // namespace dom
}  // namespace cobalt
