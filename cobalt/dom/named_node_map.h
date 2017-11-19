// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_NAMED_NODE_MAP_H_
#define COBALT_DOM_NAMED_NODE_MAP_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/property_enumerator.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Attr;
class Element;

// The NamedNodeMap interface represents a collection of Attr objects. Objects
// inside a NamedNodeMap are not in any particular order, unlike NodeList,
// although they may be accessed by an index as in an array.
// A NamedNodeMap object is live and will thus be auto-updated if changes are
// made to its contents internally or elsewhere.
//   https://dom.spec.whatwg.org/#interface-namednodemap
//
// This interface is a legacy interface defined in DOM Level 3 Core, and is
// referenced in DOM4. The whatwg live spec changes some definitions that is
// adopted by major browsers. The whatwg version is followed here.
//
// Using Attr objects directly to represent Element attributes would incur
// a significant memory overhead. Instead, NamedNodeMap and Attr are created
// on demand and proxy the content of the actual attributes.
class NamedNodeMap : public script::Wrappable,
                     public base::SupportsWeakPtr<NamedNodeMap> {
 public:
  explicit NamedNodeMap(const scoped_refptr<Element>& element);

  // Web API: NamedNodeMap
  //
  unsigned int length() const;

  scoped_refptr<Attr> Item(unsigned int item);
  scoped_refptr<Attr> GetNamedItem(const std::string& name) const;
  scoped_refptr<Attr> SetNamedItem(const scoped_refptr<Attr>& attribute);
  scoped_refptr<Attr> RemoveNamedItem(const std::string& name);

  bool CanQueryNamedProperty(const std::string& name) const;
  void EnumerateNamedProperties(script::PropertyEnumerator* enumerator) const;

  // Custom, not in any spec.
  //
  void SetAttributeInternal(const std::string& name, const std::string& value);
  void RemoveAttributeInternal(const std::string& name);

  scoped_refptr<Element> element() const;

  DEFINE_WRAPPABLE_TYPE(NamedNodeMap);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~NamedNodeMap();

  scoped_refptr<Attr> GetOrCreateAttr(const std::string& name) const;

  typedef base::hash_map<std::string, base::WeakPtr<Attr> > NameToAttrMap;

  // The element that contains the actual attributes.
  scoped_refptr<Element> element_;
  // Vector of weak pointers to Attr objects that proxies the actual attributes.
  mutable NameToAttrMap proxy_attributes_;

  DISALLOW_COPY_AND_ASSIGN(NamedNodeMap);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NAMED_NODE_MAP_H_
