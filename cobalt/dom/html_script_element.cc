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

#include "cobalt/dom/html_script_element.h"

#include <deque>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/string_util.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/sync_loader.h"
#include "cobalt/loader/text_decoder.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_runner.h"
#include "googleurl/src/gurl.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {

namespace {

bool PermitAnyURL(const GURL&, bool) { return true; }

loader::RequestMode GetRequestMode(
    const base::optional<std::string>& cross_origin_attribute) {
  // https://html.spec.whatwg.org/#cors-settings-attribute
  if (cross_origin_attribute) {
    if (*cross_origin_attribute == "use-credentials") {
      return loader::kCORSModeIncludeCredentials;
    } else {
      // The invalid value default of crossorigin is Anonymous state, leading to
      // "same-origin" credentials mode.
      return loader::kCORSModeSameOriginCredentials;
    }
  }
  // crossorigin attribute's missing value default is No CORS state, leading to
  // "no-cors" request mode.
  return loader::kNoCORSMode;
}
}  // namespace

// static
const char HTMLScriptElement::kTagName[] = "script";

HTMLScriptElement::HTMLScriptElement(Document* document)
    : HTMLElement(document, base::Token(kTagName)),
      is_already_started_(false),
      is_parser_inserted_(false),
      is_ready_(false),
      load_option_(0),
      inline_script_location_(GetSourceLocationName(), 1, 1),
      is_sync_load_successful_(false),
      prevent_garbage_collection_count_(0),
      should_execute_(true) {
  DCHECK(document->html_element_context()->script_runner());
}

base::optional<std::string> HTMLScriptElement::cross_origin() const {
  base::optional<std::string> cross_origin_attribute =
      GetAttribute("crossOrigin");
  if (cross_origin_attribute &&
      (*cross_origin_attribute != "anonymous" &&
       *cross_origin_attribute != "use-credentials")) {
    return std::string();
  }
  return cross_origin_attribute;
}

void HTMLScriptElement::set_cross_origin(
    const base::optional<std::string>& value) {
  if (value) {
    SetAttribute("crossOrigin", *value);
  } else {
    RemoveAttribute("crossOrigin");
  }
}

void HTMLScriptElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  if (!is_parser_inserted_) {
    Prepare();
  }
}

void HTMLScriptElement::OnParserStartTag(
    const base::SourceLocation& opening_tag_location) {
  inline_script_location_ = opening_tag_location;
  ++inline_script_location_.column_number;  // JavaScript code starts after ">".
  is_parser_inserted_ = true;
}

void HTMLScriptElement::OnParserEndTag() { Prepare(); }

scoped_refptr<HTMLScriptElement> HTMLScriptElement::AsHTMLScriptElement() {
  return this;
}

scoped_refptr<Node> HTMLScriptElement::Duplicate() const {
  // The cloning steps for script elements must set the "already started" flag
  // on the copy if it is set on the element being cloned.
  //   https://www.w3.org/TR/html5/scripting-1.html#already-started
  scoped_refptr<HTMLScriptElement> new_script = HTMLElement::Duplicate()
                                                    ->AsElement()
                                                    ->AsHTMLElement()
                                                    ->AsHTMLScriptElement();
  new_script->is_already_started_ = is_already_started_;
  return new_script;
}

HTMLScriptElement::~HTMLScriptElement() {
  // Remove the script from the list of scripts that will execute in order as
  // soon as possible associated with the Document, only if the document still
  // exists when the script element is destroyed.
  // NOTE: While the garbage collection prevention logic will typically protect
  // from this, it is still possible during shutdown.
  if (node_document()) {
    std::deque<HTMLScriptElement*>* scripts_to_be_executed =
        node_document()->scripts_to_be_executed();

    std::deque<HTMLScriptElement*>::iterator it = std::find(
        scripts_to_be_executed->begin(), scripts_to_be_executed->end(), this);
    if (it != scripts_to_be_executed->end()) {
      scripts_to_be_executed->erase(it);
    }
  }
}

