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

#include "cobalt/dom/html_script_element.h"

#include <deque>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/console_log.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/sync_loader.h"
#include "cobalt/loader/text_decoder.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/web/csp_delegate.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

namespace {

bool PermitAnyURL(const GURL&, bool) { return true; }

loader::RequestMode GetRequestMode(
    const base::Optional<std::string>& cross_origin_attribute) {
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
      should_execute_(true),
      synchronous_loader_interrupt_(
          document->html_element_context()->synchronous_loader_interrupt()) {
  DCHECK(document->html_element_context()->script_runner());
}

std::string HTMLScriptElement::src() const {
  std::string src = GetAttribute("src").value_or("");
  if (src == "" || !node_document()) {
    return src;
  }
  GURL url = node_document()->location()->url().Resolve(src);
  return url.is_valid() ? url.spec() : src;
}

base::Optional<std::string> HTMLScriptElement::cross_origin() const {
  base::Optional<std::string> cross_origin_attribute =
      GetAttribute("crossOrigin");
  if (cross_origin_attribute &&
      (*cross_origin_attribute != "anonymous" &&
       *cross_origin_attribute != "use-credentials")) {
    return std::string();
  }
  return cross_origin_attribute;
}

void HTMLScriptElement::set_cross_origin(
    const base::Optional<std::string>& value) {
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return this;
}

scoped_refptr<Node> HTMLScriptElement::Duplicate() const {
  // The cloning steps for script elements must set the "already started" flag
  // on the copy if it is set on the element being cloned.
  //   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#already-started
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
  if (document_) {
    std::deque<HTMLScriptElement*>* scripts_to_be_executed =
        document_->scripts_to_be_executed();

    std::deque<HTMLScriptElement*>::iterator it = std::find(
        scripts_to_be_executed->begin(), scripts_to_be_executed->end(), this);
    if (it != scripts_to_be_executed->end()) {
      scripts_to_be_executed->erase(it);
    }
    ExecuteSyncScripts();
  }
}

// Algorithm for Prepare:
//   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#prepare-a-script
void HTMLScriptElement::Prepare() {
  // Custom, not in any spec.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(base::MessageLoop::current());
  DCHECK(!loader_ || is_already_started_);
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::Prepare()");

  error_.reset();

  // If the script element is marked as having "already started", then the
  // user agent must abort these steps at this point. The script is not
  // executed.
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
    TrimWhitespaceASCII(type(), base::TRIM_ALL, &trimmed_type);
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
  //
  // Need to use the "src" attribute. The |src| property is fully resolved. See
  // header file for details.
  auto src = GetAttribute("src").value_or("");
  if (HasAttribute("src") && src == "") {
    LOG(ERROR) << "src attribute of script element is empty.";

    PreventGarbageCollectionAndPostToDispatchEvent(
        FROM_HERE, base::Tokens::error(),
        &prevent_gc_until_error_event_dispatch_);
    return;
  }

  //   3. Resolve src relative to the element.
  //   4. If the previous step failed, queue a task to fire a simple event named
  // error at the element, and abort these steps.
  const GURL& base_url = document_->location()->url();
  url_ = base_url.Resolve(src);
  if (!url_.is_valid()) {
    LOG(ERROR) << src << " cannot be resolved based on " << base_url << ".";

    PreventGarbageCollectionAndPostToDispatchEvent(
        FROM_HERE, base::Tokens::error(),
        &prevent_gc_until_error_event_dispatch_);
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

    if (owner_document()
            ->html_element_context()
            ->enable_inline_script_warnings()) {
      CLOG(WARNING, debugger_hooks())
          << "A request to synchronously load a script is being made as "
             "a result of a non-async <script> tag inlined within HTML. "
             "You should avoid this in Cobalt because if the app is "
             "suspended while loading the script, Cobalt's current "
             "logic is to abort the load and without any retries or "
             "signals. To avoid difficult to diagnose suspend/resume "
             "bugs, it is recommended to use JavaScript to create a "
             "script element and load it async. The <script> reference "
             "appears at: \""
          << inline_script_location_ << "\" and its src is \"" << src << "\"";
    }

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

  web::CspDelegate* csp_delegate = document_->GetCSPDelegate();
  // If the script element has a valid nonce, we always permit it, regardless
  // of its URL or inline nature.
  const bool bypass_csp =
      csp_delegate->IsValidNonce(web::CspDelegate::kScript, nonce());

  csp::SecurityCallback csp_callback;
  if (bypass_csp) {
    csp_callback = base::Bind(&PermitAnyURL);
  } else {
    csp_callback =
        base::Bind(&web::CspDelegate::CanLoad, base::Unretained(csp_delegate),
                   web::CspDelegate::kScript);
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

      // This variable will be set to true in the completion callback for the
      // loader below.  If that completion callback never fires, the variable
      // will stay false.  This can happen if the loader was interrupted, or
      // failed for another reason.
      loader::LoadSynchronously(
          html_element_context()->sync_load_thread()->message_loop(),
          synchronous_loader_interrupt_,
          base::Bind(
              &loader::FetcherFactory::CreateSecureFetcher,
              base::Unretained(html_element_context()->fetcher_factory()), url_,
              /*main_resource=*/false, csp_callback, request_mode_,
              document_->location() ? document_->location()->GetOriginAsObject()
                                    : loader::Origin(),
              disk_cache::kUncompiledScript, net::HttpRequestHeaders(),
              /*skip_fetch_intercept=*/false),
          base::Bind(&loader::TextDecoder::Create,
                     base::Bind(&HTMLScriptElement::OnSyncContentProduced,
                                base::Unretained(this)),
                     loader::TextDecoder::ResponseStartedCallback(),
                     loader::Decoder::OnCompleteFunction()),
          base::Bind(&HTMLScriptElement::OnSyncLoadingComplete,
                     base::Unretained(this)));

      // This block exists to ensure that garbage collection is prevented while
      // executing the script.
      {
        script::GlobalEnvironment::ScopedPreventGarbageCollection
            scoped_prevent_gc(
                html_element_context()->script_runner()->GetGlobalEnvironment(),
                this);
        ExecuteExternal();
      }
    } break;
    case 4: {
      // This is an asynchronous script. Prevent garbage collection until
      // loading completes and the script potentially executes.
      PreventGCUntilLoadComplete();

      // If the element has a src attribute, does not have an async attribute,
      // and does not have the "force-async" flag set.

      // The element must be added to the end of the list of scripts that will
      // execute in order as soon as possible associated with the Document of
      // the script element at the time the prepare a script algorithm started.
      std::deque<HTMLScriptElement*>* scripts_to_be_executed =
          document_->scripts_to_be_executed();
      scripts_to_be_executed->push_back(this);

      // Fetching an external script must delay the load event
      // of the element's document until the task that is
      // queued by the networking task source once the resource
      // has been fetched (defined above) has been run.
      document_->IncreaseLoadingCounter();

      loader::Origin origin = document_->location()
                                  ? document_->location()->GetOriginAsObject()
                                  : loader::Origin();

      loader_ = html_element_context()->loader_factory()->CreateScriptLoader(
          url_, origin, csp_callback,
          base::Bind(&HTMLScriptElement::OnContentProduced,
                     base::Unretained(this)),
          base::Bind(&HTMLScriptElement::OnLoadingComplete,
                     base::Unretained(this)));
    } break;
    case 5: {
      // This is an asynchronous script. Prevent garbage collection until
      // loading completes and the script potentially executes.
      PreventGCUntilLoadComplete();

      // If the element has a src attribute.

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      document_->IncreaseLoadingCounter();

      // The element must be added to the set of scripts that will execute as
      // soon as possible of the Document of the script element at the time the
      // prepare a script algorithm started.
      DCHECK(!loader_);

      loader::Origin origin = document_->location()
                                  ? document_->location()->GetOriginAsObject()
                                  : loader::Origin();

      loader_ = html_element_context()->loader_factory()->CreateScriptLoader(
          url_, origin, csp_callback,
          base::Bind(&HTMLScriptElement::OnContentProduced,
                     base::Unretained(this)),
          base::Bind(&HTMLScriptElement::OnLoadingComplete,
                     base::Unretained(this)));
    } break;
    case 6: {
      // Otherwise.

      // The user agent must immediately execute the script block, even if other
      // scripts are already executing.
      base::Optional<std::string> content = text_content();
      const std::string& text = content.value_or(base::EmptyString());
      if (bypass_csp || text.empty() ||
          csp_delegate->AllowInline(web::CspDelegate::kScript,
                                    inline_script_location_, text)) {
        fetched_last_url_origin_ = document_->location()->GetOriginAsObject();
        ExecuteInternal();
      } else {
        LOG(ERROR) << " empty synchronous script.";
        PreventGarbageCollectionAndPostToDispatchEvent(
            FROM_HERE, base::Tokens::error(),
            &prevent_gc_until_error_event_dispatch_);
      }
    } break;
    default: {
      NOTREACHED();
    }
  }
}

void HTMLScriptElement::OnSyncContentProduced(
    const loader::Origin& last_url_origin,
    std::unique_ptr<std::string> content) {
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::OnSyncContentProduced()");
  fetched_last_url_origin_ = last_url_origin;
  content_ = std::move(content);
}

void HTMLScriptElement::OnSyncLoadingComplete(
    const base::Optional<std::string>& error) {
  error_ = error;
  if (!error) return;
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::OnSyncLoadingComplete()");
  LOG(ERROR) << "Error during synchronous script load referenced from \""
             << inline_script_location_ << "\" : " << *error;
}

// Algorithm for OnContentProduced:
//   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#prepare-a-script
void HTMLScriptElement::OnContentProduced(
    const loader::Origin& last_url_origin,
    std::unique_ptr<std::string> content) {
  // From Algorithm for OnContentProduced:
  //   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#prepare-a-script
  // 15. Then, the first of the following options that describes the situation
  // must be followed:
  //
  DCHECK(content);
  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::OnContentProduced()");

  fetched_last_url_origin_ = last_url_origin;
  content_ = std::move(content);

  OnReadyToExecute();
}

// Algorithm for OnLoadingComplete:
//   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#prepare-a-script
void HTMLScriptElement::OnLoadingComplete(
    const base::Optional<std::string>& error) {
  error_ = error;
  // GetLoadTimingInfo and create resource timing before loader released.
  GetLoadTimingInfoAndCreateResourceTiming();

  TRACE_EVENT0("cobalt::dom", "HTMLScriptElement::OnLoadingComplete()");

  if (!error_) return;

  OnReadyToExecute();
}

void HTMLScriptElement::ExecuteSyncScripts() {
  // From Algorithm for OnContentProduced:
  //   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#prepare-a-script
  DCHECK(document_);

  std::deque<HTMLScriptElement*>* scripts_to_be_executed =
      document_->scripts_to_be_executed();
  if (scripts_to_be_executed->empty() ||
      scripts_to_be_executed->front() != this) {
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
      script->AllowGCAfterLoadComplete();
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
  AllowGCAfterLoadComplete();
}

void HTMLScriptElement::OnReadyToExecute() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(load_option_ == 4 || load_option_ == 5);
  if (!document_) {
    error_ = "No document";
    AllowGCAfterLoadComplete();
    return;
  }

  switch (load_option_) {
    case 4: {
      // From Algorithm for OnContentProduced:
      //   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#script-processing-src-sync

      // If the element has a src attribute, does not have an async attribute,
      // and does not have the "force-async" flag set.

      // The task that the networking task source places on the task queue once
      // the fetching algorithm has completed must run the following steps:
      //   1. If the element is not now the first element in the list of scripts
      //   that will execute in order as soon as possible to which it was added
      //   above, then mark the element as ready but abort these steps without
      //   executing the script yet.
      if (document_->scripts_to_be_executed()->front() != this) {
        is_ready_ = true;
      } else {
        ExecuteSyncScripts();
      }
    } break;
    case 5: {
      // From Algorithm for OnContentProduced:
      //   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#script-processing-src

      // If the element has a src attribute.

      // The task that the networking task source places on the task queue once
      // the fetching algorithm has completed must execute the script block and
      // then remove the element from the set of scripts that will execute as
      // soon as possible.
      ExecuteExternal();

      // Allow garbage collection on the current object now that it has finished
      // executing.
      AllowGCAfterLoadComplete();

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      document_->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
    } break;
  }

  // Post a task to release the loader.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&HTMLScriptElement::ReleaseLoader, this));
}

void HTMLScriptElement::ExecuteExternal() {
  if (error_) {
    LOG(ERROR) << *error_;
    // If the load resulted in an error (for example a DNS error, or an HTTP
    // 404 error)
    // Executing the script block must just consist of firing a simple event
    // named error at the element.
    DispatchEvent(new web::Event(base::Tokens::error()));
  } else {
    DCHECK(content_);
    Execute(*content_, base::SourceLocation(url_.spec(), 1, 1), true);
  }

  // Release the content string now that we're finished with it.
  content_.reset();
}

void HTMLScriptElement::ExecuteInternal() {
  DCHECK(!error_);
  Execute(text_content().value(), inline_script_location_, false);
}

// Algorithm for Execute:
//   https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#execute-the-script-block
void HTMLScriptElement::Execute(const std::string& content,
                                const base::SourceLocation& script_location,
                                bool is_external) {
  // If should_execute_ is set to false for whatever reason, then we
  // immediately exit.
  // When inserted using the document.write() method, script elements execute
  // (typically synchronously), but when inserted using innerHTML and
  // outerHTML attributes, they do not execute at all.
  // https://www.w3.org/TR/2018/SPSD-html5-20180327/scripting-1.html#the-script-element.
  if (!should_execute_) {
    return;
  }

  TRACE_EVENT2("cobalt::dom", "HTMLScriptElement::Execute()", "file_path",
               script_location.file_path, "line_number",
               script_location.line_number);

  // Since error is already handled, it is guaranteed the load is successful.
  DCHECK(!error_);

  // The script is now being run. Track it in the global stats.
  GlobalStats::GetInstance()->StartJavaScriptEvent();

  // For |currentScript| logic, see
  // https://html.spec.whatwg.org/commit-snapshots/20a0ddc6841176adab93efefb76b23e0a1e6fa43/#execute-the-script-block
  scoped_refptr<HTMLScriptElement> old_current_script =
      node_document()->current_script();
  node_document()->set_current_script(this);

  // 1. 2. 3. Not needed by Cobalt.

  // 4. Create a script, using the script block's source, the URL from which the
  // script was obtained, the script block's type as the scripting language, and
  // the script settings object of the script element's Document's Window
  // object.
  bool mute_errors =
      request_mode_ == loader::kNoCORSMode &&
      fetched_last_url_origin_ != document_->location()->GetOriginAsObject();
  html_element_context()->script_runner()->Execute(
      content, script_location, mute_errors, NULL /*out_succeeded*/);

  // 5. 6. Not needed by Cobalt.

  // 7. If the script is from an external file, fire a simple event named load
  // at the script element.
  // Otherwise, the script is internal; queue a task to fire a simple event
  // named load at the script element.
  // TODO: Remove the firing of readystatechange once we support Promise.
  if (is_external) {
    DispatchEvent(new web::Event(base::Tokens::load()));
    DispatchEvent(new web::Event(base::Tokens::readystatechange()));
  } else {
    PreventGarbageCollectionAndPostToDispatchEvent(
        FROM_HERE, base::Tokens::load(),
        &prevent_gc_until_load_event_dispatch_);
    PreventGarbageCollectionAndPostToDispatchEvent(
        FROM_HERE, base::Tokens::readystatechange(),
        &prevent_gc_until_ready_event_dispatch_);
  }

  // For |currentScript| logic, see
  // https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-block.
  node_document()->set_current_script(old_current_script);

  // The script is done running. Stop tracking it in the global stats.
  GlobalStats::GetInstance()->StopJavaScriptEvent();

  // Notify the DomStatTracker of the execution.
  dom_stat_tracker_->OnHtmlScriptElementExecuted(content.size());
}

void HTMLScriptElement::PreventGarbageCollectionAndPostToDispatchEvent(
    const base::Location& location, const base::Token& token,
    std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>*
        scoped_prevent_gc) {
  // Ensure that this HTMLScriptElement is not garbage collected until the event
  // has been processed.
  DCHECK(!(*scoped_prevent_gc));
  scoped_prevent_gc->reset(
      new script::GlobalEnvironment::ScopedPreventGarbageCollection(
          html_element_context()->script_runner()->GetGlobalEnvironment(),
          this));
  PostToDispatchEventNameAndRunCallback(
      location, token,
      base::Bind(&HTMLScriptElement::AllowGCAfterEventDispatch, this,
                 scoped_prevent_gc));
}

void HTMLScriptElement::AllowGCAfterEventDispatch(
    std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>*
        scoped_prevent_gc) {
  scoped_prevent_gc->reset();
}

void HTMLScriptElement::PreventGCUntilLoadComplete() {
  DCHECK(!prevent_gc_until_load_complete_);
  prevent_gc_until_load_complete_.reset(
      new script::GlobalEnvironment::ScopedPreventGarbageCollection(
          html_element_context()->script_runner()->GetGlobalEnvironment(),
          this));
}

void HTMLScriptElement::AllowGCAfterLoadComplete() {
  prevent_gc_until_load_complete_.reset();
}

void HTMLScriptElement::ReleaseLoader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(loader_);
  loader_.reset();
}

void HTMLScriptElement::GetLoadTimingInfoAndCreateResourceTiming() {
  if (html_element_context()->performance() == nullptr) return;
  if (loader_) {
    html_element_context()->performance()->CreatePerformanceResourceTiming(
        loader_->get_load_timing_info(), kTagName, url_.spec());
  }
}

}  // namespace dom
}  // namespace cobalt
