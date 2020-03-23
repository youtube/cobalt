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

#ifndef COBALT_DOM_HTML_COLLECTION_H_
#define COBALT_DOM_HTML_COLLECTION_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/script/property_enumerator.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Element;
class Node;

// The HTMLCollection interface represents a generic collection (array) of
// elements (in document order) and offers methods and properties for
// selecting from the list.
//
// An HTMLCollection in the HTML DOM is live; it is automatically updated when
// the underlying document is changed.
//    https://www.w3.org/TR/2014/WD-dom-20140710/#interface-htmlcollection
//
// This is a DOM interface which has an HTML prefix for historical reasons.
class HTMLCollection : public script::Wrappable {
 public:
  // A collection of all first-level child elements.
  static scoped_refptr<HTMLCollection> CreateWithChildElements(
      const scoped_refptr<const Node>& base);
  // A collections of all descendants with a given class name.
  static scoped_refptr<HTMLCollection> CreateWithElementsByClassName(
      const scoped_refptr<const Node>& base, const std::string& name);
  // A collections of all descendants with a given local name.
  static scoped_refptr<HTMLCollection> CreateWithElementsByLocalName(
      const scoped_refptr<const Node>& base, const std::string& name);

  // Web API: HTMLCollection
  virtual uint32 length() const = 0;

  virtual scoped_refptr<Element> Item(uint32 item) const = 0;
  virtual scoped_refptr<Element> NamedItem(const std::string& name) const = 0;

  // Custom, not in any spec.
  virtual bool CanQueryNamedProperty(const std::string& name) const = 0;

  // Custom, not in any spec.
  virtual void EnumerateNamedProperties(
      script::PropertyEnumerator* enumerator) const = 0;

  DEFINE_WRAPPABLE_TYPE(HTMLCollection);
  JSObjectType GetJSObjectType() override { return JSObjectType::kArray; }

 protected:
  HTMLCollection();
  virtual ~HTMLCollection();

 private:
  DISALLOW_COPY_AND_ASSIGN(HTMLCollection);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_COLLECTION_H_
