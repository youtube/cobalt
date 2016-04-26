/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/web_module.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stringprintf.h"
#include "cobalt/base/tokens.h"
#include "cobalt/browser/switches.h"
#include "cobalt/debug/debug_server_builder.h"
#include "cobalt/dom/storage.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/storage/storage_manager.h"

namespace cobalt {
namespace browser {

namespace {

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
// Help string for the 'partial_layout' command.
const char kPartialLayoutCommandShortHelp[] =
    "Controls partial layout: on | off | wipe | wipe,off";
const char kPartialLayoutCommandLongHelp[] =
    "Controls partial layout.\n"
    "\n"
    "Syntax:\n"
    "  debug.partial_layout('CMD [, CMD ...]')\n"
    "\n"
    "Where CMD can be:\n"
    "  on   : turn partial layout on.\n"
    "  off  : turn partial layout off.\n"
    "  wipe : wipe the box tree.\n"
    "\n"
    "Example:\n"
    "  debug.partial_layout('off,wipe')\n"
    "\n"
    "To wipe the box tree and turn partial layout off.";
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

}  // namespace

class WebModule::DocumentLoadedObserver : public dom::DocumentObserver {
 public:
  typedef std::vector<base::Closure> ClosureVector;
  explicit DocumentLoadedObserver(const ClosureVector& loaded_callbacks)
      : loaded_callbacks_(loaded_callbacks) {}
  // Called at most once, when document and all referred resources are loaded.
  void OnLoad() OVERRIDE {
    for (size_t i = 0; i < loaded_callbacks_.size(); ++i) {
      loaded_callbacks_[i].Run();
    }
  }

  void OnMutation() OVERRIDE{};

 private:
  ClosureVector loaded_callbacks_;
};

WebModule::WebModule(
    const GURL& initial_url,
    const OnRenderTreeProducedCallback& render_tree_produced_callback,
    const base::Callback<void(const GURL&, const std::string&)>& error_callback,
    media::MediaModule* media_module, network::NetworkModule* network_module,
    const math::Size& window_dimensions,
    render_tree::ResourceProvider* resource_provider, float layout_refresh_rate,
    const Options& options)
    : name_(options.name),
      // TODO(***REMOVED***) This assumes the web module runs in the message loop
      // current when it was created. If that changes, we must change this.
      self_message_loop_(MessageLoop::current()),
      error_callback_(error_callback),
      css_parser_(css_parser::Parser::Create()),
      ALLOW_THIS_IN_INITIALIZER_LIST(dom_parser_(new dom_parser::Parser(
          base::Bind(&WebModule::OnError, base::Unretained(this))))),
      fetcher_factory_(new loader::FetcherFactory(network_module,
                                                  options.extra_web_file_dir)),
      image_cache_(loader::image::CreateImageCache(
          base::StringPrintf("%s.ImageCache", name_.c_str()),
          options.image_cache_capacity, resource_provider,
          fetcher_factory_.get())),
      remote_font_cache_(loader::font::CreateRemoteFontCache(
          base::StringPrintf("%s.RemoteFontCache", name_.c_str()),
          kRemoteFontCacheCapacity, resource_provider, fetcher_factory_.get())),
      local_storage_database_(network_module->storage_manager()),
      javascript_engine_(script::JavaScriptEngine::CreateEngine()),
      global_object_proxy_(javascript_engine_->CreateGlobalObjectProxy()),
      execution_state_(
          script::ExecutionState::CreateExecutionState(global_object_proxy_)),
      script_runner_(
          script::ScriptRunner::CreateScriptRunner(global_object_proxy_)),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new dom::Window(
          window_dimensions.width(), window_dimensions.height(),
          css_parser_.get(), dom_parser_.get(), fetcher_factory_.get(),
          resource_provider, image_cache_.get(), remote_font_cache_.get(),
          &local_storage_database_, media_module, execution_state_.get(),
          script_runner_.get(), &media_source_registry_, initial_url,
          network_module->GetUserAgent(), network_module->preferred_language(),
          options.navigation_callback,
          base::Bind(&WebModule::OnError, base::Unretained(this)),
          network_module->cookie_jar(), network_module->GetPostSender(),
          options.location_policy, options.csp_enforcement_mode,
          base::Bind(&WebModule::OnCspPolicyChanged, base::Unretained(this))))),
      window_weak_(base::AsWeakPtr(window_.get())),
      environment_settings_(new dom::DOMSettings(
          fetcher_factory_.get(), network_module, window_,
          &media_source_registry_, javascript_engine_.get(),
          global_object_proxy_.get(), options.dom_settings_options)),
      render_tree_produced_callback_(render_tree_produced_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(layout_manager_(
          window_.get(),
          base::Bind(&WebModule::OnRenderTreeProduced, base::Unretained(this)),
          options.layout_trigger, layout_refresh_rate,
          network_module->preferred_language()))
#if defined(ENABLE_DEBUG_CONSOLE)
      ,
      debug_overlay_(render_tree_produced_callback),
      resource_provider_(resource_provider)
#endif  // ENABLE_DEBUG_CONSOLE
{
  global_object_proxy_->CreateGlobalObject(window_,
                                           environment_settings_.get());

#if !defined(COBALT_FORCE_CSP)
  if (options.csp_enforcement_mode == dom::kCspEnforcementDisable) {
    // If CSP is disabled, enable eval(). Otherwise, it will be enabled by
    // a CSP directive.
    global_object_proxy_->EnableEval();
  }
#endif

  global_object_proxy_->SetReportEvalCallback(
      base::Bind(&dom::CspDelegate::ReportEval,
                 base::Unretained(window_->document()->csp_delegate())));

  InjectCustomWindowAttributes(options.injected_window_attributes);

  if (!options.loaded_callbacks.empty()) {
    document_load_observer_.reset(
        new DocumentLoadedObserver(options.loaded_callbacks));
    window_->document()->AddObserver(document_load_observer_.get());
  }

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kPartialLayout)) {
    const std::string partial_layout_string =
        command_line->GetSwitchValueASCII(browser::switches::kPartialLayout);
    OnPartialLayoutConsoleCommandReceived(partial_layout_string);
  }
  partial_layout_command_handler_.reset(
      new base::ConsoleCommandManager::CommandHandler(
          browser::switches::kPartialLayout,
          base::Bind(&WebModule::OnPartialLayoutConsoleCommandReceived,
                     base::Unretained(this)),
          kPartialLayoutCommandShortHelp, kPartialLayoutCommandLongHelp));
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
}