// Algorithm for Prepare:
//   https://www.w3.org/TR/html5/scripting-1.html#prepare-a-script
void HTMLScriptElement::Prepare() {
  TRACK_MEMORY_SCOPE("DOM");
  // Custom, not in any spec.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(MessageLoop::current());
  DCHECK(!loader_);
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::Prepare()");

  // If the script element is marked as having "already started", then the user
  // agent must abort these steps at this point. The script is not executed.
  if (is_already_started_) {
    return;
  }

  // 2. 3. 4. Not needed by Cobalt.

  // 5. If the element is not in a Document, then the user agent must abort
  // these steps at this point. The script is not executed.
  if (!node_document()) {
    return;
  }
  document_ = base::AsWeakPtr<Document>(node_document());

  // 6. If either:
  //    the script element has a type attribute and its value is the empty
  //      string, or
  //    the script element has no type attribute but it has a language attribute
  //      and that attribute's value is the empty string, or
  //    the script element has neither a type attribute nor a language
  //      attribute, then
  // ...let the script block's type for this script element be
  // "text/javascript".
  // Otherwise, if the script element has a type attribute, let the script
  // block's type for this script element be the value of that attribute with
  // any leading or trailing sequences of space characters removed.
  // Otherwise, the element has a non-empty language attribute; let the script
  // block's type for this script element be the concatenation of the string
  // "text/" followed by the value of the language attribute.
  if (type() == "") {
    set_type("text/javascript");
  } else {
    std::string trimmed_type;
    TrimWhitespace(type(), TRIM_ALL, &trimmed_type);
    set_type(trimmed_type);
  }

  // 7. If the user agent does not support the scripting language given by the
  // script block's type for this script element, then the user agent must abort
  // these steps at this point. The script is not executed.
  if (type() != "text/javascript") return;

  // 8. Not needed by Cobalt.

  // 9. The user agent must set the element's "already started" flag.
  is_already_started_ = true;

  // 10. ~ 13. Not needed by Cobalt.

  // 14. If the element has a src content attribute, run these substeps:
  //   1. Let src be the value of the element's src attribute.
  //   2. If src is the empty string, queue a task to fire a simple event
  // named error at the element, and abort these steps.
  if (HasAttribute("src") && src() == "") {
    LOG(WARNING) << "src attribute of script element is empty.";

    PreventGarbageCollectionAndPostToDispatchEvent(FROM_HERE,
                                                   base::Tokens::error());
    return;
  }

  //   3. Resolve src relative to the element.
  //   4. If the previous step failed, queue a task to fire a simple event named
  // error at the element, and abort these steps.
  const GURL& base_url = document_->url_as_gurl();
  url_ = base_url.Resolve(src());
  if (!url_.is_valid()) {
    LOG(WARNING) << src() << " cannot be resolved based on " << base_url << ".";

    PreventGarbageCollectionAndPostToDispatchEvent(FROM_HERE,
                                                   base::Tokens::error());
    return;
  }

  //   5. Do a potentially CORS-enabled fetch of the resulting absolute URL,
  // with the mode being the current state of the element's crossorigin
  // content attribute, the origin being the origin of the script element's
  // Document, and the default origin behaviour set to taint.

  // 15. Then, the first of the following options that describes the situation
  // must be followed:

  // Option 1 and Option 3 are not needed by Cobalt.
  if (HasAttribute("src") && is_parser_inserted_ && !async()) {
    // Option 2
    // If the element has a src attribute, and the element has been flagged as
    // "parser-inserted", and the element does not have an async attribute.
    load_option_ = 2;
  } else if (HasAttribute("src") && !async()) {
    // Option 4
    // If the element has a src attribute, does not have an async attribute, and
    // does not have the "force-async" flag set.
    load_option_ = 4;
  } else if (HasAttribute("src")) {
    // Option 5
    // If the element has a src attribute.
    load_option_ = 5;
  } else {
    // Option 6
    // Otherwise.
    load_option_ = 6;
  }

  // https://www.w3.org/TR/CSP2/#directive-script-src

  CspDelegate* csp_delegate = document_->csp_delegate();
  // If the script element has a valid nonce, we always permit it, regardless
  // of its URL or inline nature.
  const bool bypass_csp =
      csp_delegate->IsValidNonce(CspDelegate::kScript, nonce());

  csp::SecurityCallback csp_callback;
  if (bypass_csp) {
    csp_callback = base::Bind(&PermitAnyURL);
  } else {
    csp_callback =
        base::Bind(&CspDelegate::CanLoad, base::Unretained(csp_delegate),
                   CspDelegate::kScript);
  }

  // Clear fetched resource's origin before start.
  fetched_last_url_origin_ = loader::Origin();

  // Determine request mode from crossorigin attribute.
  request_mode_ = GetRequestMode(GetAttribute("crossOrigin"));

  switch (load_option_) {
    case 2: {
      // If the element has a src attribute, and the element has been flagged as
      // "parser-inserted", and the element does not have an async attribute.

      // The element is the pending parsing-blocking script of the Document of
      // the parser that created the element. (There can only be one such script
      // per Document at a time.)
      is_sync_load_successful_ = false;

      loader::LoadSynchronously(
          html_element_context()->sync_load_thread()->message_loop(),
          base::Bind(
              &loader::FetcherFactory::CreateSecureFetcher,
              base::Unretained(html_element_context()->fetcher_factory()), url_,
              csp_callback, request_mode_,
              document_->location() ? document_->location()->OriginObject()
                                    : loader::Origin()),
          base::Bind(&loader::TextDecoder::Create,
                     base::Bind(&HTMLScriptElement::OnSyncLoadingDone,
                                base::Unretained(this))),
          base::Bind(&HTMLScriptElement::OnSyncLoadingError,
                     base::Unretained(this)));

      if (is_sync_load_successful_) {
        PreventGarbageCollection();
        ExecuteExternal();
        AllowGarbageCollection();
      } else {
        // Executing the script block must just consist of firing a simple event
        // named error at the element.
        DispatchEvent(new Event(base::Tokens::error()));
      }
    } break;
    case 4: {
      // This is an asynchronous script. Prevent garbage collection until
      // loading completes and the script potentially executes.
      PreventGarbageCollection();

      // If the element has a src attribute, does not have an async attribute,
      // and does not have the "force-async" flag set.

      // The element must be added to the end of the list of scripts that will
      // execute in order as soon as possible associated with the Document of
      // the script element at the time the prepare a script algorithm started.
      std::deque<HTMLScriptElement*>* scripts_to_be_executed =
          document_->scripts_to_be_executed();
      scripts_to_be_executed->push_back(this);

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      document_->IncreaseLoadingCounter();

      loader_.reset(new loader::Loader(
          base::Bind(
              &loader::FetcherFactory::CreateSecureFetcher,
              base::Unretained(html_element_context()->fetcher_factory()), url_,
              csp_callback, request_mode_,
              document_->location() ? document_->location()->OriginObject()
                                    : loader::Origin()),
          scoped_ptr<loader::Decoder>(new loader::TextDecoder(base::Bind(
              &HTMLScriptElement::OnLoadingDone, base::Unretained(this)))),
          base::Bind(&HTMLScriptElement::OnLoadingError,
                     base::Unretained(this))));
    } break;
    case 5: {
      // This is an asynchronous script. Prevent garbage collection until
      // loading completes and the script potentially executes.
      PreventGarbageCollection();

      // If the element has a src attribute.

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      document_->IncreaseLoadingCounter();

      // The element must be added to the set of scripts that will execute as
      // soon as possible of the Document of the script element at the time the
      // prepare a script algorithm started.
      loader_.reset(new loader::Loader(
          base::Bind(
              &loader::FetcherFactory::CreateSecureFetcher,
              base::Unretained(html_element_context()->fetcher_factory()), url_,
              csp_callback, request_mode_,
              document_->location() ? document_->location()->OriginObject()
                                    : loader::Origin()),
          scoped_ptr<loader::Decoder>(new loader::TextDecoder(base::Bind(
              &HTMLScriptElement::OnLoadingDone, base::Unretained(this)))),
          base::Bind(&HTMLScriptElement::OnLoadingError,
                     base::Unretained(this))));
    } break;
    case 6: {
      // Otherwise.

      // The user agent must immediately execute the script block, even if other
      // scripts are already executing.
      base::optional<std::string> content = text_content();
      const std::string& text = content.value_or(EmptyString());
      if (bypass_csp || text.empty() ||
          csp_delegate->AllowInline(CspDelegate::kScript,
                                    inline_script_location_,
                                    text)) {
        fetched_last_url_origin_ = document_->location()->OriginObject();
        ExecuteInternal();
      } else {
        PreventGarbageCollectionAndPostToDispatchEvent(FROM_HERE,
                                                       base::Tokens::error());
      }
    } break;
    default: { NOTREACHED(); }
  }
}

