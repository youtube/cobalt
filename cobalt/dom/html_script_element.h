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

#ifndef COBALT_DOM_HTML_SCRIPT_ELEMENT_H_
#define COBALT_DOM_HTML_SCRIPT_ELEMENT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/source_location.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/loader/loader.h"
#include "cobalt/script/global_environment.h"

namespace cobalt {
namespace dom {

// The script element allows authors to include dynamic script and data blocks
// in their documents.
//   https://www.w3.org/TR/html50/scripting-1.html#the-script-element
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

  base::Optional<std::string> cross_origin() const;
  void set_cross_origin(const base::Optional<std::string>& value);

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
  void OnInsertedIntoDocument() override;

  // From Element.
  void OnParserStartTag(
      const base::SourceLocation& opening_tag_location) override;
  void OnParserEndTag() override;

  // From HTMLElement.
  scoped_refptr<HTMLScriptElement> AsHTMLScriptElement() override;

  // Create Performance Resource Timing entry for script element.
  void GetLoadTimingInfoAndCreateResourceTiming();

  DEFINE_WRAPPABLE_TYPE(HTMLScriptElement);

 protected:
  scoped_refptr<Node> Duplicate() const override;

 private:
  ~HTMLScriptElement() override;

  // From the spec: HTMLScriptElement.
  //
  void Prepare();

  void OnSyncContentProduced(const loader::Origin& last_url_origin,
                             std::unique_ptr<std::string> content);
  void OnSyncLoadingComplete(const base::Optional<std::string>& error);

  void OnContentProduced(const loader::Origin& last_url_origin,
                         std::unique_ptr<std::string> content);
  void OnLoadingComplete(const base::Optional<std::string>& error);

  void ExecuteExternal();
  void ExecuteInternal();
  void Execute(const std::string& content,
               const base::SourceLocation& script_location, bool is_external);

  void PreventGarbageCollectionAndPostToDispatchEvent(
      const base::Location& location, const base::Token& token,
      std::unique_ptr<
          script::GlobalEnvironment::ScopedPreventGarbageCollection>*
          scoped_prevent_gc);
  void AllowGCAfterEventDispatch(
      std::unique_ptr<
          script::GlobalEnvironment::ScopedPreventGarbageCollection>*
          scoped_prevent_gc);
  void PreventGCUntilLoadComplete();
  void AllowGCAfterLoadComplete();
  void ReleaseLoader();

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
  THREAD_CHECKER(thread_checker_);
  // Weak reference to the document at the time Prepare() started.
  base::WeakPtr<Document> document_;
  // The loader that is used for asynchronous loads.
  std::unique_ptr<loader::Loader> loader_;
  // Whether the sync load is successful.
  bool is_sync_load_successful_;
  // Resolved URL of the script.
  GURL url_;
  // Content of the script. Released after Execute is called.
  std::unique_ptr<std::string> content_;

  // Whether or not the script should execute at all.
  bool should_execute_;

  // The request mode for the fetch request.
  loader::RequestMode request_mode_;

  // Will be compared with document's origin to derive mute_errors flag
  // javascript parser takes in to record if the error report should be muted
  // due to cross-origin fetched script.
  loader::Origin fetched_last_url_origin_;

  base::WaitableEvent* synchronous_loader_interrupt_;

  std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
      prevent_gc_until_load_event_dispatch_;

  std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
      prevent_gc_until_ready_event_dispatch_;

  std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
      prevent_gc_until_error_event_dispatch_;

  std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
      prevent_gc_until_load_complete_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_SCRIPT_ELEMENT_H_
