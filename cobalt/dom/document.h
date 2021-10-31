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

#ifndef COBALT_DOM_DOCUMENT_H_
#define COBALT_DOM_DOCUMENT_H_

#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/base/clock.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/css_keyframes_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/application_lifecycle_state.h"
#include "cobalt/dom/csp_delegate_type.h"
#include "cobalt/dom/document_ready_state.h"
#include "cobalt/dom/document_timeline.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/intersection_observer_task_manager.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/node.h"
#include "cobalt/dom/pointer_state.h"
#include "cobalt/dom/visibility_state.h"
#include "cobalt/math/size.h"
#include "cobalt/network_bridge/cookie_jar.h"
#include "cobalt/network_bridge/net_poster.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/wrappable.h"
#include "starboard/time.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

class Comment;
class CspDelegate;
class DOMImplementation;
class Element;
class FontCache;
class HTMLBodyElement;
class HTMLCollection;
class HTMLElement;
class HTMLElementContext;
class HTMLHeadElement;
class HTMLHtmlElement;
class HTMLMediaElement;
class HTMLScriptElement;
class Location;
class Text;
class Window;

class DocumentObserver : public base::CheckedObserver {
 public:
  // Called at most once, when document and all referred resources are loaded.
  virtual void OnLoad() = 0;

  // Called each time when the document or one of its descendants is changed.
  virtual void OnMutation() = 0;

  // Called when document.activeElement changes.
  virtual void OnFocusChanged() = 0;

 protected:
  virtual ~DocumentObserver() {}
};

