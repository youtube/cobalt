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

#include "cobalt/browser/browser_module.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/source_location.h"
#include "cobalt/base/tokens.h"
#include "cobalt/browser/resource_provider_array_buffer_allocator.h"
#include "cobalt/browser/screen_shot_writer.h"
#include "cobalt/browser/storage_upgrade_handler.h"
#include "cobalt/browser/switches.h"
#include "cobalt/dom/csp_delegate_factory.h"
#include "cobalt/dom/keycode.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/input/input_device_manager_fuzzer.h"

namespace cobalt {
namespace browser {
namespace {

// This constant defines the maximum rate at which the layout engine will
// refresh over time.  Since there is little benefit in performing a layout
// faster than the display's refresh rate, we set this to 60Hz.
const float kLayoutMaxRefreshFrequencyInHz = 60.0f;

// TODO: Subscribe to viewport size changes.

#if defined(ENABLE_DEBUG_CONSOLE)

const char kFuzzerToggleCommand[] = "fuzzer_toggle";
const char kFuzzerToggleCommandShortHelp[] = "Toggles the input fuzzer on/off.";
const char kFuzzerToggleCommandLongHelp[] =
    "Each time this is called, it will toggle whether the input fuzzer is "
    "activated or not.  While activated, input will constantly and randomly be "
    "generated and passed directly into the main web module.";

#if defined(ENABLE_SCREENSHOT)
// Command to take a screenshot.
const char kScreenshotCommand[] = "screenshot";

// Help strings for the navigate command.
const char kScreenshotCommandShortHelp[] = "Takes a screenshot.";
const char kScreenshotCommandLongHelp[] =
    "Creates a screenshot of the most recent layout tree and writes it "
    "to disk. Logs the filename of the screenshot to the console when done.";
#endif  // defined(ENABLE_SCREENSHOT)

#if defined(ENABLE_SCREENSHOT)
void ScreenshotCompleteCallback(const FilePath& output_path) {
  DLOG(INFO) << "Screenshot written to " << output_path.value();
}

void OnScreenshotMessage(BrowserModule* browser_module,
                         const std::string& message) {
  UNREFERENCED_PARAMETER(message);
  FilePath dir;
  if (!PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &dir)) {
    NOTREACHED() << "Failed to get debug out directory.";
  }

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  DCHECK(exploded.HasValidValues());
  std::string screenshot_file_name =
      StringPrintf("screenshot-%04d-%02d-%02d_%02d-%02d-%02d.png",
                   exploded.year, exploded.month, exploded.day_of_month,
                   exploded.hour, exploded.minute, exploded.second);

  FilePath output_path = dir.Append(screenshot_file_name);
  browser_module->RequestScreenshotToFile(
      output_path, base::Bind(&ScreenshotCompleteCallback, output_path));
}
#endif  // defined(ENABLE_SCREENSHOT)

#endif  // defined(ENABLE_DEBUG_CONSOLE)

scoped_refptr<script::Wrappable> CreateH5VCC(
    const h5vcc::H5vcc::Settings& settings) {
  return scoped_refptr<script::Wrappable>(new h5vcc::H5vcc(settings));
}

}  // namespace

BrowserModule::BrowserModule(const GURL& url,
                             system_window::SystemWindow* system_window,
                             account::AccountManager* account_manager,
                             const Options& options)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          weak_this_(weak_ptr_factory_.GetWeakPtr())),
      self_message_loop_(MessageLoop::current()),
      storage_manager_(
          scoped_ptr<StorageUpgradeHandler>(new StorageUpgradeHandler(url))
              .PassAs<storage::StorageManager::UpgradeHandler>(),
          options.storage_manager_options),
#if defined(OS_STARBOARD)
      is_rendered_(false),
#endif  // OS_STARBOARD
      renderer_module_(system_window, options.renderer_module_options),
#if defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
      array_buffer_allocator_(new ResourceProviderArrayBufferAllocator(
          renderer_module_.pipeline()->GetResourceProvider())),
      array_buffer_cache_(new dom::ArrayBuffer::Cache(3 * 1024 * 1024)),
#endif  // defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
      media_module_(media::MediaModule::Create(
          system_window, renderer_module_.render_target()->GetSize(),
          renderer_module_.pipeline()->GetResourceProvider(),
          options.media_module_options)),
      network_module_(&storage_manager_, system_window->event_dispatcher(),
                      options.network_module_options),
      render_tree_combiner_(&renderer_module_,
                            renderer_module_.render_target()->GetSize()),
