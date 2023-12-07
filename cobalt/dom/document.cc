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

#include "cobalt/dom/document.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/css_media_rule.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/benchmark_stat_names.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/dom_implementation.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/font_cache.h"
#include "cobalt/dom/font_face_updater.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_factory.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/html_script_element.h"
#include "cobalt/dom/initial_computed_style.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/keyframes_map_updater.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/mouse_event.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/node_descendants_iterator.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/text.h"
#include "cobalt/dom/ui_event.h"
#include "cobalt/dom/wheel_event.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/web/custom_event.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/message_event.h"

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace dom {
namespace {
csp::SecurityCallback CreateSecurityCallback(
    web::CspDelegate* csp_delegate, web::CspDelegate::ResourceType type) {
  csp::SecurityCallback callback;
  if (csp_delegate) {
    callback = base::Bind(&web::CspDelegate::CanLoad,
                          base::Unretained(csp_delegate), type);
  }
  return callback;
}
}  // namespace

Document::Document(HTMLElementContext* html_element_context,
                   const Options& options, web::CspDelegate* csp_delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(Node(html_element_context, this)),
      html_element_context_(html_element_context),
      application_lifecycle_state_(
          html_element_context_->application_lifecycle_state()),
      implementation_(new DOMImplementation(html_element_context)),
      style_sheets_(new cssom::StyleSheetList()),
      loading_counter_(0),
      should_dispatch_load_event_(true),
      are_style_sheets_dirty_(true),
      is_selector_tree_dirty_(true),
      is_computed_style_dirty_(true),
      are_font_faces_dirty_(true),
      are_keyframes_dirty_(true),
      selector_tree_(new cssom::SelectorTree()),
      should_recreate_selector_tree_(false),
      navigation_start_clock_(options.navigation_start_clock
                                  ? options.navigation_start_clock
                                  : new base::SystemMonotonicClock()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          default_timeline_(new DocumentTimeline(this, 0))),
      user_agent_style_sheet_(options.user_agent_style_sheet),
      initial_computed_style_declaration_(
          new cssom::CSSComputedStyleDeclaration()),
      ready_state_(kDocumentReadyStateComplete),
      dom_max_element_depth_(options.dom_max_element_depth),
      render_postponed_(false),
      frozenness_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(intersection_observer_task_manager_(
          new IntersectionObserverTaskManager())),
      navigation_type_(kNavigationTypeNavigate) {
  DCHECK(html_element_context_);
  DCHECK(options.url.is_empty() || options.url.is_valid());
  application_lifecycle_state_->AddObserver(this);

  if (options.viewport_size) {
    SetViewport(*options.viewport_size);
  }

  cookie_jar_ = options.cookie_jar;

  if (!csp_delegate) {
    csp_delegate = Document::GetCSPDelegate();
  }

  location_ = new Location(
      options.url, options.hashchange_callback, options.navigation_callback,
      CreateSecurityCallback(csp_delegate, web::CspDelegate::kLocation),
      base::Bind(&Document::SetNavigationType, base::Unretained(this)));

  font_cache_.reset(new FontCache(
      html_element_context_->resource_provider(),
      html_element_context_->remote_typeface_cache(),
      base::Bind(&Document::OnTypefaceLoadEvent, base::Unretained(this)),
      html_element_context_->font_language_script(), location_));

  if (html_element_context_->remote_typeface_cache()) {
    html_element_context_->remote_typeface_cache()->set_security_callback(
        CreateSecurityCallback(csp_delegate, web::CspDelegate::kFont));
  }
  if (html_element_context_->image_cache()) {
    html_element_context_->image_cache()->set_security_callback(
        CreateSecurityCallback(csp_delegate, web::CspDelegate::kImage));
  }

  ready_state_ = kDocumentReadyStateLoading;

  // Sample the timeline upon initialization.
  SampleTimelineTime();

  // Call OnInsertedIntoDocument() immediately to ensure that the Document
  // object itself is considered to be "in the document".
  OnInsertedIntoDocument();
}

base::Token Document::node_name() const {
  return base::Tokens::document_name();
}

scoped_refptr<Element> Document::document_element() const {
  return first_element_child();
}

scoped_refptr<Window> Document::default_view() const { return window(); }

std::string Document::title() const {
  const char kTitleTag[] = "title";
  if (head()) {
    scoped_refptr<HTMLCollection> collection =
        head()->GetElementsByTagName(kTitleTag);
    if (collection->length() > 0) {
      return collection->Item(0)->text_content().value_or("");
    }
  }
  return "";
}

scoped_refptr<DOMImplementation> Document::implementation() {
  return implementation_;
}

// Algorithm for GetElementsByTagName:
//   https://www.w3.org/TR/dom/#concept-getelementsbytagname
scoped_refptr<HTMLCollection> Document::GetElementsByTagName(
    const std::string& local_name) const {
  // 2. If the document is not an HTML document, then return an HTML collection
  //    whose name is local name. If it is an HTML document, then return an,
  //    HTML collection whose name is local name converted to ASCII lowercase.
  if (IsXMLDocument()) {
    return HTMLCollection::CreateWithElementsByLocalName(this, local_name);
  } else {
    const std::string lower_local_name = base::ToLowerASCII(local_name);
    return HTMLCollection::CreateWithElementsByLocalName(this,
                                                         lower_local_name);
  }
}

scoped_refptr<HTMLCollection> Document::GetElementsByClassName(
    const std::string& class_names) const {
  return HTMLCollection::CreateWithElementsByClassName(this, class_names);
}

scoped_refptr<Element> Document::CreateElement(const std::string& local_name) {
  if (IsXMLDocument()) {
    return new Element(this, base::Token(local_name));
  } else {
    std::string lower_local_name = base::ToLowerASCII(local_name);
    DCHECK(html_element_context_->html_element_factory());
    return html_element_context_->html_element_factory()->CreateHTMLElement(
        this, base::Token(lower_local_name));
  }
}

scoped_refptr<Element> Document::CreateElementNS(
    const std::string& namespace_uri, const std::string& local_name) {
  // TODO: Implement namespaces, if we actually need this.
  NOTIMPLEMENTED();
  return CreateElement(local_name);
}

scoped_refptr<Text> Document::CreateTextNode(const std::string& data) {
  return new Text(this, data);
}

scoped_refptr<Comment> Document::CreateComment(const std::string& data) {
  return new Comment(this, data);
}

scoped_refptr<web::Event> Document::CreateEvent(
    const std::string& interface_name,
    script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/dom/#dom-document-createevent
  // The match of interface name is case-insensitive.
  if (strcasecmp(interface_name.c_str(), "event") == 0 ||
      strcasecmp(interface_name.c_str(), "events") == 0 ||
      strcasecmp(interface_name.c_str(), "htmlevents") == 0) {
    return new web::Event(web::Event::Uninitialized);
  } else if (strcasecmp(interface_name.c_str(), "keyboardevent") == 0 ||
             strcasecmp(interface_name.c_str(), "keyevents") == 0) {
    return new KeyboardEvent(web::Event::Uninitialized);
  } else if (strcasecmp(interface_name.c_str(), "messageevent") == 0) {
    return new web::MessageEvent(web::Event::Uninitialized);
  } else if (strcasecmp(interface_name.c_str(), "mouseevent") == 0 ||
             strcasecmp(interface_name.c_str(), "mouseevents") == 0) {
    return new MouseEvent(web::Event::Uninitialized);
  } else if (strcasecmp(interface_name.c_str(), "uievent") == 0 ||
             strcasecmp(interface_name.c_str(), "uievents") == 0) {
    return new UIEvent(web::Event::Uninitialized);
  } else if (strcasecmp(interface_name.c_str(), "wheelevent") == 0) {
    // This not in the spec, but commonly implemented to create a WheelEvent.
    //   https://www.w3.org/TR/2016/WD-uievents-20160804/#interface-wheelevent
    return new WheelEvent(web::Event::Uninitialized);
  } else if (strcasecmp(interface_name.c_str(), "customevent") == 0) {
    return new web::CustomEvent(web::Event::Uninitialized);
  }

  web::DOMException::Raise(
      web::DOMException::kNotSupportedErr,
      "document.createEvent does not support \"" + interface_name + "\".",
      exception_state);

  // Return value will be ignored.
  return NULL;
}

scoped_refptr<Element> Document::GetElementById(const std::string& id) const {
  NodeDescendantsIterator iterator(this);

  // TODO: Consider optimizing this method by replacing the linear
  // search with a constant time lookup.
  Node* child = iterator.First();
  while (child) {
    scoped_refptr<Element> element = child->AsElement();
    if (element && element->id() == id) {
      return element;
    }
    child = iterator.Next();
  }
  return NULL;
}

const scoped_refptr<Location>& Document::location() const { return location_; }

// Algorithm for dir:
//   https://html.spec.whatwg.org/commit-snapshots/ebcac971c2add28a911283899da84ec509876c44/#dom-dir
std::string Document::dir() const {
  // The dir IDL attribute on Document objects must reflect the dir content
  // attribute of the html element, if any, limited to only known values. If
  // there is no such element, then the attribute must return the empty string
  // and do nothing on setting.
  HTMLHtmlElement* html_element = html();
  if (!html_element) {
    return "";
  }
  return html_element->dir();
}

// Algorithm for dir:
//   https://html.spec.whatwg.org/commit-snapshots/ebcac971c2add28a911283899da84ec509876c44/#dom-dir
void Document::set_dir(const std::string& value) {
  // The dir IDL attribute on Document objects must reflect the dir content
  // attribute of the html element, if any, limited to only known values. If
  // there is no such element, then the attribute must return the empty string
  // and do nothing on setting.
  HTMLHtmlElement* html_element = html();
  if (html_element) {
    html_element->set_dir(value);
  }
}

// Algorithm for body:
//   https://www.w3.org/TR/html50/dom.html#dom-document-body
scoped_refptr<HTMLBodyElement> Document::body() const {
  // The body element of a document is the first child of the html element that
  // is either a body element or a frameset element. If there is no such
  // element, it is null.
  //   https://www.w3.org/TR/html50/dom.html#the-body-element-0
  HTMLHtmlElement* html_element = html().get();
  if (!html_element) {
    return NULL;
  }
  for (Element* child = html_element->first_element_child(); child;
       child = child->next_element_sibling()) {
    HTMLElement* child_html_element = child->AsHTMLElement().get();
    if (child_html_element) {
      HTMLBodyElement* body_element = child_html_element->AsHTMLBodyElement();
      if (body_element) {
        return body_element;
      }
    }
  }
  return NULL;
}

// Algorithm for set_body:
//   https://www.w3.org/TR/html50/dom.html#dom-document-body
void Document::set_body(const scoped_refptr<HTMLBodyElement>& body) {
  // 1. If the new value is not a body or frameset element, then throw a
  //    HierarchyRequestError exception and abort these steps.
  // 2. Otherwise, if the new value is the same as the body element, do nothing.
  //    Abort these steps.
  scoped_refptr<HTMLBodyElement> current_body = this->body();
  if (current_body == body) {
    return;
  }

  // 3. Otherwise, if the body element is not null, then replace that element
  //    with the new value in the DOM, as if the root element's replaceChild()
  //    method had been called with the new value and the incumbent body element
  //    as its two arguments respectively, then abort these steps.
  // 4. Otherwise, if there is no root element, throw a HierarchyRequestError
  //    exception and abort these steps.
  // 5. Otherwise, the body element is null, but there's a root element. Append
  //    the new value to the root element.
  scoped_refptr<HTMLHtmlElement> current_html = html();
  if (!current_html) {
    // TODO: Throw JS HierarchyRequestError.
    return;
  }
  if (current_body) {
    current_html->ReplaceChild(body, current_body);
  } else {
    current_html->AppendChild(body);
  }
}

// Algorithm for head:
//   https://www.w3.org/TR/html50/dom.html#dom-document-head
scoped_refptr<HTMLHeadElement> Document::head() const {
  // The head element of a document is the first head element that is a child of
  // the html element, if there is one, or null otherwise.
  //   https://www.w3.org/TR/html50/dom.html#the-head-element-0
  HTMLHtmlElement* html_element = html().get();
  if (!html_element) {
    return NULL;
  }
  for (Element* child = html_element->first_element_child(); child;
       child = child->next_element_sibling()) {
    HTMLElement* child_html_element = child->AsHTMLElement().get();
    if (child_html_element) {
      HTMLHeadElement* head_element = child_html_element->AsHTMLHeadElement();
      if (head_element) {
        return head_element;
      }
    }
  }
  return NULL;
}

scoped_refptr<HTMLScriptElement> Document::current_script() const {
  return current_script_;
}

void Document::set_current_script(
    const scoped_refptr<HTMLScriptElement>& current_script) {
  current_script_ = current_script;
}

bool Document::HasFocus() const {
  return application_lifecycle_state()->HasWindowFocus();
}

// https://www.w3.org/TR/html50/editing.html#dom-document-activeelement
scoped_refptr<Element> Document::active_element() const {
  // The activeElement attribute on Document objects must return the element in
  // the document that is focused. If no element in the Document is focused,
  // this must return the body element.
  if (!active_element_) {
    return body();
  } else {
    return active_element_.get();
  }
}

// https://www.w3.org/TR/2016/REC-html51-20161101/matching-html-elements-using-selectors.html#selectordef-hover
scoped_refptr<HTMLElement> Document::indicated_element() const {
  return indicated_element_.get();
}

const scoped_refptr<cssom::StyleSheetList>& Document::style_sheets() {
  UpdateStyleSheets();
  return style_sheets_;
}

// https://html.spec.whatwg.org/#cookie-averse-document-object
bool Document::IsCookieAverseDocument() const {
  return !HasBrowsingContext() || (!location_->url().SchemeIs("ftp") &&
                                   !location_->url().SchemeIs("http") &&
                                   !location_->url().SchemeIs("https"));
}

// https://html.spec.whatwg.org/#dom-document-cookie
void Document::set_cookie(const std::string& cookie,
                          script::ExceptionState* exception_state) {
  if (IsCookieAverseDocument()) {
    DLOG(WARNING) << "Document is a cookie-averse Document object, not "
                     "setting cookie.";
    return;
  }
  if (location_->GetOriginAsObject().is_opaque()) {
    web::DOMException::Raise(web::DOMException::kSecurityErr,
                             "Document origin is opaque, cookie setting failed",
                             exception_state);
    return;
  }
  if (cookie_jar_) {
    cookie_jar_->SetCookie(location()->url(), cookie);
  }
}

// https://html.spec.whatwg.org/#dom-document-cookie
std::string Document::cookie(script::ExceptionState* exception_state) const {
  if (IsCookieAverseDocument()) {
    DLOG(WARNING) << "Document is a cookie-averse Document object, returning "
                     "empty cookie.";
    return "";
  }
  if (location_->GetOriginAsObject().is_opaque()) {
    web::DOMException::Raise(web::DOMException::kSecurityErr,
                             "Document origin is opaque, cookie getting failed",
                             exception_state);
    return "";
  }
  if (cookie_jar_) {
    return net::CanonicalCookie::BuildCookieLine(
        cookie_jar_->GetCookies(location()->url()));
  } else {
    DLOG(WARNING) << "Document has no cookie jar";
    return "";
  }
}

void Document::set_cookie(const std::string& cookie) {
  if (IsCookieAverseDocument()) {
    DLOG(WARNING) << "Document is a cookie-averse Document object, not "
                     "setting cookie.";
    return;
  }
  if (location_->GetOriginAsObject().is_opaque()) {
    DLOG(WARNING) << "Document origin is opaque, cookie setting failed";
    return;
  }
  if (cookie_jar_) {
    cookie_jar_->SetCookie(location()->url(), cookie);
  }
}

std::string Document::cookie() const {
  if (IsCookieAverseDocument()) {
    DLOG(WARNING) << "Document is a cookie-averse Document object, returning "
                     "empty cookie.";
    return "";
  }
  if (location_->GetOriginAsObject().is_opaque()) {
    DLOG(WARNING) << "Document origin is opaque, cookie getting failed";
    return "";
  }
  if (cookie_jar_) {
    return net::CanonicalCookie::BuildCookieLine(
        cookie_jar_->GetCookies(location()->url()));
  } else {
    DLOG(WARNING) << "Document has no cookie jar";
    return "";
  }
}

void Document::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

void Document::Accept(ConstNodeVisitor* visitor) const { visitor->Visit(this); }

scoped_refptr<Node> Document::Duplicate() const {
  // For Document, copy Its encoding, content type, URL, its mode (quirks mode,
  // limited quirks mode, or no-quirks mode), and its type (XML document or HTML
  // document).
  //   https://www.w3.org/TR/dom/#concept-node-clone
  return new Document(html_element_context_,
                      Document::Options(location()->url()));
}

scoped_refptr<HTMLHtmlElement> Document::html() const {
  // The html element of a document is the document's root element, if there is
  // one and it's an html element, or null otherwise.
  //   https://www.w3.org/TR/html50/dom.html#the-html-element-0
  Element* root = document_element().get();
  if (!root) {
    return NULL;
  }
  HTMLElement* root_html_element = root->AsHTMLElement();
  return root_html_element ? root_html_element->AsHTMLHtmlElement() : NULL;
}

void Document::SetActiveElement(Element* active_element) {
  if (active_element) {
    active_element_ = base::AsWeakPtr(active_element);
    if (active_element != ui_nav_focus_element_) {
      // Call UpdateUiNavigationFocus() after UI navigation items have been
      // updated. This happens during render tree generation from layout.
      ui_nav_focus_needs_update_ = true;
    }
  } else {
    active_element_.reset();
  }
}

void Document::SetIndicatedElement(HTMLElement* indicated_element) {
  if (indicated_element != indicated_element_.get()) {
    if (indicated_element_) {
      // Clear the rule matching state on this element and its ancestors, as
      // their hover state may be changing. However, the tree's matching rules
      // only need to be invalidated once, so only do it here if it won't occur
      // below.
      bool invalidate_tree_matching_rules = (indicated_element == NULL);
      indicated_element_->ClearRuleMatchingStateOnElementAndAncestors(
          invalidate_tree_matching_rules);
      indicated_element_->OnCSSMutation();
    }
    if (indicated_element) {
      indicated_element_ = base::AsWeakPtr(indicated_element);
      // Clear the rule matching state on this element and its ancestors, as
      // their hover state may be changing.
      indicated_element_->ClearRuleMatchingStateOnElementAndAncestors(
          true /*invalidate_tree_matching_rules*/);
      indicated_element_->OnCSSMutation();
    } else {
      indicated_element_.reset();
    }
  }
}

web::CspDelegate* Document::GetCSPDelegate() const {
  web::WindowOrWorkerGlobalScope* window_or_worker_global_scope =
      environment_settings()->context()
          ? environment_settings()->context()->GetWindowOrWorkerGlobalScope()
          : nullptr;
  return window_or_worker_global_scope
             ? window_or_worker_global_scope->csp_delegate()
             : nullptr;
}

const scoped_refptr<Window> Document::window() const {
  web::WindowOrWorkerGlobalScope* window_or_worker_global_scope =
      environment_settings()->context()
          ? environment_settings()->context()->GetWindowOrWorkerGlobalScope()
          : nullptr;
  return window_or_worker_global_scope
             ? window_or_worker_global_scope->AsWindow()
             : nullptr;
}

void Document::IncreaseLoadingCounter() { ++loading_counter_; }

void Document::DecreaseLoadingCounter() { --loading_counter_; }

void Document::DecreaseLoadingCounterAndMaybeDispatchLoadEvent() {
  DCHECK_GT(loading_counter_, 0);
  loading_counter_--;
  if (loading_counter_ == 0 && should_dispatch_load_event_) {
    DCHECK(base::MessageLoop::current());
    should_dispatch_load_event_ = false;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&Document::DispatchOnLoadEvent,
                              base::AsWeakPtr<Document>(this)));

    HTMLBodyElement* body_element = body().get();
    if (body_element) {
      body_element->PostToDispatchEventName(FROM_HERE, base::Tokens::load());
    }
  }
}

