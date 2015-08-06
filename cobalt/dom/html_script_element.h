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

#ifndef DOM_HTML_SCRIPT_ELEMENT_H_
#define DOM_HTML_SCRIPT_ELEMENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/loader/loader.h"

namespace cobalt {
namespace dom {

// The script element allows authors to include dynamic script and data blocks
// in their documents.
//   http://www.w3.org/TR/html5/scripting-1.html#the-script-element
class HTMLScriptElement : public HTMLElement {
 public:
  static const char* kTagName;

  HTMLScriptElement(Document* document,
                    HTMLElementContext* html_element_context);

  // Web API: Element
  //
  std::string tag_name() const OVERRIDE;

  // Web API: HTMLScriptElement
  //
  bool async() const { return GetBooleanAttribute("async"); }
  void set_async(bool value) { SetBooleanAttribute("async", value); }

  std::string src() const { return GetAttribute("src").value_or(""); }
  void set_src(const std::string& value) { SetAttribute("src", value); }

  std::string type() const { return GetAttribute("type").value_or(""); }
  void set_type(const std::string& value) { SetAttribute("type", value); }

  // Custom, not in any spec.
  //
  scoped_refptr<HTMLScriptElement> AsHTMLScriptElement() OVERRIDE {
    return this;
  }

  // From Node.
  void OnInsertedIntoDocument() OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(HTMLScriptElement);

 private:
  ~HTMLScriptElement() OVERRIDE;

  // From the spec: HTMLScriptElement.
  void Prepare();

  void OnLoadingDone(const std::string& content);
  void OnLoadingError(const std::string& error);
  void StopLoading();

  // Thread checker ensures all calls to DOM element are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;
  // The loader.
  scoped_ptr<loader::Loader> loader_;
  // Whether the script has been started.
  bool is_already_started_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_SCRIPT_ELEMENT_H_