#if defined(ENABLE_SCREENSHOT)
      screen_shot_writer_(new ScreenShotWriter(renderer_module_.pipeline())),
#endif  // defined(ENABLE_SCREENSHOT)
      web_module_loaded_(true /* manually_reset */,
                         false /* initially_signalled */),
      web_module_recreated_callback_(options.web_module_recreated_callback),
#if defined(ENABLE_DEBUG_CONSOLE)
      ALLOW_THIS_IN_INITIALIZER_LIST(fuzzer_toggle_command_handler_(
          kFuzzerToggleCommand,
          base::Bind(&BrowserModule::OnFuzzerToggle, base::Unretained(this)),
          kFuzzerToggleCommandShortHelp, kFuzzerToggleCommandLongHelp)),
#if defined(ENABLE_SCREENSHOT)
      ALLOW_THIS_IN_INITIALIZER_LIST(screenshot_command_handler_(
          kScreenshotCommand,
          base::Bind(&OnScreenshotMessage, base::Unretained(this)),
          kScreenshotCommandShortHelp, kScreenshotCommandLongHelp)),
#endif  // defined(ENABLE_SCREENSHOT)
#endif  // defined(ENABLE_DEBUG_CONSOLE)
      ALLOW_THIS_IN_INITIALIZER_LIST(
          h5vcc_url_handler_(this, system_window, account_manager)),
      web_module_options_(options.web_module_options),
      has_resumed_(true, false),
      will_quit_(false),
      suspended_(false) {
  // Setup our main web module to have the H5VCC API injected into it.
  DCHECK(!ContainsKey(web_module_options_.injected_window_attributes, "h5vcc"));
  h5vcc::H5vcc::Settings h5vcc_settings;
  h5vcc_settings.media_module = media_module_.get();
  h5vcc_settings.network_module = &network_module_;
  h5vcc_settings.account_manager = account_manager;
  h5vcc_settings.event_dispatcher = system_window->event_dispatcher();
  h5vcc_settings.initial_deep_link = options.initial_deep_link;
  web_module_options_.injected_window_attributes["h5vcc"] =
      base::Bind(&CreateH5VCC, h5vcc_settings);

#if defined(ENABLE_DEBUG_CONSOLE) && defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInputFuzzer)) {
    OnFuzzerToggle(std::string());
  }
#endif  // ENABLE_DEBUG_CONSOLE && ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  input_device_manager_ = input::InputDeviceManager::CreateFromWindow(
      base::Bind(&BrowserModule::OnKeyEventProduced, base::Unretained(this)),
      system_window);

#if defined(ENABLE_DEBUG_CONSOLE)
  debug_console_.reset(new DebugConsole(
      base::Bind(&BrowserModule::QueueOnDebugConsoleRenderTreeProduced,
                 base::Unretained(this)),
      media_module_.get(), &network_module_,
      renderer_module_.render_target()->GetSize(),
      renderer_module_.pipeline()->GetResourceProvider(),
      kLayoutMaxRefreshFrequencyInHz,
      base::Bind(&BrowserModule::GetDebugServer, base::Unretained(this))));
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  // Always render the debug console. It will draw nothing if disabled.
  // This setting is ignored if ENABLE_DEBUG_CONSOLE is not defined.
  // TODO: Render tree combiner should probably be refactored.
  render_tree_combiner_.set_render_debug_console(true);

  // Synchronously construct our WebModule object.
  NavigateInternal(url);
  DCHECK(web_module_);
}

BrowserModule::~BrowserModule() {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
}

void BrowserModule::Navigate(const GURL& url) {
  // Always post this as a task in case this is being called from the WebModule.
  self_message_loop_->PostTask(
      FROM_HERE, base::Bind(&BrowserModule::NavigateInternal, weak_this_, url));
}

void BrowserModule::Reload() {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(web_module_);
  web_module_->ExecuteJavascript(
      "location.reload();",
      base::SourceLocation("[object BrowserModule]", 1, 1));
}