WebModule::~WebModule() {
  DCHECK(thread_checker_.CalledOnValidThread());
  global_object_proxy_->SetReportEvalCallback(base::Closure());
  window_->DispatchEvent(new dom::Event(base::Tokens::unload()));
}

void WebModule::InjectKeyboardEvent(const dom::KeyboardEvent::Data& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectKeyboardEvent()", "type",
               event.type);

  // Repost to this web module's message loop if it was called on another.
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(FROM_HERE,
                                 base::Bind(&WebModule::InjectKeyboardEvent,
                                            base::Unretained(this), event));
    return;
  }

  // Construct the DOM object from the keyboard event builder and inject it
  // into the window.
  scoped_refptr<dom::KeyboardEvent> keyboard_event(
      new dom::KeyboardEvent(event));

  window_->InjectEvent(keyboard_event);
}

std::string WebModule::ExecuteJavascript(
    const std::string& script_utf8,
    const base::SourceLocation& script_location) {
  TRACE_EVENT0("cobalt::browser", "WebModule::ExecuteJavascript()");

  // If this method was called on a message loop different to this WebModule's,
  // post the task to this WebModule's message loop and wait for the result.
  if (MessageLoop::current() != self_message_loop_) {
    base::WaitableEvent got_result(true, false);
    std::string result;
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&WebModule::ExecuteJavascriptInternal,
                              base::Unretained(this), script_utf8,
                              script_location, &got_result, &result));
    got_result.Wait();
    return result;
  }

  return script_runner_->Execute(script_utf8, script_location);
}

void WebModule::ExecuteJavascriptInternal(
    const std::string& script_utf8, const base::SourceLocation& script_location,
    base::WaitableEvent* got_result, std::string* result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *result = script_runner_->Execute(script_utf8, script_location);
  got_result->Signal();
}

void WebModule::OnRenderTreeProduced(const LayoutResults& layout_results) {
#if defined(ENABLE_REMOTE_DEBUGGING)
  debug_overlay_.OnRenderTreeProduced(layout_results);
#else  // ENABLE_REMOTE_DEBUGGING
  render_tree_produced_callback_.Run(layout_results);
#endif  // ENABLE_REMOTE_DEBUGGING
}

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
void WebModule::OnPartialLayoutConsoleCommandReceived(
    const std::string& message) {
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&dom::Document::SetPartialLayout,
                   base::AsWeakPtr<dom::Document>(window_->document()),
                   message));
  } else {
    window_->document()->SetPartialLayout(message);
  }
}
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

void WebModule::OnCspPolicyChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string eval_disabled_message;
  bool allow_eval =
      window_->document()->csp_delegate()->AllowEval(&eval_disabled_message);
  if (allow_eval) {
    global_object_proxy_->EnableEval();
  } else {
    global_object_proxy_->DisableEval(eval_disabled_message);
  }
}

#if defined(ENABLE_WEBDRIVER)
scoped_ptr<webdriver::WindowDriver> WebModule::CreateWindowDriver(
    const webdriver::protocol::WindowId& window_id) {
  // This may be called from a thread other than web_module_message_loop_.
  return make_scoped_ptr(new webdriver::WindowDriver(
      window_id, window_weak_,
      base::Bind(&WebModule::global_object_proxy, base::Unretained(this)),
      self_message_loop_->message_loop_proxy()));
}
#endif

#if defined(ENABLE_DEBUG_CONSOLE)
// May be called from a thread other than |self_message_loop_|.
debug::DebugServer* WebModule::GetDebugServer() {
  // Create the debug server if necessary
  if (!debug_server_) {
    debug::DebugServerBuilder debug_server_builder;
    debug_server_builder.SetConsole(window_->console())
        .SetGlobalObjectProxy(global_object_proxy_)
        .SetMessageLoop(self_message_loop_)
        .SetRenderOverlay(&debug_overlay_)
        .SetResourceProvider(resource_provider_)
        .SetWindow(window_);
    debug_server_ = debug_server_builder.Build();
  }

  DCHECK(debug_server_);
  return debug_server_.get();
}
#endif  // ENABLE_DEBUG_CONSOLE

void WebModule::InjectCustomWindowAttributes(
    const Options::InjectedWindowAttributes& attributes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (Options::InjectedWindowAttributes::const_iterator iter =
           attributes.begin();
       iter != attributes.end(); ++iter) {
    global_object_proxy_->Bind(iter->first, iter->second.Run());
  }
}

}  // namespace browser
}  // namespace cobalt
