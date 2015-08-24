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

#ifndef DOM_HTML_ELEMENT_FACTORY_H_
#define DOM_HTML_ELEMENT_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"

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
                                               const std::string& tag_name);

 private:
  typedef base::Callback<scoped_refptr<HTMLElement>(Document* document)>
      CreateHTMLElementTCallback;
  typedef base::hash_map<std::string, CreateHTMLElementTCallback>
      TagNameToCreateHTMLElementTCallbackMap;

  TagNameToCreateHTMLElementTCallbackMap
      tag_name_to_create_html_element_t_callback_map_;

  DISALLOW_COPY_AND_ASSIGN(HTMLElementFactory);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_ELEMENT_FACTORY_H_