void BrowserModule::NavigateInternal(const GURL& url) {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);

  web_module_loaded_.Reset();

  // First try the registered handlers (e.g. for h5vcc://). If one of these
  // handles the URL, we don't use the web module.
  if (TryURLHandlers(url)) {
    return;
  }

  // Destroy old WebModule first, so we don't get a memory high-watermark after
  // the second WebModule's constructor runs, but before scoped_ptr::reset() is
  // run.
  web_module_.reset(NULL);

  // Show a splash screen while we're waiting for the web page to load.
  const math::Size& viewport_size = renderer_module_.render_target()->GetSize();
  DestroySplashScreen();
  splash_screen_.reset(
      new SplashScreen(base::Bind(&BrowserModule::QueueOnRenderTreeProduced,
                                  base::Unretained(this)),
                       &network_module_, viewport_size,
                       renderer_module_.pipeline()->GetResourceProvider(),
                       kLayoutMaxRefreshFrequencyInHz));

  // Create new WebModule.
#if !defined(COBALT_FORCE_CSP)
  web_module_options_.csp_insecure_allowed_token =
      dom::CspDelegateFactory::GetInsecureAllowedToken();
#endif
  WebModule::Options options(web_module_options_);
  options.navigation_callback =
      base::Bind(&BrowserModule::Navigate, base::Unretained(this));
  options.loaded_callbacks.push_back(
      base::Bind(&BrowserModule::OnLoad, base::Unretained(this)));
#if defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
  options.dom_settings_options.array_buffer_allocator =
      array_buffer_allocator_.get();
  options.dom_settings_options.array_buffer_cache = array_buffer_cache_.get();
#endif  // defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
  options.image_cache_capacity_multiplier_when_playing_video =
      COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO;
  web_module_.reset(new WebModule(
      url, base::Bind(&BrowserModule::QueueOnRenderTreeProduced,
                      base::Unretained(this)),
      base::Bind(&BrowserModule::OnError, base::Unretained(this)),
      base::Bind(&BrowserModule::OnWindowClose, base::Unretained(this)),
      media_module_.get(), &network_module_, viewport_size,
      renderer_module_.pipeline()->GetResourceProvider(),
      kLayoutMaxRefreshFrequencyInHz, options));
  if (!web_module_recreated_callback_.is_null()) {
    web_module_recreated_callback_.Run();
  }
}

void BrowserModule::OnLoad() {
  // Repost to our own message loop if necessary. This also prevents
  // asynchonrous access to this object by |web_module_| during destruction.
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnLoad, weak_this_));
    return;
  }

  DestroySplashScreen();
  web_module_loaded_.Signal();
}

bool BrowserModule::WaitForLoad(const base::TimeDelta& timeout) {
  return web_module_loaded_.TimedWait(timeout);
}

#if defined(ENABLE_SCREENSHOT)
void BrowserModule::RequestScreenshotToFile(const FilePath& path,
                                            const base::Closure& done_cb) {
  screen_shot_writer_->RequestScreenshot(path, done_cb);
}

void BrowserModule::RequestScreenshotToBuffer(
    const ScreenShotWriter::PNGEncodeCompleteCallback&
        encode_complete_callback) {
  screen_shot_writer_->RequestScreenshotToMemory(encode_complete_callback);
}
#endif

void BrowserModule::ProcessRenderTreeSubmissionQueue() {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::ProcessRenderTreeSubmissionQueue()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  render_tree_submission_queue_.ProcessAll();
}

void BrowserModule::QueueOnRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::QueueOnRenderTreeProduced()");
  render_tree_submission_queue_.AddMessage(
      base::Bind(&BrowserModule::OnRenderTreeProduced, base::Unretained(this),
                 layout_results));
  self_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::ProcessRenderTreeSubmissionQueue, weak_this_));
}

void BrowserModule::OnRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnRenderTreeProduced()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);

  renderer::Submission renderer_submission(layout_results.render_tree,
                                           layout_results.layout_time);
#if defined(OS_STARBOARD)
  renderer_submission.on_rasterized_callback = base::Bind(
      &BrowserModule::OnRendererSubmissionRasterized, base::Unretained(this));
#endif  // OS_STARBOARD
  render_tree_combiner_.UpdateMainRenderTree(renderer_submission);

#if defined(ENABLE_SCREENSHOT)
  screen_shot_writer_->SetLastPipelineSubmission(renderer::Submission(
      layout_results.render_tree, layout_results.layout_time));
#endif
}

void BrowserModule::OnWindowClose() {
#if defined(ENABLE_DEBUG_CONSOLE)
  if (input_device_manager_fuzzer_) {
    return;
  }
#endif

#if defined(OS_STARBOARD)
  SbSystemRequestStop(0);
#else
  LOG(WARNING) << "window.close is not supported on this platform.";
#endif
}

