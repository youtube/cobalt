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

#ifndef COBALT_DOM_ATTR_H_
#define COBALT_DOM_ATTR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Element;
class NamedNodeMap;

// This type represents a DOM element's attribute as an object.
//    https://www.w3.org/TR/2014/WD-dom-20140710/#interface-attr
//
// Starting from DOM4, Attr no longer inherits from Node.
//
// This class is designed to proxy the actual attribute value which is stored
// in the Element and be created only on demand.
class Attr : public script::Wrappable,
             public base::SupportsWeakPtr<Attr> {
 public:
  // If container is NULL, the Attr will be created in a detached state.
  Attr(const std::string& name, const std::string& value,
       const scoped_refptr<const NamedNodeMap>& container);

  // Web API: Attr
  const std::string& name() const { return name_; }
  const std::string& node_name() const {
    // TODO: Report deprecated attribute.
    return name_;
  }

  const std::string& value() const { return value_; }
  void set_value(const std::string& value);

  const std::string& node_value() const;

  DEFINE_WRAPPABLE_TYPE(Attr);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~Attr();

  // Used by NamedNodeMap to set the value of Attr without triggering an
  // update in the proxied Element.
  void SetValueInternal(const std::string& value) { value_ = value; }

  // Used by NamedNodeMap to attach the Attr to itself.
  scoped_refptr<const NamedNodeMap> container() const;
  void set_container(const scoped_refptr<const NamedNodeMap>& value);

  // Name of the attribute.
  const std::string name_;
  // Value of the attribute.
  std::string value_;
  // Pointer to the NamedNodeMap that contains this Attr.
  scoped_refptr<const NamedNodeMap> container_;

  friend class NamedNodeMap;
  DISALLOW_COPY_AND_ASSIGN(Attr);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ATTR_H_