void HTMLScriptElement::OnSyncLoadingDone(
    const std::string& content, const loader::Origin& last_url_origin) {
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::OnSyncLoadingDone()");
  content_ = content;
  is_sync_load_successful_ = true;
  fetched_last_url_origin_ = last_url_origin;
}

void HTMLScriptElement::OnSyncLoadingError(const std::string& error) {
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::OnSyncLoadingError()");
  LOG(ERROR) << error;
}

// Algorithm for OnLoadingDone:
//   https://www.w3.org/TR/html5/scripting-1.html#prepare-a-script
void HTMLScriptElement::OnLoadingDone(const std::string& content,
                                      const loader::Origin& last_url_origin) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(load_option_ == 4 || load_option_ == 5);
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::OnLoadingDone()");
  if (!document_) {
    AllowGarbageCollection();
    return;
  }
  fetched_last_url_origin_ = last_url_origin;

  content_ = content;
  switch (load_option_) {
    case 4: {
      // If the element has a src attribute, does not have an async attribute,
      // and does not have the "force-async" flag set.

      // The task that the networking task source places on the task queue once
      // the fetching algorithm has completed must run the following steps:
      //   1. If the element is not now the first element in the list of scripts
      //   that will execute in order as soon as possible to which it was added
      //   above, then mark the element as ready but abort these steps without
      //   executing the script yet.
      std::deque<HTMLScriptElement*>* scripts_to_be_executed =
          document_->scripts_to_be_executed();
      if (scripts_to_be_executed->front() != this) {
        is_ready_ = true;
        return;
      }
      while (true) {
        // 2. Execution: Execute the script block corresponding to the first
        // script element in this list of scripts that will execute in order as
        // soon as possible.
        HTMLScriptElement* script = scripts_to_be_executed->front();
        script->ExecuteExternal();

        // NOTE: Must disable warning 6011 on Windows. It mysteriously believes
        // that a NULL pointer is being dereferenced with this comparison.
        MSVC_PUSH_DISABLE_WARNING(6011);
        // If this script isn't the current object, then allow it to be garbage
        // collected now that it has executed.
        if (script != this) {
          script->AllowGarbageCollection();
        }
        MSVC_POP_WARNING();

        // 3. Remove the first element from this list of scripts that will
        // execute in order as soon as possible.
        scripts_to_be_executed->pop_front();

        // Fetching an external script must delay the load event of the
        //  element's document until the task that is queued by the networking
        // task source once the resource has been fetched (defined above)
        // has been run.
        document_->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();

        // 4. If this list of scripts that will execute in order as soon as
        // possible is still not empty and the first entry has already been
        // marked
        // as ready, then jump back to the step labeled execution.
        if (scripts_to_be_executed->empty() ||
            !scripts_to_be_executed->front()->is_ready_) {
          break;
        }
      }
      // Allow garbage collection on the current script object now that it has
      // finished executing both itself and other pending scripts.
      AllowGarbageCollection();
    } break;
    case 5: {
      // If the element has a src attribute.

      // The task that the networking task source places on the task queue once
      // the fetching algorithm has completed must execute the script block and
      // then remove the element from the set of scripts that will execute as
      // soon as possible.
      ExecuteExternal();

      // Allow garbage collection on the current object now that it has finished
      // executing.
      AllowGarbageCollection();

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      document_->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
    } break;
  }
}