#if defined(ENABLE_DEBUG_CONSOLE)
void BrowserModule::OnFuzzerToggle(const std::string& message) {
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnFuzzerToggle, weak_this_, message));
    return;
  }

  if (!input_device_manager_fuzzer_) {
    // Wire up the input fuzzer key generator to the keyboard event callback.
    input_device_manager_fuzzer_ = scoped_ptr<input::InputDeviceManager>(
        new input::InputDeviceManagerFuzzer(
            base::Bind(&BrowserModule::InjectKeyEventToMainWebModule,
                       base::Unretained(this))));
  } else {
    input_device_manager_fuzzer_.reset();
  }
}

void BrowserModule::QueueOnDebugConsoleRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::QueueOnDebugConsoleRenderTreeProduced()");
  render_tree_submission_queue_.AddMessage(
      base::Bind(&BrowserModule::OnDebugConsoleRenderTreeProduced,
                 base::Unretained(this), layout_results));
  self_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::ProcessRenderTreeSubmissionQueue, weak_this_));
}

void BrowserModule::OnDebugConsoleRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnDebugConsoleRenderTreeProduced()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);

  render_tree_combiner_.UpdateDebugConsoleRenderTree(renderer::Submission(
      layout_results.render_tree, layout_results.layout_time));
}

#endif  // defined(ENABLE_DEBUG_CONSOLE)

void BrowserModule::OnKeyEventProduced(const dom::KeyboardEvent::Data& event) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnKeyEventProduced()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnKeyEventProduced, weak_this_, event));
    return;
  }

  // Filter the key event.
  if (!FilterKeyEvent(event)) {
    return;
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  trace_manager.OnKeyEventProduced();
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  InjectKeyEventToMainWebModule(event);
}

void BrowserModule::InjectKeyEventToMainWebModule(
    const dom::KeyboardEvent::Data& event) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::InjectKeyEventToMainWebModule()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::InjectKeyEventToMainWebModule,
                              weak_this_, event));
    return;
  }

  DCHECK(web_module_);
  web_module_->InjectKeyboardEvent(event);
}

void BrowserModule::OnError(const GURL& url, const std::string& error) {
  LOG(ERROR) << error;
  std::string url_string = "h5vcc://network-failure";

  // Retry the current URL.
  url_string += "?retry-url=" + url.spec();

  Navigate(GURL(url_string));
}

bool BrowserModule::FilterKeyEvent(const dom::KeyboardEvent::Data& event) {
  // Check for hotkeys first. If it is a hotkey, no more processing is needed.
  if (!FilterKeyEventForHotkeys(event)) {
    return false;
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  // If the debug console is fully visible, it gets the next chance to handle
  // key events.
  if (debug_console_->GetMode() >= debug::DebugHub::kDebugConsoleOn) {
    if (!debug_console_->FilterKeyEvent(event)) {
      return false;
    }
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  return true;
}

bool BrowserModule::FilterKeyEventForHotkeys(
    const dom::KeyboardEvent::Data& event) {
#if !defined(ENABLE_DEBUG_CONSOLE)
  UNREFERENCED_PARAMETER(event);
#else
  if (event.key_code == dom::keycode::kF1 ||
      (event.modifiers & dom::UIEventWithKeyState::kCtrlKey &&
       event.key_code == dom::keycode::kO)) {
    if (event.type == dom::KeyboardEvent::kTypeKeyDown) {
      // Ctrl+O toggles the debug console display.
      debug_console_->CycleMode();
    }
    return false;
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  return true;
}

void BrowserModule::AddURLHandler(
    const URLHandler::URLHandlerCallback& callback) {
  url_handlers_.push_back(callback);
}

void BrowserModule::RemoveURLHandler(
    const URLHandler::URLHandlerCallback& callback) {
  for (URLHandlerCollection::iterator iter = url_handlers_.begin();
       iter != url_handlers_.end(); ++iter) {
    if (iter->Equals(callback)) {
      url_handlers_.erase(iter);
      return;
    }
  }
}

bool BrowserModule::TryURLHandlers(const GURL& url) {
  for (URLHandlerCollection::const_iterator iter = url_handlers_.begin();
       iter != url_handlers_.end(); ++iter) {
    if (iter->Run(url)) {
      return true;
    }
  }

  // No registered handler handled the URL, let the caller handle it.
  return false;
}

void BrowserModule::DestroySplashScreen() { splash_screen_.reset(NULL); }

#if defined(ENABLE_WEBDRIVER)
scoped_ptr<webdriver::SessionDriver> BrowserModule::CreateSessionDriver(
    const webdriver::protocol::SessionId& session_id) {
  return make_scoped_ptr(new webdriver::SessionDriver(
      session_id,
      base::Bind(&BrowserModule::CreateWindowDriver, base::Unretained(this)),
      base::Bind(&BrowserModule::WaitForLoad, base::Unretained(this))));
}

scoped_ptr<webdriver::WindowDriver> BrowserModule::CreateWindowDriver(
    const webdriver::protocol::WindowId& window_id) {
  // Repost to our message loop to ensure synchronous access to |web_module_|.
  scoped_ptr<webdriver::WindowDriver> window_driver;
  self_message_loop_->PostTask(
      FROM_HERE, base::Bind(&BrowserModule::CreateWindowDriverInternal,
                            base::Unretained(this), window_id,
                            base::Unretained(&window_driver)));

  // Wait for the result and return it.
  base::WaitableEvent got_window_driver(true, false);
  self_message_loop_->PostTask(
      FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&got_window_driver)));
  got_window_driver.Wait();
  DCHECK(window_driver);
  return window_driver.Pass();
}

void BrowserModule::CreateWindowDriverInternal(
    const webdriver::protocol::WindowId& window_id,
    scoped_ptr<webdriver::WindowDriver>* out_window_driver) {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(web_module_);
  *out_window_driver = web_module_->CreateWindowDriver(window_id);
}
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUG_CONSOLE)
debug::DebugServer* BrowserModule::GetDebugServer() {
  // Repost to our message loop to ensure synchronous access to |web_module_|.
  debug::DebugServer* debug_server = NULL;
  self_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::GetDebugServerInternal, base::Unretained(this),
                 base::Unretained(&debug_server)));

  // Wait for the result and return it.
  base::WaitableEvent got_debug_server(true, false);
  self_message_loop_->PostTask(FROM_HERE,
                               base::Bind(&base::WaitableEvent::Signal,
                                          base::Unretained(&got_debug_server)));
  got_debug_server.Wait();
  DCHECK(debug_server);
  return debug_server;
}

