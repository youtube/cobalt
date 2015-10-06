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

#include "cobalt/dom/html_script_element.h"

#include <deque>

#include "base/bind.h"
#include "base/string_util.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/sync_loader.h"
#include "cobalt/loader/text_decoder.h"
#include "cobalt/script/script_runner.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// static
const char HTMLScriptElement::kTagName[] = "script";

HTMLScriptElement::HTMLScriptElement(Document* document)
    : HTMLElement(document),
      is_already_started_(false),
      is_parser_inserted_(false),
      is_ready_(false),
      load_option_(0),
      inline_script_location_("[object HTMLScriptElement]", 1, 1) {
  DCHECK(document->html_element_context()->script_runner());
}

std::string HTMLScriptElement::tag_name() const { return kTagName; }

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

HTMLScriptElement::~HTMLScriptElement() {
  if (loader_) {
    StopLoading();
  }
}

// Algorithm for Prepare:
//   http://www.w3.org/TR/html5/scripting-1.html#prepare-a-script
void HTMLScriptElement::Prepare() {
  // Custom, not in any spec.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(MessageLoop::current());
  DCHECK(!loader_);

  // If the script element is marked as having "already started", then the user
  // agent must abort these steps at this point. The script is not executed.
  if (is_already_started_) {
    return;
  }

  // 2. 3. 4. Not needed by Cobalt.

  // 5. If the element is not in a Document, then the user agent must abort
  // these steps at this point. The script is not executed.
  if (!owner_document()) {
    return;
  }

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

    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&HTMLScriptElement::DispatchEvent),
                   base::AsWeakPtr<HTMLScriptElement>(this),
                   make_scoped_refptr(new Event("error"))));
    return;
  }

  //   3. Resolve src relative to the element.
  //   4. If the previous step failed, queue a task to fire a simple event named
  // error at the element, and abort these steps.
  const GURL base_url = owner_document()->url_as_gurl();
  url_ = base_url.Resolve(src());
  if (!url_.is_valid()) {
    LOG(WARNING) << src() << " cannot be resolved based on " << base_url << ".";

    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&HTMLScriptElement::DispatchEvent),
                   base::AsWeakPtr<HTMLScriptElement>(this),
                   make_scoped_refptr(new Event("error"))));
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
              &loader::FetcherFactory::CreateFetcher,
              base::Unretained(html_element_context()->fetcher_factory()),
              url_),
          base::Bind(&loader::TextDecoder::Create,
                     base::Bind(&HTMLScriptElement::OnSyncLoadingDone,
                                base::Unretained(this))),
          base::Bind(&HTMLScriptElement::OnSyncLoadingError,
                     base::Unretained(this)));

      if (is_sync_load_successful_) {
        html_element_context()->script_runner()->Execute(
            content_, base::SourceLocation(url_.spec(), 1, 1));

        // If the script is from an external file, fire a simple event named
        // load at the script element.
        DispatchEvent(new Event("load"));
      } else {
        // Executing the script block must just consist of firing a simple event
        // named error at the element.
        DispatchEvent(new Event("error"));
      }
    } break;
    case 4: {
      // If the element has a src attribute, does not have an async attribute,
      // and does not have the "force-async" flag set.

      // The element must be added to the end of the list of scripts that will
      // execute in order as soon as possible associated with the Document of
      // the script element at the time the prepare a script algorithm started.
      std::deque<HTMLScriptElement*>* scripts_to_be_executed =
          owner_document()->scripts_to_be_executed();
      scripts_to_be_executed->push_back(this);
      loader_.reset(new loader::Loader(
          base::Bind(
              &loader::FetcherFactory::CreateFetcher,
              base::Unretained(html_element_context()->fetcher_factory()),
              url_),
          scoped_ptr<loader::Decoder>(new loader::TextDecoder(base::Bind(
              &HTMLScriptElement::OnLoadingDone, base::Unretained(this)))),
          base::Bind(&HTMLScriptElement::OnLoadingError,
                     base::Unretained(this))));

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      owner_document()->IncreaseLoadingCounter();
    } break;
    case 5: {
      // If the element has a src attribute.

      // The element must be added to the set of scripts that will execute as
      // soon as possible of the Document of the script element at the time the
      // prepare a script algorithm started.
      loader_.reset(new loader::Loader(
          base::Bind(
              &loader::FetcherFactory::CreateFetcher,
              base::Unretained(html_element_context()->fetcher_factory()),
              url_),
          scoped_ptr<loader::Decoder>(new loader::TextDecoder(base::Bind(
              &HTMLScriptElement::OnLoadingDone, base::Unretained(this)))),
          base::Bind(&HTMLScriptElement::OnLoadingError,
                     base::Unretained(this))));

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      owner_document()->IncreaseLoadingCounter();
    } break;
    case 6: {
      // Otherwise.

      // The user agent must immediately execute the script block, even if other
      // scripts are already executing.
      ExecuteInternal();
    } break;
    default: { NOTREACHED(); }
  }
}