// The Document interface serves as an entry point into the web page's content
// (the DOM tree, including elements such as <head> and <body>) and provides
// functionality which is global to the document.
//   https://www.w3.org/TR/dom/#document
class Document : public Node,
                 public cssom::MutationObserver,
                 public ApplicationLifecycleState::Observer {
 public:
  struct Options {
    Options()
        : window(NULL),
          cookie_jar(NULL),
          csp_enforcement_mode(kCspEnforcementEnable) {}
    explicit Options(const GURL& url_value)
        : url(url_value),
          window(NULL),
          cookie_jar(NULL),
          csp_enforcement_mode(kCspEnforcementEnable) {}
    Options(const GURL& url_value, Window* window,
            const base::Closure& hashchange_callback,
            const scoped_refptr<base::BasicClock>& navigation_start_clock_value,
            const base::Callback<void(const GURL&)>& navigation_callback,
            const scoped_refptr<cssom::CSSStyleSheet> user_agent_style_sheet,
            const base::Optional<cssom::ViewportSize>& viewport_size,
            network_bridge::CookieJar* cookie_jar,
            const network_bridge::PostSender& post_sender,
            csp::CSPHeaderPolicy require_csp,
            CspEnforcementType csp_enforcement_mode,
            const base::Closure& csp_policy_changed_callback,
            int csp_insecure_allowed_token = 0, int dom_max_element_depth = 0)
        : url(url_value),
          window(window),
          hashchange_callback(hashchange_callback),
          navigation_start_clock(navigation_start_clock_value),
          navigation_callback(navigation_callback),
          user_agent_style_sheet(user_agent_style_sheet),
          viewport_size(viewport_size),
          cookie_jar(cookie_jar),
          post_sender(post_sender),
          require_csp(require_csp),
          csp_enforcement_mode(csp_enforcement_mode),
          csp_policy_changed_callback(csp_policy_changed_callback),
          csp_insecure_allowed_token(csp_insecure_allowed_token),
          dom_max_element_depth(dom_max_element_depth) {}

    GURL url;
    Window* window;
    base::Closure hashchange_callback;
    scoped_refptr<base::BasicClock> navigation_start_clock;
    base::Callback<void(const GURL&)> navigation_callback;
    scoped_refptr<cssom::CSSStyleSheet> user_agent_style_sheet;
    base::Optional<cssom::ViewportSize> viewport_size;
    network_bridge::CookieJar* cookie_jar;
    network_bridge::PostSender post_sender;
    csp::CSPHeaderPolicy require_csp;
    CspEnforcementType csp_enforcement_mode;
    base::Closure csp_policy_changed_callback;
    int csp_insecure_allowed_token;
    int dom_max_element_depth;
  };

  Document(HTMLElementContext* html_element_context,
           const Options& options = Options());

  // Web API: Node
  //
  NodeType node_type() const override { return Node::kDocumentNode; }
  base::Token node_name() const override;

  // Web API: Document
  //
  scoped_refptr<DOMImplementation> implementation();
  const std::string& url() const { return location_->url().spec(); }
  const std::string& document_uri() const { return location_->url().spec(); }

  scoped_refptr<Element> document_element() const;
  std::string title() const;

  scoped_refptr<Window> default_view() const;

  scoped_refptr<HTMLCollection> GetElementsByTagName(
      const std::string& local_name) const;
  scoped_refptr<HTMLCollection> GetElementsByClassName(
      const std::string& class_names) const;

  scoped_refptr<Element> CreateElement(const std::string& local_name);
  scoped_refptr<Element> CreateElementNS(const std::string& namespace_uri,
                                         const std::string& local_name);

  scoped_refptr<Text> CreateTextNode(const std::string& data);
  scoped_refptr<Comment> CreateComment(const std::string& data);

  scoped_refptr<Event> CreateEvent(const std::string& interface_name,
                                   script::ExceptionState* exception_state);

  // Web API: NonElementParentNode (implements)
  //   https://www.w3.org/TR/2014/WD-dom-20140710/#interface-nonelementparentnode
  //
  scoped_refptr<Element> GetElementById(const std::string& id) const;

  // Web API: HTML5 (partial interface)
  //   https://www.w3.org/TR/html50/dom.html#the-document-object
  //
  const scoped_refptr<Location>& location() const;

  std::string dir() const;
  void set_dir(const std::string& value);

  scoped_refptr<HTMLBodyElement> body() const;
  void set_body(const scoped_refptr<HTMLBodyElement>& body);

  scoped_refptr<HTMLHeadElement> head() const;

  scoped_refptr<HTMLScriptElement> current_script() const;
  void set_current_script(
      const scoped_refptr<HTMLScriptElement>& current_script);

  // https://www.w3.org/TR/html50/editing.html#dom-document-hasfocus
  bool HasFocus() const;

  scoped_refptr<Element> active_element() const;
  scoped_refptr<HTMLElement> indicated_element() const;

  const EventListenerScriptValue* onreadystatechange() const {
    return GetAttributeEventListener(base::Tokens::readystatechange());
  }
  void set_onreadystatechange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::readystatechange(), event_listener);
  }

  // Web API: CSS Object Model (partial interface)
  //   http://dev.w3.org/csswg/cssom/#extensions-to-the-document-interface
  const scoped_refptr<cssom::StyleSheetList>& style_sheets();

  // Web Animations API
  // https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#extensions-to-the-document-interface
  const scoped_refptr<DocumentTimeline>& timeline() const {
    return default_timeline_;
  }

  // https://www.w3.org/TR/html50/dom.html#dom-document-cookie
  void set_cookie(const std::string& cookie,
                  script::ExceptionState* exception_state);
  std::string cookie(script::ExceptionState* exception_state) const;

  // For Cobalt code use only. Logs warnings instead of raising exceptions.
  void set_cookie(const std::string& cookie);
  std::string cookie() const;

  // Returns the document's ready state, i.e. whether the document's 'load'
  // event has fired yet or not.
  // https://www.w3.org/TR/html50/dom.html#dom-document-readystate
  DocumentReadyState ready_state() const { return ready_state_; }

  // Custom, not in any spec: Node.
  //
  Document* AsDocument() override { return this; }

  void Accept(NodeVisitor* visitor) override;
  void Accept(ConstNodeVisitor* visitor) const override;
  scoped_refptr<Node> Duplicate() const override;

  // Custom, not in any spec.
  //
  virtual bool IsXMLDocument() const { return false; }

  HTMLElementContext* html_element_context() const {
    return html_element_context_;
  }

  FontCache* font_cache() const { return font_cache_.get(); }

  const GURL& url_as_gurl() const { return location_->url(); }

  scoped_refptr<HTMLHtmlElement> html() const;

  // List of scripts that will execute in order as soon as possible.
  //   https://www.w3.org/TR/html50/scripting-1.html#list-of-scripts-that-will-execute-in-order-as-soon-as-possible
  std::deque<HTMLScriptElement*>* scripts_to_be_executed() {
    return &scripts_to_be_executed_;
  }

  cssom::SelectorTree* selector_tree() { return selector_tree_.get(); }

  cssom::RulesWithCascadePrecedence* scratchpad_html_element_matching_rules() {
    return &scratchpad_html_element_matching_rules_;
  }
  cssom::RulesWithCascadePrecedence* scratchpad_pseudo_element_matching_rules(
      PseudoElementType element_type) {
    return &(scratchpad_pseudo_element_matching_rules_[element_type]);
  }

  // Returns a mapping from keyframes name to CSSKeyframesRule.  This can be
  // used to quickly lookup the @keyframes rule given a string identifier.
  const cssom::CSSKeyframesRule::NameMap& keyframes_map() const {
    return keyframes_map_;
  }

  // Returns whether the document has browsing context. Having the browsing
  // context means the document is shown on the screen.
  //   https://www.w3.org/TR/html50/browsers.html#browsing-context
  bool HasBrowsingContext() const { return !!window_; }

  void set_window(Window* window) { window_ = window; }
  const scoped_refptr<Window> window();

  // Sets the active element of the document.
  void SetActiveElement(Element* active_element);

  // Sets the indicated element of the document.
  void SetIndicatedElement(HTMLElement* indicated_element);

  // Count all ongoing loadings, including document itself and its dependent
  // resources, and dispatch OnLoad() if necessary.
  void IncreaseLoadingCounter();
  void DecreaseLoadingCounter();
  void DecreaseLoadingCounterAndMaybeDispatchLoadEvent();

  // Utilities related to DocumentObserver.
  void AddObserver(DocumentObserver* observer);
  void RemoveObserver(DocumentObserver* observer);
  void SignalOnLoadToObservers();

  // Must be called by all descendants of the document on their modification.
  // TODO: Provide more granularity, model after mutation observers
  //       (see https://www.w3.org/TR/dom/#mutation-observers).
  void RecordMutation();

  // Called when the focus changes. This should be called only once when the
  // focus is shifted from one element to another.
  void OnFocusChange();

  // Called when the DOM style sheets changed.
  void OnStyleSheetsModified();

  // From cssom::MutationObserver.
  void OnCSSMutation() override;

  // Called when the DOM is mutated in some way.
  void OnDOMMutation();

  // Called when a new typeface has been loaded.
  void OnTypefaceLoadEvent();

  // Called when the inline style of an element is modified.
  void OnElementInlineStyleMutation();

  // Updates the computed styles of all of this document's HTML elements.
  // Matching rules, media rules, font faces and key frames are also updated.
  void UpdateComputedStyles();

  // Updates the computed styles of the element and all its ancestors.
  // Matching rules, media rules, font faces and key frames are also updated.
  // Returns whether the computed style is valid after the call.
  bool UpdateComputedStyleOnElementAndAncestor(HTMLElement* element);

  // Called periodically to update the UI navigation system.
  void UpdateUiNavigation();

  // Track UI navigation system's focus element.
  const void* ui_nav_focus_element() const { return ui_nav_focus_element_; }
  bool TrySetUiNavFocusElement(const void* focus_element, SbTimeMonotonic time);

  // Track HTML elements that are UI navigation items. This facilitates updating
  // their layout information as needed.
  void AddUiNavigationElement(HTMLElement* element) {
    ui_nav_elements_.insert(element);
  }
  void RemoveUiNavigationElement(HTMLElement* element) {
    ui_nav_elements_.erase(element);
  }
  const std::unordered_set<HTMLElement*>& ui_navigation_elements() const {
    return ui_nav_elements_;
  }
  void set_ui_nav_needs_layout(bool needs_layout) {
    ui_nav_needs_layout_ = needs_layout;
  }
  bool ui_nav_needs_layout() const { return ui_nav_needs_layout_; }

  // Manages the clock used by Web Animations.
  //     https://www.w3.org/TR/web-animations
  // This clock is also used for requestAnimationFrame() callbacks, according
  // to the specification above.
  void SampleTimelineTime();

  const scoped_refptr<base::BasicClock>& navigation_start_clock() const {
    return navigation_start_clock_;
  }

  CspDelegate* csp_delegate() const { return csp_delegate_.get(); }

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  bool partial_layout_is_enabled() { return partial_layout_is_enabled_; }
  void SetPartialLayout(bool enabled);
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

  // Triggers a synchronous layout.
  scoped_refptr<render_tree::Node> DoSynchronousLayoutAndGetRenderTree();
  void DoSynchronousLayout();

  void set_synchronous_layout_callback(
      const base::Closure& synchronous_layout_callback) {
    synchronous_layout_callback_ = synchronous_layout_callback;
  }
  void set_synchronous_layout_and_produce_render_tree_callback(
      const base::Callback<scoped_refptr<render_tree::Node>()>&
          synchronous_layout_and_produce_render_tree_callback) {
    synchronous_layout_and_produce_render_tree_callback_ =
        synchronous_layout_and_produce_render_tree_callback;
  }

  cssom::ViewportSize viewport_size();
  void SetViewport(const cssom::ViewportSize& viewport_size);

  const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
  initial_computed_style_declaration() const {
    return initial_computed_style_declaration_;
  }
  const scoped_refptr<const cssom::CSSComputedStyleData>&
  initial_computed_style_data() const {
    return initial_computed_style_data_;
  }

  int dom_max_element_depth() const { return dom_max_element_depth_; }

  void NotifyUrlChanged(const GURL& url);

  // Updates the selector tree using all the style sheets in the document.
  // Exposed for test purposes.
  void UpdateSelectorTree();

  void PurgeCachedResources();
  void InvalidateLayoutBoxes();

  // Disable just-in-time compilation of JavaScript code.
  void DisableJit();

  // Page Visibility fields.
  bool hidden() const { return visibility_state() == kVisibilityStateHidden; }
  VisibilityState visibility_state() const {
    return application_lifecycle_state()->GetVisibilityState();
  }
  const EventListenerScriptValue* onvisibilitychange() const {
    return GetAttributeEventListener(base::Tokens::visibilitychange());
  }
  void set_onvisibilitychange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::visibilitychange(), event_listener);
  }

  // Page Lifecycle fields.
  const EventListenerScriptValue* onfreeze() const {
    return GetAttributeEventListener(base::Tokens::freeze());
  }
  const EventListenerScriptValue* onresume() const {
    return GetAttributeEventListener(base::Tokens::resume());
  }
  void set_onfreeze(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::freeze(), event_listener);
  }
  void set_onresume(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::resume(), event_listener);
  }

  // ApplicationLifecycleState::Observer implementation.
  void OnWindowFocusChanged(bool has_focus) override;
  void OnVisibilityStateChanged(VisibilityState visibility_state) override;
  void OnFrozennessChanged(bool is_frozen) override;

  bool was_discarded() const { return false; }

  PointerState* pointer_state() { return &pointer_state_; }

  // render_postponed is a Cobalt-specific Web API.
  bool render_postponed() const { return render_postponed_; }

  void set_render_postponed(bool render_postponed);

  // Called when the root element has its offset dimensions requested and is
  // unable to provide them.
  void OnRootElementUnableToProvideOffsetDimensions();

  IntersectionObserverTaskManager* intersection_observer_task_manager() const {
    return intersection_observer_task_manager_;
  }

  DEFINE_WRAPPABLE_TYPE(Document);
  void TraceMembers(script::Tracer* tracer) override;

 protected:
  ~Document() override;

  ApplicationLifecycleState* application_lifecycle_state() {
    return html_element_context_->application_lifecycle_state().get();
  }

  const ApplicationLifecycleState* application_lifecycle_state() const {
    return html_element_context_->application_lifecycle_state().get();
  }

 private:
  void DispatchOnLoadEvent();

  // Updates the style sheets in the document.
  void UpdateStyleSheets();

  // Updates the media rules in all the style sheets in the document.
  void UpdateMediaRules();

  // Updates the font faces in all the style sheets in the document.
  void UpdateFontFaces();

  // Compiles/updates a set of all declared CSS keyframes used to define CSS
  // Animations, using all the style sheets in the document.
  void UpdateKeyframes();

  bool IsCookieAverseDocument() const;

  // Collect HTML media elements for the preparation of changing the
  // frozeness of documents.
  void CollectHTMLMediaElements(
      std::vector<HTMLMediaElement*>* html_media_elements);

  // https://wicg.github.io/page-lifecycle/#changing-frozenness
  void FreezeSteps();

  // https://wicg.github.io/page-lifecycle/#changing-frozenness
  void ResumeSteps();

  // Reference to HTML element context.
  HTMLElementContext* const html_element_context_;

  // Explicitly store a weak pointer to the application lifecycle state object.
  // It is possible that we destroy the application lifecycle state object
  // before Document, during shutdown, so this allows us to handle that
  // situation more gracefully than crashing.
  base::WeakPtr<ApplicationLifecycleState> application_lifecycle_state_;

  // Reference to the associated window object.
  Window* window_;
  // Associated DOM implementation object.
  scoped_refptr<DOMImplementation> implementation_;
  // List of CSS style sheets.
  scoped_refptr<cssom::StyleSheetList> style_sheets_;
  // <script> element whose script is currently being processed, if any.
  scoped_refptr<HTMLScriptElement> current_script_;
  // List of scripts that will execute in order as soon as possible.
  std::deque<HTMLScriptElement*> scripts_to_be_executed_;
  // A mapping from keyframes declaration names to their parsed structure.
  cssom::CSSKeyframesRule::NameMap keyframes_map_;
  // The number of ongoing loadings.
  int loading_counter_;
  // Whether the load event should be dispatched when loading counter hits zero.
  bool should_dispatch_load_event_;
  // Indicates if the document's style sheets need to be re-collected before
  // the next layout.
  bool are_style_sheets_dirty_;
  // Indicates if rule matching/computed style is dirty and needs to be
  // recomputed before the next layout.
  bool is_selector_tree_dirty_;
  bool is_computed_style_dirty_;
  bool are_font_faces_dirty_;
  bool are_keyframes_dirty_;