// Algorithm for OnLoadingError:
//   https://www.w3.org/TR/html5/scripting-1.html#prepare-a-script
void HTMLScriptElement::OnLoadingError(const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(load_option_ == 4 || load_option_ == 5);
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::OnLoadingError()");

  if (!document_) {
    AllowGarbageCollection();
    return;
  }

  LOG(ERROR) << error;

  // Executing the script block must just consist of firing a simple event
  // named error at the element.
  DCHECK_GT(prevent_garbage_collection_count_, 0);
  DispatchEvent(new Event(base::Tokens::error()));
  AllowGarbageCollection();

  switch (load_option_) {
    case 4: {
      // If the element has a src attribute, does not have an async attribute,
      // and does not have the "force-async" flag set.
      std::deque<HTMLScriptElement*>* scripts_to_be_executed =
          document_->scripts_to_be_executed();

      std::deque<HTMLScriptElement*>::iterator it = std::find(
          scripts_to_be_executed->begin(), scripts_to_be_executed->end(), this);
      if (it != scripts_to_be_executed->end()) {
        scripts_to_be_executed->erase(it);
      }
    } break;
    case 5: {
      // If the element has a src attribute.
    } break;
  }

  // Fetching an external script must delay the load event of the element's
  // document until the task that is queued by the networking task source
  // once the resource has been fetched (defined above) has been run.
  document_->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
}