void HTMLScriptElement::OnSyncLoadingDone(const std::string& content) {
  content_ = content;
  is_sync_load_successful_ = true;
}

void HTMLScriptElement::OnSyncLoadingError(const std::string& error) {
  LOG(ERROR) << error;
}

// Algorithm for OnLoadingDone:
//   http://www.w3.org/TR/html5/scripting-1.html#prepare-a-script
void HTMLScriptElement::OnLoadingDone(const std::string& content) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
          owner_document()->scripts_to_be_executed();
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

        // 3. Remove the first element from this list of scripts that will
        // execute in order as soon as possible.
        scripts_to_be_executed->pop_front();

        // 4. If this list of scripts that will execute in order as soon as
        // possible is still not empty and the first entry has already been
        // marked
        // as ready, then jump back to the step labeled execution.
        if (scripts_to_be_executed->empty() ||
            !scripts_to_be_executed->front()->is_ready_) {
          break;
        }
      }

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      owner_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent(true);
    } break;
    case 5: {
      // If the element has a src attribute.

      // The task that the networking task source places on the task queue once
      // the fetching algorithm has completed must execute the script block and
      // then remove the element from the set of scripts that will execute as
      // soon as possible.
      ExecuteExternal();

      // Fetching an external script must delay the load event of the element's
      // document until the task that is queued by the networking task source
      // once the resource has been fetched (defined above) has been run.
      owner_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent(true);
    } break;
    default: { NOTREACHED(); }
  }
  StopLoading();
}

// Algorithm for OnLoadingError:
//   http://www.w3.org/TR/html5/scripting-1.html#prepare-a-script
void HTMLScriptElement::OnLoadingError(const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(load_option_ == 4 || load_option_ == 5);

  LOG(ERROR) << error;

  // Executing the script block must just consist of firing a simple event
  // named error at the element.
  DispatchEvent(new Event("error"));

  // Fetching an external script must delay the load event of the element's
  // document until the task that is queued by the networking task source
  // once the resource has been fetched (defined above) has been run.
  owner_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent(false);

  StopLoading();
}

// Algorithm for StopLoading:
//   http://www.w3.org/TR/html5/scripting-1.html#prepare-a-script
void HTMLScriptElement::StopLoading() {
  switch (load_option_) {
    case 4: {
      // If the element has a src attribute, does not have an async attribute,
      // and does not have the "force-async" flag set.
      std::deque<HTMLScriptElement*>* scripts_to_be_executed =
          owner_document()->scripts_to_be_executed();

      std::deque<HTMLScriptElement*>::iterator it = std::find(
          scripts_to_be_executed->begin(), scripts_to_be_executed->end(), this);
      if (it != scripts_to_be_executed->end()) {
        scripts_to_be_executed->erase(it);
      }
    } break;
    case 5: {
      // If the element has a src attribute.
    } break;
    default: { NOTREACHED(); }
  }
  DCHECK(loader_);
  loader_.reset();
}

// Algorithm for Execute:
//   http://www.w3.org/TR/html5/scripting-1.html#execute-the-script-block
void HTMLScriptElement::Execute(const std::string& content,
                                const base::SourceLocation& script_location,
                                bool is_external) {
  // Since error is already handled, it is guaranteed the load is successful.

  // 1. 2. 3. Not needed by Cobalt.

  // 4. Create a script, using the script block's source, the URL from which the
  // script was obtained, the script block's type as the scripting language, and
  // the script settings object of the script element's Document's Window
  // object.
  html_element_context()->script_runner()->Execute(content, script_location);

  // 5. 6. Not needed by Cobalt.

  // 7. If the script is from an external file, fire a simple event named load
  // at the script element.
  // Otherwise, the script is internal; queue a task to fire a simple event
  // named load at the script element.
  if (is_external) {
    DispatchEvent(new Event("load"));
  } else {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&HTMLScriptElement::DispatchEvent),
                   base::AsWeakPtr<HTMLScriptElement>(this),
                   make_scoped_refptr(new Event("load"))));
  }
}

}  // namespace dom
}  // namespace cobalt