#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  bool partial_layout_is_enabled_;
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

  // Viewport size.
  base::Optional<cssom::ViewportSize> viewport_size_;
  // Content Security Policy enforcement for this document.
  std::unique_ptr<CspDelegate> csp_delegate_;
  network_bridge::CookieJar* cookie_jar_;
  // Associated location object.
  scoped_refptr<Location> location_;
  // The font cache for this document.
  std::unique_ptr<FontCache> font_cache_;

  // Weak reference to the active element.
  base::WeakPtr<Element> active_element_;
  // Weak reference to the indicated element.
  base::WeakPtr<HTMLElement> indicated_element_;
  // List of document observers.
  base::ObserverList<DocumentObserver> observers_;
  // Selector Tree.
  std::unique_ptr<cssom::SelectorTree> selector_tree_;
  // This is set when the document has a style sheet removed or the order of its
  // style sheets changed. In this case, it is more straightforward to simply
  // recreate the selector tree than to attempt to manage updating all of its
  // internal state.
  bool should_recreate_selector_tree_;
  // Matching rules that are available for temporary operations, so that the
  // vectors don't have to be repeatedly re-allocated during rule matching.
  cssom::RulesWithCascadePrecedence scratchpad_html_element_matching_rules_;
  cssom::RulesWithCascadePrecedence
      scratchpad_pseudo_element_matching_rules_[kMaxPseudoElementType];
  // The document's latest sample from the global clock, used for updating
  // animations.
  const scoped_refptr<base::BasicClock> navigation_start_clock_;
  scoped_refptr<DocumentTimeline> default_timeline_;

  base::Callback<scoped_refptr<render_tree::Node>()>
      synchronous_layout_and_produce_render_tree_callback_;

  base::Closure synchronous_layout_callback_;

  scoped_refptr<cssom::CSSStyleSheet> user_agent_style_sheet_;

  // Computed style of the initial containing block, width and height come from
  // the viewport size.
  scoped_refptr<cssom::CSSComputedStyleDeclaration>
      initial_computed_style_declaration_;
  scoped_refptr<const cssom::CSSComputedStyleData> initial_computed_style_data_;

  // The document's current ready state (e.g. has the 'load' event been fired
  // yet)
  DocumentReadyState ready_state_;

  // The max depth of elements that are guaranteed to be rendered.
  int dom_max_element_depth_;

  // Various state related to pointer and mouse support.
  PointerState pointer_state_;

  // Whether or not rendering is currently postponed.
  bool render_postponed_;

  // Whether or not page lifecycle is currently frozen.
  //   https://wicg.github.io/page-lifecycle/#page-lifecycle
  bool frozenness_;

  // Indicates whether UI navigation focus needs to be updated.
  bool ui_nav_focus_needs_update_ = false;

  // Track the current focus of UI navigation. This is only an identifier and
  // not meant to be dereferenced.
  const void* ui_nav_focus_element_ = nullptr;

  // Since UI navigation involves multiple threads, use a timestamp to help
  // filter out obsolete focus changes.
  SbTimeMonotonic ui_nav_focus_element_update_time_ = 0;

  // Track all HTMLElements in this document which are UI navigation items.
  // These should be raw pointers to avoid affecting the elements' ref counts.
  // The elements will explicitly add and remove themselves from this set.
  std::unordered_set<HTMLElement*> ui_nav_elements_;

  // This specifies whether the UI navigation HTML elements need updating during
  // layout.
  bool ui_nav_needs_layout_ = false;

  scoped_refptr<IntersectionObserverTaskManager>
      intersection_observer_task_manager_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOCUMENT_H_