void BrowserModule::GetDebugServerInternal(
    debug::DebugServer** out_debug_server) {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(web_module_);
  *out_debug_server = web_module_->GetDebugServer();
}
#endif  // ENABLE_DEBUG_CONSOLE

void BrowserModule::SetProxy(const std::string& proxy_rules) {
  // NetworkModule will ensure this happens on the correct thread.
  network_module_.SetProxy(proxy_rules);
}

void BrowserModule::Suspend() {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(!suspended_);

// First release the resource provider used by all of our web modules.
#if defined(ENABLE_DEBUG_CONSOLE)
  if (debug_console_) {
    debug_console_->Suspend();
  }
#endif
  if (splash_screen_) {
    splash_screen_->Suspend();
  }
  if (web_module_) {
    web_module_->Suspend();
  }

  // Flush out any submitted render trees pushed since we started shutting down
  // the web modules above.
  render_tree_submission_queue_.ProcessAll();

#if defined(ENABLE_SCREENSHOT)
  // The screenshot writer may be holding on to a reference to a render tree
  // which could in turn be referencing resources like images, so clear that
  // out.
  screen_shot_writer_->ClearLastPipelineSubmission();
#endif

  // Clear out the render tree combiner so that it doesn't hold on to any
  // render tree resources either.
  render_tree_combiner_.Reset();

  // Place the renderer module into a suspended state where it releases all its
  // graphical resources.
  renderer_module_.Suspend();

  suspended_ = true;
}

void BrowserModule::Resume() {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(suspended_);

  renderer_module_.Resume();

  // Note that at this point, it is probable that this resource provider is
  // different than the one that was managed in the associated call to
  // Suspend().
  render_tree::ResourceProvider* resource_provider =
      renderer_module_.pipeline()->GetResourceProvider();

#if defined(ENABLE_DEBUG_CONSOLE)
  if (debug_console_) {
    debug_console_->Resume(resource_provider);
  }
#endif
  if (splash_screen_) {
    splash_screen_->Resume(resource_provider);
  }
  if (web_module_) {
    web_module_->Resume(resource_provider);
  }

  suspended_ = false;
}

#if defined(OS_STARBOARD)
void BrowserModule::OnRendererSubmissionRasterized() {
  if (!is_rendered_) {
    // Hide the system splash screen when the first render has completed.
    is_rendered_ = true;
    SbSystemHideSplashScreen();
  }
}
#endif  // OS_STARBOARD

}  // namespace browser
}  // namespace cobalt