// Algorithm for Execute:
//   https://www.w3.org/TR/html5/scripting-1.html#execute-the-script-block
void HTMLScriptElement::Execute(const std::string& content,
                                const base::SourceLocation& script_location,
                                bool is_external) {
  TRACK_MEMORY_SCOPE("DOM");
  // If should_execute_ is set to false for whatever reason, then we
  // immediately exit.
  // When inserted using the document.write() method, script elements execute
  // (typically synchronously), but when inserted using innerHTML and
  // outerHTML attributes, they do not execute at all.
  // https://www.w3.org/TR/html5/scripting-1.html#the-script-element.
  if (!should_execute_) {
    return;
  }

  // The script is now being run. Track it in the global stats.
  GlobalStats::GetInstance()->StartJavaScriptEvent();

  TRACE_EVENT2("cobalt::dom", "HTMLScriptElement::Execute()", "file_path",
               script_location.file_path, "line_number",
               script_location.line_number);
  // Since error is already handled, it is guaranteed the load is successful.

  // 1. 2. 3. Not needed by Cobalt.

  // 4. Create a script, using the script block's source, the URL from which the
  // script was obtained, the script block's type as the scripting language, and
  // the script settings object of the script element's Document's Window
  // object.
  bool mute_errors =
      request_mode_ == loader::kNoCORSMode &&
      fetched_last_url_origin_ != document_->location()->OriginObject();
  html_element_context()->script_runner()->Execute(
      content, script_location, mute_errors, NULL /*out_succeeded*/);

  // 5. 6. Not needed by Cobalt.

  // 7. If the script is from an external file, fire a simple event named load
  // at the script element.
  // Otherwise, the script is internal; queue a task to fire a simple event
  // named load at the script element.
  // TODO: Remove the firing of readystatechange once we support Promise.
  if (is_external) {
    DCHECK_GT(prevent_garbage_collection_count_, 0);
    DispatchEvent(new Event(base::Tokens::load()));
    DispatchEvent(new Event(base::Tokens::readystatechange()));
  } else {
    PreventGarbageCollectionAndPostToDispatchEvent(FROM_HERE,
                                                   base::Tokens::load());
    PreventGarbageCollectionAndPostToDispatchEvent(
        FROM_HERE, base::Tokens::readystatechange());
  }

  // The script is done running. Stop tracking it in the global stats.
  GlobalStats::GetInstance()->StopJavaScriptEvent();

  // Notify the DomStatTracker of the execution.
  dom_stat_tracker_->OnHtmlScriptElementExecuted();
}

void HTMLScriptElement::PreventGarbageCollectionAndPostToDispatchEvent(
    const tracked_objects::Location& location, const base::Token& token) {
  // Ensure that this HTMLScriptElement is not garbage collected until the event
  // has been processed.
  PreventGarbageCollection();
  PostToDispatchEventAndRunCallback(
      location, token,
      base::Bind(&HTMLScriptElement::AllowGarbageCollection, this));
}

void HTMLScriptElement::PreventGarbageCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(prevent_garbage_collection_count_, 0);
  if (prevent_garbage_collection_count_++ == 0) {
    DCHECK(html_element_context()->script_runner());
    DCHECK(html_element_context()->script_runner()->GetGlobalEnvironment());
    html_element_context()
        ->script_runner()
        ->GetGlobalEnvironment()
        ->PreventGarbageCollection(make_scoped_refptr(this));
  }
}

void HTMLScriptElement::AllowGarbageCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(prevent_garbage_collection_count_, 0);
  if (--prevent_garbage_collection_count_ == 0) {
    DCHECK(html_element_context()->script_runner());
    DCHECK(html_element_context()->script_runner()->GetGlobalEnvironment());
    html_element_context()
        ->script_runner()
        ->GetGlobalEnvironment()
        ->AllowGarbageCollection(make_scoped_refptr(this));
  }
}

}  // namespace dom
}  // namespace cobalt
