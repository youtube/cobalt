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

#ifndef COBALT_DOM_HTML_ELEMENT_FACTORY_H_
#define COBALT_DOM_HTML_ELEMENT_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/token.h"

namespace cobalt {
namespace dom {

class Document;
class HTMLElement;

// This factory is responsible for creating HTML elements according to tag name.
class HTMLElementFactory {
 public:
  HTMLElementFactory();
  ~HTMLElementFactory();

  scoped_refptr<HTMLElement> CreateHTMLElement(Document* document,
                                               base::Token tag_name);

 private:
  typedef base::Callback<scoped_refptr<HTMLElement>(Document* document)>
      CreateHTMLElementTCallback;
  typedef base::hash_map<base::Token, CreateHTMLElementTCallback>
      TagNameToCreateHTMLElementTCallbackMap;

  // Helper function templates for adding entries to the map below.
  template <typename T>
  void RegisterHTMLElementWithSingleTagName();
  template <typename T>
  void RegisterHTMLElementWithMultipleTagName();

  TagNameToCreateHTMLElementTCallbackMap
      tag_name_to_create_html_element_t_callback_map_;

  DISALLOW_COPY_AND_ASSIGN(HTMLElementFactory);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_ELEMENT_FACTORY_H_
