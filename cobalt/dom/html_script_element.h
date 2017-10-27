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

#ifndef COBALT_DOM_HTML_SCRIPT_ELEMENT_H_
#define COBALT_DOM_HTML_SCRIPT_ELEMENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/source_location.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/loader/loader.h"

namespace cobalt {
namespace dom {

// The script element allows authors to include dynamic script and data blocks
// in their documents.
//   https://www.w3.org/TR/html5/scripting-1.html#the-script-element
class HTMLScriptElement : public HTMLElement {
 public:
  static const char kTagName[];

  explicit HTMLScriptElement(Document* document);

  // Web API: HTMLScriptElement
  //
  std::string src() const { return GetAttribute("src").value_or(""); }
  void set_src(const std::string& value) { SetAttribute("src", value); }

  std::string type() const { return GetAttribute("type").value_or(""); }
  void set_type(const std::string& value) { SetAttribute("type", value); }

  std::string charset() const { return GetAttribute("charset").value_or(""); }
  void set_charset(const std::string& value) { SetAttribute("charset", value); }

  bool async() const { return GetBooleanAttribute("async"); }
  void set_async(bool value) { SetBooleanAttribute("async", value); }

  base::optional<std::string> cross_origin() const;
  void set_cross_origin(const base::optional<std::string>& value);

  std::string nonce() const { return GetAttribute("nonce").value_or(""); }
  void set_nonce(const std::string& value) { SetAttribute("nonce", value); }

  const EventListenerScriptValue* onreadystatechange() const {
    return GetAttributeEventListener(base::Tokens::readystatechange());
  }
  void set_onreadystatechange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::readystatechange(), event_listener);
  }

  void set_should_execute(bool should_execute) {
    should_execute_ = should_execute;
  }

  // Custom, not in any spec.
  //
  // From Node.
  void OnInsertedIntoDocument() OVERRIDE;

  // From Element.
  void OnParserStartTag(
      const base::SourceLocation& opening_tag_location) OVERRIDE;
  void OnParserEndTag() OVERRIDE;

  // From HTMLElement.
  scoped_refptr<HTMLScriptElement> AsHTMLScriptElement() OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(HTMLScriptElement);

 protected:
  scoped_refptr<Node> Duplicate() const OVERRIDE;

 private:
  ~HTMLScriptElement() OVERRIDE;

  // From the spec: HTMLScriptElement.
  //
  void Prepare();

  void OnSyncLoadingDone(const std::string& content,
                         const loader::Origin& last_url_origin);
  void OnSyncLoadingError(const std::string& error);

  void OnLoadingDone(const std::string& content,
                     const loader::Origin& last_url_origin);
  void OnLoadingError(const std::string& error);

  void ExecuteExternal() {
    Execute(content_, base::SourceLocation(url_.spec(), 1, 1), true);
  }
  void ExecuteInternal() {
    Execute(text_content().value(), inline_script_location_, false);
  }
  void Execute(const std::string& content,
               const base::SourceLocation& script_location, bool is_external);

  void PreventGarbageCollectionAndPostToDispatchEvent(
      const tracked_objects::Location& location, const base::Token& token);
  void PreventGarbageCollection();
  void AllowGarbageCollection();

  // Whether the script has been started.
  bool is_already_started_;
  // Whether the script element is inserted by parser.
  bool is_parser_inserted_;
  // Whether the script is ready to be executed.
  bool is_ready_;
  // The option that defines how the script should be loaded and executed.
  int load_option_;
  // SourceLocation for inline script.
  base::SourceLocation inline_script_location_;

  // Thread checker ensures all calls to DOM element are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;
  // Weak reference to the document at the time Prepare() started.
  base::WeakPtr<Document> document_;
  // The loader that is used for asynchronous loads.
  scoped_ptr<loader::Loader> loader_;
  // Whether the sync load is successful.
  bool is_sync_load_successful_;
  // Resolved URL of the script.
  GURL url_;
  // Content of the script.
  std::string content_;
  // Active requests disabling garbage collection.
  int prevent_garbage_collection_count_;

  // Whether or not the script should execute at all.
  bool should_execute_;

  // The request mode for the fetch request.
  loader::RequestMode request_mode_;

  // Will be compared with document's origin to derive mute_errors flag
  // javascript parser takes in to record if the error reqort should be muted
  // due to cross-origin fetched script.
  loader::Origin fetched_last_url_origin_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_SCRIPT_ELEMENT_H_
