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

#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/browser/loader/text_load.h"
#include "cobalt/script/script_runner.h"

namespace cobalt {
namespace dom {

// The script element allows authors to include dynamic script and data blocks
// in their documents.
//    http://www.w3.org/TR/html/scripting-1.html#the-script-element
class HTMLScriptElement : public HTMLElement {
 public:
  static const char* kTagName;

  static scoped_refptr<HTMLScriptElement> Create(
      browser::ResourceLoaderFactory* loader_factory,
      script::ScriptRunner* script_runner);

  // Web API: Element
  //
  const std::string& tag_name() const OVERRIDE;

  // Web API: HTMLScriptElement
  //
  bool async() const { return GetBooleanAttribute("async"); }
  void set_async(bool value) { SetBooleanAttribute("async", value); }

  std::string src() const { return GetAttribute("src").value_or(""); }
  void set_src(const std::string& value) { SetAttribute("src", value); }

  std::string type() const { return GetAttribute("type").value_or(""); }
  void set_type(const std::string& value) { SetAttribute("type", value); }

  std::string text() const;

  // Custom, not in any spec.
  //
  scoped_refptr<HTMLScriptElement> AsHTMLScriptElement() OVERRIDE {
    return this;
  }

 protected:
  // From Node.
  void AttachToDocument(Document* document) OVERRIDE;

 private:
  HTMLScriptElement(browser::ResourceLoaderFactory* loader_factory,
                    script::ScriptRunner* script_runner);
  ~HTMLScriptElement() OVERRIDE;

  // From the spec: HTMLScriptElement.
  void Prepare();

  void OnLoadingDone(const std::string& content);
  void OnLoadingError(const browser::ResourceLoaderError& error);
  bool IsLoading() const;
  void StopLoading();

  // ResourceLoaderFactory that is used to create a byte loader.
  browser::ResourceLoaderFactory* loader_factory_;
  // Proxy to JavaScript Global Object in which scripts should be run
  script::ScriptRunner* script_runner_;
  // This object is responsible for the loading.
  scoped_ptr<browser::TextLoad> text_load_;
  // Whether the script has been started.
  bool is_already_started_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_SCRIPT_ELEMENT_H_