void Document::AddObserver(DocumentObserver* observer) {
  observers_.AddObserver(observer);
}

void Document::RemoveObserver(DocumentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Document::SignalOnLoadToObservers() {
  FOR_EACH_OBSERVER(DocumentObserver, observers_, OnLoad());
}

void Document::RecordMutation() {
  TRACE_EVENT0("cobalt::dom", "Document::RecordMutation()");

  FOR_EACH_OBSERVER(DocumentObserver, observers_, OnMutation());
}

void Document::DoSynchronousLayout() {
  TRACE_EVENT0("cobalt::dom", "Document::DoSynchronousLayout()");

  if (!synchronous_layout_callback_.is_null()) {
    synchronous_layout_callback_.Run();
  }
}

scoped_refptr<render_tree::Node>
Document::DoSynchronousLayoutAndGetRenderTree() {
  TRACE_EVENT0("cobalt::dom",
               "Document::DoSynchronousLayoutAndGetRenderTree()");

  if (synchronous_layout_and_produce_render_tree_callback_.is_null()) {
    DLOG(WARNING)
        << "|synchronous_layout_and_produce_render_tree_callback_| is null";
    return nullptr;
  }

  return synchronous_layout_and_produce_render_tree_callback_.Run();
}

void Document::NotifyUrlChanged(const GURL& url) {
  location_->set_url(url);
  GetCSPDelegate()->NotifyUrlChanged(url);
}

void Document::OnFocusChange() {
  is_computed_style_dirty_ = true;
  RecordMutation();
  FOR_EACH_OBSERVER(DocumentObserver, observers_, OnFocusChanged());
}

void Document::OnStyleSheetsModified() {
  are_style_sheets_dirty_ = true;
  OnCSSMutation();
}

void Document::OnCSSMutation() {
  // Something in the document's CSS rules has been modified, but we don't know
  // what, so set the flag indicating that rule matching needs to be done.
  is_selector_tree_dirty_ = true;
  is_computed_style_dirty_ = true;
  are_font_faces_dirty_ = true;
  are_keyframes_dirty_ = true;

  scoped_refptr<HTMLHtmlElement> current_html = html();
  if (current_html) {
    current_html->InvalidateComputedStylesOfNodeAndDescendants();
  }

  RecordMutation();
}

void Document::OnDOMMutation() {
  // Something in the document's DOM has been modified, but we don't know what,
  // so set the flag indicating that computed styles need to be updated.
  is_computed_style_dirty_ = true;

  RecordMutation();
}

void Document::OnTypefaceLoadEvent() {
  scoped_refptr<HTMLHtmlElement> current_html = html();
  if (current_html) {
    current_html->InvalidateLayoutBoxesOfNodeAndDescendants();
  }
  RecordMutation();
}

void Document::OnElementInlineStyleMutation() {
  is_computed_style_dirty_ = true;

  RecordMutation();
}

namespace {

void RemoveRulesFromCSSRuleListFromSelectorTree(
    const scoped_refptr<cssom::CSSRuleList>& css_rule_list,
    cssom::SelectorTree* maybe_selector_tree) {
  for (unsigned int i = 0; i < css_rule_list->length(); ++i) {
    cssom::CSSRule* rule = css_rule_list->Item(i).get();

    cssom::CSSStyleRule* css_style_rule = rule->AsCSSStyleRule();
    if (css_style_rule && css_style_rule->added_to_selector_tree()) {
      if (maybe_selector_tree) {
        maybe_selector_tree->RemoveRule(css_style_rule);
      }
      css_style_rule->set_added_to_selector_tree(false);
    }

    cssom::CSSMediaRule* css_media_rule = rule->AsCSSMediaRule();
    if (css_media_rule) {
      RemoveRulesFromCSSRuleListFromSelectorTree(css_media_rule->css_rules(),
                                                 maybe_selector_tree);
    }
  }
}

void AppendRulesFromCSSRuleListToSelectorTree(
    const scoped_refptr<cssom::CSSRuleList>& css_rule_list,
    cssom::SelectorTree* selector_tree) {
  for (unsigned int i = 0; i < css_rule_list->length(); ++i) {
    cssom::CSSRule* rule = css_rule_list->Item(i).get();

    cssom::CSSStyleRule* css_style_rule = rule->AsCSSStyleRule();
    if (css_style_rule && !css_style_rule->added_to_selector_tree()) {
      selector_tree->AppendRule(css_style_rule);
      css_style_rule->set_added_to_selector_tree(true);
    }

    cssom::CSSMediaRule* css_media_rule = rule->AsCSSMediaRule();
    if (css_media_rule) {
      if (css_media_rule->condition_value()) {
        AppendRulesFromCSSRuleListToSelectorTree(css_media_rule->css_rules(),
                                                 selector_tree);
      } else {
        RemoveRulesFromCSSRuleListFromSelectorTree(css_media_rule->css_rules(),
                                                   selector_tree);
      }
    }
  }
}

void AppendRulesFromCSSStyleSheetToSelectorTree(
    const scoped_refptr<cssom::CSSStyleSheet>& style_sheet,
    cssom::SelectorTree* selector_tree) {
  AppendRulesFromCSSRuleListToSelectorTree(style_sheet->css_rules_same_origin(),
                                           selector_tree);
}

void ClearAddedToSelectorTreeFromCSSStyleSheetRules(
    const scoped_refptr<cssom::CSSStyleSheet>& style_sheet) {
  RemoveRulesFromCSSRuleListFromSelectorTree(
      style_sheet->css_rules_same_origin(), NULL);
}

}  // namespace

void Document::UpdateComputedStyles() {
  TRACE_EVENT0("cobalt::dom", "Document::UpdateComputedStyles()");

  UpdateSelectorTree();
  UpdateKeyframes();
  UpdateFontFaces();

  if (is_computed_style_dirty_) {
    TRACE_EVENT0("cobalt::layout", kBenchmarkStatUpdateComputedStyles);
    base::StopWatch stop_watch_update_compute_style(
        DomStatTracker::kStopWatchTypeUpdateComputedStyle,
        base::StopWatch::kAutoStartOn,
        html_element_context_->dom_stat_tracker());

    // Determine the official time that this style change event took place. This
    // is needed (as opposed to repeatedly calling base::Time::Now()) because
    // all animations that may be triggered here must start at the exact same
    // time if they were triggered in the same style change event.
    //   https://www.w3.org/TR/css3-transitions/#starting
    base::TimeDelta style_change_event_time =
        base::TimeDelta::FromMillisecondsD(*default_timeline_->current_time());

    scoped_refptr<HTMLElement> root = html();
    if (root) {
      DCHECK_EQ(this, root->parent_node());
      // First, update the matching rules for all elements.
      root->UpdateMatchingRulesRecursively();

      // Then, update the computed style for the root element.
      root->UpdateComputedStyle(
          initial_computed_style_declaration_, initial_computed_style_data_,
          style_change_event_time, HTMLElement::kAncestorsAreDisplayed);

      // Finally, update the computed styles for the other elements.
      root->UpdateComputedStyleRecursively(
          root->css_computed_style_declaration(), root->computed_style(),
          style_change_event_time, true, 0 /* current_element_depth */);
    }

    is_computed_style_dirty_ = false;
  }
}

bool Document::UpdateComputedStyleOnElementAndAncestor(HTMLElement* element) {
  TRACE_EVENT0("cobalt::dom",
               "Document::UpdateComputedStyleOnElementAndAncestor");
  if (!element || element->node_document() != this) {
    return false;
  }

  // We explicitly don't short-circuit if the document's
  // is_computed_style_dirty_ is not set because the specific element we are
  // updating may have or be under an ancestor element with 'display: none' on
  // it, in which case the element's computed style will be un-updated despite
  // the document's is_computed_style_dirty_ being false.

  UpdateSelectorTree();
  UpdateKeyframes();
  UpdateFontFaces();

  base::TimeDelta style_change_event_time =
      base::TimeDelta::FromMillisecondsD(*default_timeline_->current_time());

  // Find all ancestors of the element until the document.
  std::vector<HTMLElement*> ancestors;
  while (true) {
    ancestors.push_back(element);
    if (element->parent_node() == static_cast<Node*>(this)) {
      break;
    }
    Element* parent_element = element->parent_element();
    if (!parent_element) {
      return false;
    }
    element = parent_element->AsHTMLElement();
    if (!element) {
      return false;
    }
  }

  // Update computed styles on the ancestors and the element.
  HTMLElement* previous_element = NULL;
  bool ancestors_were_valid = true;
  HTMLElement::AncestorsAreDisplayed ancestors_are_displayed =
      HTMLElement::kAncestorsAreDisplayed;
  scoped_refptr<const cssom::CSSComputedStyleData> root_element_computed_style;
  for (std::vector<HTMLElement*>::reverse_iterator it = ancestors.rbegin();
       it != ancestors.rend(); ++it) {
    HTMLElement* current_element = *it;

    // Ensure that the matching rules are up to date prior to updating the
    // computed style.
    current_element->UpdateMatchingRules();

    bool is_valid =
        ancestors_were_valid && current_element->AreComputedStylesValid();
    if (!is_valid) {
      DCHECK(initial_computed_style_declaration_);
      DCHECK(initial_computed_style_data_);
      current_element->UpdateComputedStyle(
          previous_element ? previous_element->css_computed_style_declaration()
                           : initial_computed_style_declaration_,
          root_element_computed_style ? root_element_computed_style
                                      : initial_computed_style_data_,
          style_change_event_time, ancestors_are_displayed);
    }
    if (!root_element_computed_style) {
      DCHECK_EQ(this, current_element->parent_node());
      root_element_computed_style = current_element->computed_style();
    }
    if (ancestors_are_displayed == HTMLElement::kAncestorsAreDisplayed &&
        current_element->computed_style()->display() ==
            cssom::KeywordValue::GetNone()) {
      ancestors_are_displayed = HTMLElement::kAncestorsAreNotDisplayed;
    }
    previous_element = current_element;
    ancestors_were_valid =
        is_valid && current_element->descendant_computed_styles_valid();
  }

  return true;
}

void Document::UpdateUiNavigation() {
  HTMLElement* active_html_element =
      active_element() ? active_element()->AsHTMLElement() : nullptr;
  if (active_html_element && ui_nav_focus_needs_update_) {
    ui_nav_focus_needs_update_ = false;
    active_html_element->UpdateUiNavigationFocus();
  }
}

bool Document::TrySetUiNavFocusElement(const void* focus_element,
                                       SbTimeMonotonic time) {
  if (ui_nav_focus_element_update_time_ > time) {
    // A later focus update was already issued.
    return false;
  }
  ui_nav_focus_element_update_time_ = time;
  ui_nav_focus_element_ = focus_element;
  return true;
}

void Document::SampleTimelineTime() { default_timeline_->Sample(); }

ViewportSize Document::viewport_size() {
  return viewport_size_.value_or(ViewportSize());
}

void Document::SetViewport(const ViewportSize& viewport_size) {
  if (viewport_size_ && *viewport_size_ == viewport_size) {
    return;
  }
  viewport_size_ = viewport_size;
  initial_computed_style_data_ =
      CreateInitialComputedStyle(viewport_size_->width_height());
  initial_computed_style_declaration_->SetData(initial_computed_style_data_);

  is_computed_style_dirty_ = true;
  is_selector_tree_dirty_ = true;

  scoped_refptr<HTMLHtmlElement> current_html = html();
  if (current_html) {
    current_html->InvalidateComputedStylesOfNodeAndDescendants();
  }

  RecordMutation();
}

Document::~Document() {
  if (application_lifecycle_state_) {
    application_lifecycle_state_->RemoveObserver(this);
  }
  // Ensure that all outstanding weak ptrs become invalid.
  // Some objects that will be released while this destructor runs may
  // have weak ptrs to |this|.
  InvalidateWeakPtrs();
}

void Document::UpdateSelectorTree() {
  TRACE_EVENT0("cobalt::dom", "Document::UpdateSelectorTree()");
  if (is_selector_tree_dirty_) {
    TRACE_EVENT0("cobalt::dom", kBenchmarkStatUpdateSelectorTree);

    UpdateStyleSheets();
    UpdateMediaRules();

    // If the selector tree is being recreated, then clear the added state from
    // the document's style sheets. This will cause them to be added to the new
    // selector tree.
    if (should_recreate_selector_tree_) {
      DLOG(WARNING) << "A style sheet was removed from the document or the "
                       "document's style sheets have been reordered. This "
                       "triggers a recreation of the selector tree and should "
                       "be avoided if possible.";
      if (user_agent_style_sheet_) {
        ClearAddedToSelectorTreeFromCSSStyleSheetRules(user_agent_style_sheet_);
      }
      for (unsigned int style_sheet_index = 0;
           style_sheet_index < style_sheets_->length(); ++style_sheet_index) {
        scoped_refptr<cssom::CSSStyleSheet> css_style_sheet =
            style_sheets_->Item(style_sheet_index)->AsCSSStyleSheet();
        ClearAddedToSelectorTreeFromCSSStyleSheetRules(css_style_sheet);
      }
      selector_tree_.reset(new cssom::SelectorTree());
      should_recreate_selector_tree_ = false;
    }

    if (user_agent_style_sheet_) {
      AppendRulesFromCSSStyleSheetToSelectorTree(user_agent_style_sheet_,
                                                 selector_tree_.get());
    }
    for (unsigned int style_sheet_index = 0;
         style_sheet_index < style_sheets_->length(); ++style_sheet_index) {
      scoped_refptr<cssom::CSSStyleSheet> css_style_sheet =
          style_sheets_->Item(style_sheet_index)->AsCSSStyleSheet();
      AppendRulesFromCSSStyleSheetToSelectorTree(css_style_sheet,
                                                 selector_tree_.get());
    }
    // Now that the selector tree is fully updated, validate its version
    // compatibility.
    selector_tree_->ValidateVersionCompatibility();

    scoped_refptr<HTMLHtmlElement> current_html = html();
    if (current_html) {
      current_html->ClearRuleMatchingStateOnElementAndSiblingsAndDescendants();
    }

    is_selector_tree_dirty_ = false;
  }
}

void Document::PurgeCachedResources() {
  // Set the font faces to dirty prior to purging the font cache so that they'll
  // be restored when processing resumes.
  are_font_faces_dirty_ = true;
  font_cache_->PurgeCachedResources();

  // Set the computed style to dirty so that it'll be able to update any
  // elements that had images purged when processing resumes.
  is_computed_style_dirty_ = true;

  scoped_refptr<HTMLHtmlElement> current_html = html();
  if (current_html) {
    current_html->PurgeCachedBackgroundImagesOfNodeAndDescendants();
  }
}

void Document::InvalidateLayoutBoxes() {
  scoped_refptr<HTMLHtmlElement> current_html = html();
  if (current_html) {
    current_html->InvalidateLayoutBoxesOfNodeAndDescendants();
  }
}

void Document::DisableJit() {
  environment_settings()->context()->global_environment()->DisableJit();
}

void Document::OnWindowFocusChanged(bool has_focus) {
  // Ignored by this class.
}

void Document::OnVisibilityStateChanged(VisibilityState visibility_state) {
  DispatchEvent(new web::Event(base::Tokens::visibilitychange(),
                               web::Event::kBubbles,
                               web::Event::kNotCancelable));

  // Refocus the previously-focused UI navigation item (if any).
  if (visibility_state == kVisibilityStateVisible) {
    HTMLElement* active_html_element =
        active_element() ? active_element()->AsHTMLElement() : nullptr;
    if (active_html_element) {
      ui_nav_focus_needs_update_ = false;
      active_html_element->UpdateUiNavigationFocus();
    }
  }
}

// Algorithm for 'change the frozenness of a document'
//   https://wicg.github.io/page-lifecycle/#change-the-frozenness-of-a-document
void Document::OnFrozennessChanged(bool is_frozen) {
  // 1. If frozenness is true, run the freeze steps for doc given auto
  // resume frozen media.
  // 2. Otherwise, run the resume steps given doc.
  if (is_frozen) {
    FreezeSteps();
  } else {
    ResumeSteps();
  }
}

void Document::CollectHTMLMediaElements(
    std::vector<HTMLMediaElement*>* html_media_elements) {
  scoped_refptr<HTMLElement> root = html();
  if (root) {
    DCHECK_EQ(this, root->parent_node());
    root->CollectHTMLMediaElementsRecursively(html_media_elements, 0);
  }
}

// Algorithm for 'freeze steps'
//   https://wicg.github.io/page-lifecycle/#freeze-steps
void Document::FreezeSteps() {
  if (frozenness_) {
    return;
  }

  // 1. Set doc's frozeness state to true.
  frozenness_ = true;

  // 2. Fire an event named freeze at doc.
  DispatchEvent(new web::Event(base::Tokens::freeze(), web::Event::kBubbles,
                               web::Event::kNotCancelable));

  // 3. Let elements be all media elements that are shadow-including
  //    documents of doc, in shadow-including tree order.
  //    Note: Cobalt currently only supports one document.
  std::unique_ptr<std::vector<HTMLMediaElement*>> html_media_elements(
      new std::vector<HTMLMediaElement*>());
  CollectHTMLMediaElements(html_media_elements.get());

  // 4. For each element in elements:
  //    1. If element's paused is false, then:
  //       1. Set element's resume frozen flag to auto resume frozen
  //       media.
  //       2. Execute media pause on element.
  for (const auto& element : *html_media_elements) {
    if (!element->paused()) {
      element->set_resume_frozen_flag(true);
      element->Pause();
    }
  }
}

// Algorithm for 'resume steps'
//   https://wicg.github.io/page-lifecycle/#resume-steps
void Document::ResumeSteps() {
  if (!frozenness_) {
    return;
  }

  // 1. Let elements be all media elements that are shadow-including
  //    documents of doc, in shadow-including tree order.
  //    Note: Cobalt currently only supports one document.
  std::unique_ptr<std::vector<HTMLMediaElement*>> html_media_elements(
      new std::vector<HTMLMediaElement*>());
  CollectHTMLMediaElements(html_media_elements.get());

  // 2. For each element in elements:
  //    1. If elements's resume frozen flag is true.
  //       1. Set elements's resume frozen flag to false.
  //       2. Execute media play on element.
  for (const auto& element : *html_media_elements) {
    if (element->resume_frozen_flag()) {
      element->set_resume_frozen_flag(false);
      element->Play();
    }
  }

  // 3. Fire an event named resume at doc.
  DispatchEvent(new web::Event(base::Tokens::resume(), web::Event::kBubbles,
                               web::Event::kNotCancelable));

  // 4. Set doc's frozeness state to false.
  frozenness_ = false;
}

void Document::TraceMembers(script::Tracer* tracer) {
  Node::TraceMembers(tracer);

  tracer->Trace(implementation_);
  tracer->Trace(style_sheets_);
  tracer->TraceItems(scripts_to_be_executed_);
  tracer->TraceValues(keyframes_map_);
  tracer->Trace(location_);
  tracer->Trace(active_element_);
  tracer->Trace(indicated_element_);
  tracer->Trace(default_timeline_);
  tracer->Trace(user_agent_style_sheet_);
  tracer->Trace(initial_computed_style_declaration_);
}

void Document::CreatePerformanceNavigationTiming(
    Performance* performance, const net::LoadTimingInfo& timing_info) {
  // To create the navigation timing entry for document, given a loadTiminginfo,
  // a navigationType and a null service worker timing info, do the following:
  //   https://www.w3.org/TR/2022/WD-navigation-timing-2-20220224/#marking-navigation-timing
  // 1. Let global be document's relevant global object.
  // 2. Let navigationTimingEntry be a new PerformanceNavigationTiming object
  // in global's realm.
  // 3. Setup the resource timing entry for navigationTimingEntry given
  // "navigation", document's address, and fetchTiming.
  // 4. Set navigationTimingEntry's document load timing to document's
  // load timing info.
  // 5. Set navigationTimingEntry's previous document unload timing to
  // document's previous document unload timing.
  // 6. Set navigationTimingEntry's redirect count to redirectCount.
  // 7. Set navigationTimingEntry's navigation type to navigationType.
  // 8. Set navigationTimingEntry's service worker timing to
  // serviceWorkerTiming.
  scoped_refptr<PerformanceNavigationTiming> navigation_timing(
      new PerformanceNavigationTiming(timing_info, location_->url().spec(),
                                      performance, this,
                                      performance->GetTimeOrigin()));

  // 9. Set document's navigation timing entry to
  // navigationTimingEntry.
  navigation_timing_entry_ = navigation_timing;
  // 10. add navigationTimingEntry to global's performance entry buffer.
  performance->AddEntryToPerformanceEntryBuffer(navigation_timing_entry_);
  // 11. To queue the navigation timing entry for Document document, queue
  // document's navigation timing entry.
  performance->QueuePerformanceEntry(navigation_timing_entry_);
}

void Document::SetUnloadEventTimingInfo(base::TimeTicks start_time,
                                        base::TimeTicks end_time) {
  document_load_timing_info_.unload_event_start = start_time;
  document_load_timing_info_.unload_event_end = end_time;
}


void Document::set_render_postponed(bool render_postponed) {
  bool unpostponed = render_postponed_ && !render_postponed;
  render_postponed_ = render_postponed;
  if (unpostponed) {
    RecordMutation();
  }
}

void Document::CollectTimingInfoAndDispatchEvent() {
  DCHECK(html_element_context_);
  Performance* performance = html_element_context_->performance();
  bool is_performance_valid = performance != nullptr;

  // Set document load timing info's dom content loaded event start time
  // before user agent dispatches the DOMConentLoaded event.
  if (is_performance_valid) {
    document_load_timing_info_.dom_content_loaded_event_start =
        performance->Now();
  }

  PostToDispatchEventName(FROM_HERE, base::Tokens::domcontentloaded());

  // Set document load timing info's dom content loaded event end time
  // after user agent completes handling the DOMConentLoaded event.
  if (is_performance_valid) {
    document_load_timing_info_.dom_content_loaded_event_end =
        performance->Now();
  }
}

void Document::OnRootElementUnableToProvideOffsetDimensions() {
  window()->OnDocumentRootElementUnableToProvideOffsetDimensions();
}

void Document::DispatchOnLoadEvent() {
  TRACE_EVENT0("cobalt::dom", "Document::DispatchOnLoadEvent()");

  if (HasBrowsingContext()) {
    // Update the current timeline sample time and then update computed styles
    // before dispatching the onload event.  This guarantees that computed
    // styles have been calculated before JavaScript executes onload event
    // handlers, which may wish to start a CSS Transition (requiring that
    // computed values previously exist).
    SampleTimelineTime();
    UpdateComputedStyles();
  }

  DCHECK(html_element_context_);
  Performance* performance = html_element_context_->performance();
  bool is_performance_valid = performance != nullptr;

  // Set document load timing info's dom complete time before user agent set the
  // current document readiness to "complete".
  if (is_performance_valid) {
    document_load_timing_info_.dom_complete = performance->Now();
  }

  // Adjust the document ready state to reflect the fact that the document has
  // finished loading.  Performing this update and firing the readystatechange
  // event before the load event matches Chromium's behavior.
  ready_state_ = kDocumentReadyStateComplete;

  // Dispatch the readystatechange event (before the load event), since we
  // have changed the document ready state.
  DispatchEvent(new web::Event(base::Tokens::readystatechange()));

  // Set document load timing info's load event start time before user agent
  // dispatch the load event for the document.
  if (is_performance_valid) {
    document_load_timing_info_.load_event_start = performance->Now();
  }

  // Dispatch the document's onload event.
  DispatchEvent(new web::Event(base::Tokens::load()));

  // Set document load timing info's load event end time after user agent
  // completes handling the load event for the document.
  if (is_performance_valid) {
    document_load_timing_info_.load_event_end = performance->Now();
  }

  // After all JavaScript OnLoad event handlers have executed, signal to let
  // any Document observers know that a load event has occurred.
  SignalOnLoadToObservers();
}

void Document::UpdateStyleSheets() {
  if (are_style_sheets_dirty_) {
    // "Each Document has an associated list of zero or more CSS style sheets,
    // named the document CSS style sheets. This is an ordered list that
    // contains all CSS style sheets associated with the Document, in tree
    // order..."
    // https://drafts.csswg.org/cssom/#document-css-style-sheets
    // See also:
    //   https://www.w3.org/TR/html4/present/styles.html#h-14.4
    cssom::StyleSheetVector style_sheet_vector;
    for (Element* child = first_element_child(); child;
         child = child->next_element_sibling()) {
      child->CollectStyleSheetsOfElementAndDescendants(&style_sheet_vector);
    }

    // Check for the removal or reordering of any of the pre-existing style
    // sheets. In either of these cases, the selector tree must be recreated.
    if (style_sheets_->length() > style_sheet_vector.size()) {
      should_recreate_selector_tree_ = true;
    }
    for (unsigned int style_sheet_index = 0;
         !should_recreate_selector_tree_ &&
         style_sheet_index < style_sheets_->length();
         ++style_sheet_index) {
      if (style_sheets_->Item(style_sheet_index)->AsCSSStyleSheet().get() !=
          style_sheet_vector[style_sheet_index]->AsCSSStyleSheet().get()) {
        should_recreate_selector_tree_ = true;
      }
    }

    style_sheets_ = new cssom::StyleSheetList(style_sheet_vector, this);
    are_style_sheets_dirty_ = false;
  }
}

void Document::UpdateMediaRules() {
  TRACE_EVENT0("cobalt::dom", "Document::UpdateMediaRules()");
  if (viewport_size_) {
    if (user_agent_style_sheet_) {
      user_agent_style_sheet_->EvaluateMediaRules(*viewport_size_);
    }
    for (unsigned int style_sheet_index = 0;
         style_sheet_index < style_sheets_->length(); ++style_sheet_index) {
      scoped_refptr<cssom::CSSStyleSheet> css_style_sheet =
          style_sheets_->Item(style_sheet_index)->AsCSSStyleSheet();
      css_style_sheet->EvaluateMediaRules(*viewport_size_);
    }
  }
}

void Document::UpdateFontFaces() {
  TRACE_EVENT0("cobalt::dom", "Document::UpdateFontFaces()");
  if (are_font_faces_dirty_) {
    FontFaceUpdater font_face_updater(location_->url(), font_cache_.get());
    font_face_updater.ProcessCSSStyleSheet(user_agent_style_sheet_);
    font_face_updater.ProcessStyleSheetList(style_sheets());
    are_font_faces_dirty_ = false;
  }
}

void Document::UpdateKeyframes() {
  TRACE_EVENT0("cobalt::layout", "Document::UpdateKeyframes()");
  if (are_keyframes_dirty_) {
    KeyframesMapUpdater keyframes_map_updater(&keyframes_map_);
    keyframes_map_updater.ProcessCSSStyleSheet(user_agent_style_sheet_);
    keyframes_map_updater.ProcessStyleSheetList(style_sheets());
    are_keyframes_dirty_ = false;

    // This should eventually be altered to only invalidate the tree when the
    // the keyframes map changed.
    scoped_refptr<HTMLHtmlElement> current_html = html();
    if (current_html) {
      current_html->InvalidateComputedStylesOfNodeAndDescendants();
    }
  }
}

}  // namespace dom
}  // namespace cobalt
