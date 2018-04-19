// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/webdriver/web_driver_module.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/values.h"
#include "cobalt/webdriver/dispatcher.h"
#include "cobalt/webdriver/protocol/capabilities.h"
#include "cobalt/webdriver/protocol/window_id.h"
#include "cobalt/webdriver/server.h"
#include "cobalt/webdriver/session_driver.h"
#include "cobalt/webdriver/util/command_result.h"
#include "cobalt/webdriver/util/dispatch_command_factory.h"
#include "cobalt/webdriver/window_driver.h"

namespace cobalt {
namespace webdriver {
namespace {

// Only one session is supported. This is the session ID for that session.
const char kWebDriverSessionId[] = "session-0";

// Variable names for variable path components.
const char kSessionIdVariable[] = ":sessionId";
const char kWindowHandleVariable[] = ":windowHandle";
const char kElementId[] = ":id";
const char kOtherElementId[] = ":other";
const char kAttributeName[] = ":name";
const char kCssPropertyName[] = ":propertyName";
const char kCookieName[] = ":name";

// Error messages related to session creation.
const char kMaxSessionsCreatedMessage[] =
    "Maximum number of sessions have been created.";
const char kUnsupportedCapabilities[] =
    "An unsupported capability was requested.";
const char kUnknownSessionCreationError[] =
    "An unknown error occurred trying to create a new session.";

// Looks up the SessionDriver instance that is mapped to the
// the sessionId variable in the path.
SessionDriver* LookUpSessionDriverOrReturnInvalidResponse(
    const base::Callback<SessionDriver*(const protocol::SessionId&)>&
        get_session_cb,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    WebDriverDispatcher::CommandResultHandler* result_handler) {
  DCHECK(path_variables);
  protocol::SessionId session_id(
      path_variables->GetVariable(kSessionIdVariable));

  SessionDriver* session_driver = get_session_cb.Run(session_id);
  // If there is no session with this ID, then return an error.
  if (!session_driver) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        session_id.id());
  }
  return session_driver;
}

// There are no path variables to look up in this case. Ignore the path
// variables and return the session's current window.
WindowDriver* LookUpCurrentWindowDriver(
    SessionDriver* session_driver,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    WebDriverDispatcher::CommandResultHandler* result_handler) {
  DCHECK(session_driver);
  // If the current window has been closed, an error should be returned, but
  // we don't support closing windows on Cobalt.
  return session_driver->GetCurrentWindow();
}

// There are no path variables to look up in this case. This function simply
// returns the same SessionDriver that was passed in.
SessionDriver* LookUpSessionDriver(
    SessionDriver* session_driver,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    WebDriverDispatcher::CommandResultHandler* result_handler) {
  DCHECK(session_driver);
  return session_driver;
}

// Looks up the WindowDriver instance that is mapped to the
// the sessionId variable in the path.
WindowDriver* LookUpWindowDriverOrReturnInvalidResponse(
    SessionDriver* session_driver,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    WebDriverDispatcher::CommandResultHandler* result_handler) {
  DCHECK(path_variables);
  DCHECK(session_driver);
  protocol::WindowId window_id(
      path_variables->GetVariable(kWindowHandleVariable));
  WindowDriver* window_driver = session_driver->GetWindow(window_id);
  if (!window_driver) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        window_id.id());
  }
  return window_driver;
}

// Looks up the ElementDriver instance that is mapped to the
// the sessionId variable in the path.
ElementDriver* LookUpElementDriverOrReturnInvalidResponse(
    const char* path_variable, SessionDriver* session_driver,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    WebDriverDispatcher::CommandResultHandler* result_handler) {
  DCHECK(path_variables);
  DCHECK(session_driver);
  ElementDriver* element_driver = NULL;
  WindowDriver* window_driver =
      LookUpCurrentWindowDriver(session_driver, path_variables, result_handler);
  if (window_driver) {
    protocol::ElementId element_id(path_variables->GetVariable(path_variable));
    element_driver = window_driver->GetElementDriver(element_id);
    if (!element_driver) {
      result_handler->SendInvalidRequestResponse(
          WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
          element_id.id());
    }
  }
  return element_driver;
}

// Helper struct for getting a PNG screenshot synchronously.
struct ScreenshotResultContext {
  ScreenshotResultContext() : complete_event(true, false) {}
  scoped_refptr<loader::image::EncodedStaticImage> compressed_file;
  base::WaitableEvent complete_event;
};

// Callback function to be called when PNG encoding is complete.
void OnPNGEncodeComplete(ScreenshotResultContext* context,
                         const scoped_refptr<loader::image::EncodedStaticImage>&
                             compressed_image_data) {
  DCHECK(context);
  DCHECK(compressed_image_data->GetImageFormat() ==
         loader::image::EncodedStaticImage::ImageFormat::kPNG);
  context->compressed_file = compressed_image_data;
  context->complete_event.Signal();
}

}  // namespace

#if SB_HAS(IPV6)
const char WebDriverModule::kDefaultListenIp[] = "::";
#else
const char WebDriverModule::kDefaultListenIp[] = "0.0.0.0";
#endif

WebDriverModule::WebDriverModule(
    int server_port, const std::string& listen_ip,
    const CreateSessionDriverCB& create_session_driver_cb,
    const GetScreenshotFunction& get_screenshot_function,
    const SetProxyFunction& set_proxy_function,
    const base::Closure& shutdown_cb)
    : webdriver_thread_("WebDriver thread"),
      create_session_driver_cb_(create_session_driver_cb),
      get_screenshot_function_(get_screenshot_function),
      set_proxy_function_(set_proxy_function),
      shutdown_cb_(shutdown_cb),
      webdriver_dispatcher_(new WebDriverDispatcher()) {
  get_session_driver_ =
      base::Bind(&WebDriverModule::GetSessionDriver, base::Unretained(this));

  typedef util::DispatchCommandFactory<SessionDriver> SessionCommandFactory;
  typedef util::DispatchCommandFactory<WindowDriver> WindowCommandFactory;
  typedef util::DispatchCommandFactory<ElementDriver> ElementCommandFactory;

  scoped_refptr<SessionCommandFactory> session_command_factory(
      new SessionCommandFactory(
          base::Bind(&LookUpSessionDriverOrReturnInvalidResponse,
                     get_session_driver_),
          base::Bind(&LookUpSessionDriver)));

  scoped_refptr<WindowCommandFactory> current_window_command_factory(
      new WindowCommandFactory(
          base::Bind(&LookUpSessionDriverOrReturnInvalidResponse,
                     get_session_driver_),
          base::Bind(&LookUpCurrentWindowDriver)));

  scoped_refptr<WindowCommandFactory> window_command_factory(
      new WindowCommandFactory(
          base::Bind(&LookUpSessionDriverOrReturnInvalidResponse,
                     get_session_driver_),
          base::Bind(&LookUpWindowDriverOrReturnInvalidResponse)));

  scoped_refptr<ElementCommandFactory> element_command_factory(
      new ElementCommandFactory(
          base::Bind(&LookUpSessionDriverOrReturnInvalidResponse,
                     get_session_driver_),
          base::Bind(&LookUpElementDriverOrReturnInvalidResponse, kElementId)));

  // Server commands.
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, "/status",
      base::Bind(&WebDriverModule::GetServerStatus, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost, "/session",
      base::Bind(&WebDriverModule::CreateSession, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, "/sessions",
      base::Bind(&WebDriverModule::GetActiveSessions, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, "/shutdown",
      base::Bind(&WebDriverModule::Shutdown, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kDelete, StringPrintf("/session/%s", kSessionIdVariable),
      base::Bind(&WebDriverModule::DeleteSession, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/screenshot", kSessionIdVariable),
      base::Bind(&WebDriverModule::RequestScreenshot, base::Unretained(this)));

  // Session commands.
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, StringPrintf("/session/%s", kSessionIdVariable),
      session_command_factory->GetCommandHandler(
          base::Bind(&SessionDriver::GetCapabilities)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/window_handle", kSessionIdVariable),
      session_command_factory->GetCommandHandler(
          base::Bind(&SessionDriver::GetCurrentWindowHandle)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/window_handles", kSessionIdVariable),
      session_command_factory->GetCommandHandler(
          base::Bind(&SessionDriver::GetWindowHandles)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/alert_text", kSessionIdVariable),
      session_command_factory->GetCommandHandler(
          base::Bind(&SessionDriver::GetAlertText)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/window", kSessionIdVariable),
      session_command_factory->GetCommandHandler(
          base::Bind(&SessionDriver::SwitchToWindow)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/log/types", kSessionIdVariable),
      session_command_factory->GetCommandHandler(
          base::Bind(&SessionDriver::GetLogTypes)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/log", kSessionIdVariable),
      session_command_factory->GetCommandHandler(
          base::Bind(&SessionDriver::GetLog)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/url", kSessionIdVariable),
      session_command_factory->GetCommandHandler(
          base::Bind(&SessionDriver::Navigate)));

  // Specified window commands.
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/window/%s/size", kSessionIdVariable,
                   kWindowHandleVariable),
      window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::GetWindowSize)));

  // Current window commands.
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/url", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::GetCurrentUrl)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/source", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::GetSource)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/title", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::GetTitle)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/execute", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::Execute)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/execute_async", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::ExecuteAsync)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/element", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::FindElement)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/elements", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::FindElements)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/keys", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::SendKeys)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/element/active", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::GetActiveElement)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/frame", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::SwitchFrame)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/cookie", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::GetAllCookies)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/cookie/%s", kSessionIdVariable, kCookieName),
      base::Bind(&WebDriverModule::GetCookieByName, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/cookie", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::AddCookie)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/moveto", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::MouseMoveTo)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/buttondown", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::MouseButtonDown)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/buttonup", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::MouseButtonUp)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/click", kSessionIdVariable),
      current_window_command_factory->GetCommandHandler(
          base::Bind(&WindowDriver::SendClick)));

  // Element commands.
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, StringPrintf("/session/%s/element/%s/name",
                                          kSessionIdVariable, kElementId),
      element_command_factory->GetCommandHandler(
          base::Bind(&ElementDriver::GetTagName)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, StringPrintf("/session/%s/element/%s/text",
                                          kSessionIdVariable, kElementId),
      element_command_factory->GetCommandHandler(
          base::Bind(&ElementDriver::GetText)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, StringPrintf("/session/%s/element/%s/displayed",
                                          kSessionIdVariable, kElementId),
      element_command_factory->GetCommandHandler(
          base::Bind(&ElementDriver::IsDisplayed)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost, StringPrintf("/session/%s/element/%s/value",
                                           kSessionIdVariable, kElementId),
      element_command_factory->GetCommandHandler(
          base::Bind(&ElementDriver::SendKeys)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost, StringPrintf("/session/%s/element/%s/element",
                                           kSessionIdVariable, kElementId),
      element_command_factory->GetCommandHandler(
          base::Bind(&ElementDriver::FindElement)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost, StringPrintf("/session/%s/element/%s/elements",
                                           kSessionIdVariable, kElementId),
      element_command_factory->GetCommandHandler(
          base::Bind(&ElementDriver::FindElements)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/element/%s/click", kSessionIdVariable,
                   kElementId),
      element_command_factory->GetCommandHandler(
          base::Bind(&ElementDriver::SendClick)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/element/%s/equals/%s", kSessionIdVariable,
                   kElementId, kOtherElementId),
      base::Bind(&WebDriverModule::ElementEquals, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/element/%s/attribute/%s", kSessionIdVariable,
                   kElementId, kAttributeName),
      base::Bind(&WebDriverModule::GetAttribute, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/element/%s/css/%s", kSessionIdVariable,
                   kElementId, kCssPropertyName),
      base::Bind(&WebDriverModule::GetCssProperty, base::Unretained(this)));

  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/timeouts", kSessionIdVariable),
      base::Bind(&WebDriverModule::IgnoreCommand, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/timeouts/async_script", kSessionIdVariable),
      base::Bind(&WebDriverModule::IgnoreCommand, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/timeouts/implicit_wait", kSessionIdVariable),
      base::Bind(&WebDriverModule::IgnoreCommand, base::Unretained(this)));

  // The WebDriver API implementation will be called on the HTTP server thread.
  thread_checker_.DetachFromThread();

  // Start the thread and create the HTTP server on that thread.
  webdriver_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));
  webdriver_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebDriverModule::StartServer,
                            base::Unretained(this), server_port, listen_ip));
}  // NOLINT(readability/fn_size)

WebDriverModule::~WebDriverModule() {
  webdriver_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebDriverModule::StopServer,
                            base::Unretained(this)));
  webdriver_thread_.Stop();
}  // NOLINT(readability/fn_size)

void WebDriverModule::OnWindowRecreated() {
  if (MessageLoop::current() != webdriver_thread_.message_loop()) {
    webdriver_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&WebDriverModule::OnWindowRecreated,
                              base::Unretained(this)));
    return;
  }
  DCHECK(thread_checker_.CalledOnValidThread());
  if (session_) {
    session_->RefreshWindowDriver();
  }
}

void WebDriverModule::StartServer(int server_port,
                                  const std::string& listen_ip) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Create a new WebDriverServer and pass in the Dispatcher.
  webdriver_server_.reset(new WebDriverServer(
      server_port, listen_ip,
      base::Bind(&WebDriverDispatcher::HandleWebDriverServerRequest,
                 base::Unretained(webdriver_dispatcher_.get()))));
}

void WebDriverModule::StopServer() {
  webdriver_server_.reset();
}

SessionDriver* WebDriverModule::GetSessionDriver(
    const protocol::SessionId& session_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SessionDriver* session_driver = NULL;
  if (session_ && (session_->session_id() == session_id)) {
    return session_.get();
  }
  return NULL;
}

// https://code.google.com/p/selenium/wiki/JsonWireProtocol#/status
void WebDriverModule::GetServerStatus(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  result_handler->SendResult(base::nullopt, protocol::Response::kSuccess,
                             protocol::ServerStatus::ToValue(status_));
}

// https://code.google.com/p/selenium/wiki/JsonWireProtocol#/sessions
void WebDriverModule::GetActiveSessions(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<protocol::SessionId> sessions;
  if (session_) {
    sessions.push_back(session_->session_id());
  }
  result_handler->SendResult(base::nullopt, protocol::Response::kSuccess,
                             util::internal::ToValue(sessions));
}

// https://code.google.com/p/selenium/wiki/JsonWireProtocol#GET_/session/:sessionId
void WebDriverModule::CreateSession(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::optional<protocol::RequestedCapabilities> requested_capabilities =
      protocol::RequestedCapabilities::FromValue(parameters);
  if (!requested_capabilities) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidParameters, "");
    return;
  }

  util::CommandResult<protocol::Capabilities> command_result =
      CreateSessionInternal(requested_capabilities.value());

  base::optional<protocol::SessionId> session_id;
  if (command_result.is_success()) {
    session_id = session_->session_id();
  }
  util::internal::ReturnResponse(session_id, command_result,
                                 result_handler.get());
}

// https://code.google.com/p/selenium/wiki/JsonWireProtocol#DELETE_/session/:sessionId
void WebDriverModule::DeleteSession(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (session_) {
    // Extract the sessionId variable from the path
    std::string session_id_variable =
        path_variables->GetVariable(kSessionIdVariable);

    if (session_id_variable == session_->session_id().id()) {
      // If the sessionID matches, the delete the session.
      session_.reset();
    }
  }
  // If the session doesn't exist, then this is a no-op.
  result_handler->SendResult(base::nullopt, protocol::Response::kSuccess,
                             scoped_ptr<base::Value>());
}

// https://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/screenshot
void WebDriverModule::RequestScreenshot(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SessionDriver* session_driver = LookUpSessionDriverOrReturnInvalidResponse(
      base::Bind(&WebDriverModule::GetSessionDriver, base::Unretained(this)),
      path_variables, result_handler.get());
  if (session_driver) {
    typedef util::CommandResult<std::string> CommandResult;

    CommandResult result = RequestScreenshotInternal();
    util::internal::ReturnResponse(session_driver->session_id(), result,
                                   result_handler.get());
  }
}

void WebDriverModule::Shutdown(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // It's expected that the application will terminate, so it's okay to
  // leave the request hanging.
  shutdown_cb_.Run();
}

void WebDriverModule::ElementEquals(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SessionDriver* session_driver = LookUpSessionDriverOrReturnInvalidResponse(
      base::Bind(&WebDriverModule::GetSessionDriver, base::Unretained(this)),
      path_variables, result_handler.get());
  if (!session_driver) {
    return;
  }
  ElementDriver* element_driver = LookUpElementDriverOrReturnInvalidResponse(
      kElementId, session_driver, path_variables, result_handler.get());
  if (!element_driver) {
    return;
  }
  ElementDriver* other_element = LookUpElementDriverOrReturnInvalidResponse(
      kOtherElementId, session_driver, path_variables, result_handler.get());
  if (!other_element) {
    return;
  }

  typedef util::CommandResult<bool> CommandResult;

  CommandResult result = element_driver->Equals(other_element);
  util::internal::ReturnResponse(session_driver->session_id(), result,
                                 result_handler.get());
}

void WebDriverModule::GetAttribute(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SessionDriver* session_driver = LookUpSessionDriverOrReturnInvalidResponse(
      base::Bind(&WebDriverModule::GetSessionDriver, base::Unretained(this)),
      path_variables, result_handler.get());
  if (session_driver) {
    ElementDriver* element_driver = LookUpElementDriverOrReturnInvalidResponse(
        kElementId, session_driver, path_variables, result_handler.get());
    if (element_driver) {
      std::string attribute_name = path_variables->GetVariable(kAttributeName);

      typedef util::CommandResult<base::optional<std::string> > CommandResult;
      CommandResult result = element_driver->GetAttribute(attribute_name);
      util::internal::ReturnResponse(session_driver->session_id(), result,
                                     result_handler.get());
    }
  }
}

void WebDriverModule::GetCssProperty(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SessionDriver* session_driver = LookUpSessionDriverOrReturnInvalidResponse(
      base::Bind(&WebDriverModule::GetSessionDriver, base::Unretained(this)),
      path_variables, result_handler.get());
  if (session_driver) {
    ElementDriver* element_driver = LookUpElementDriverOrReturnInvalidResponse(
        kElementId, session_driver, path_variables, result_handler.get());
    if (element_driver) {
      std::string property_name = path_variables->GetVariable(kCssPropertyName);

      typedef util::CommandResult<std::string> CommandResult;
      CommandResult result = element_driver->GetCssProperty(property_name);
      util::internal::ReturnResponse(session_driver->session_id(), result,
                                     result_handler.get());
    }
  }
}

void WebDriverModule::GetCookieByName(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SessionDriver* session_driver = LookUpSessionDriverOrReturnInvalidResponse(
      base::Bind(&WebDriverModule::GetSessionDriver, base::Unretained(this)),
      path_variables, result_handler.get());
  if (session_driver) {
    WindowDriver* window_driver = session_driver->GetCurrentWindow();
    DCHECK(window_driver);
    if (window_driver) {
      std::string cookie_name = path_variables->GetVariable(kCookieName);

      typedef util::CommandResult<std::vector<protocol::Cookie> > CommandResult;
      CommandResult result = window_driver->GetCookie(cookie_name);
      util::internal::ReturnResponse(session_driver->session_id(), result,
                                     result_handler.get());
    }
  }
}

void WebDriverModule::IgnoreCommand(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  // TODO: Hook up and implement timeouts.
  return result_handler->SendResult(
      protocol::SessionId(kWebDriverSessionId),
      protocol::Response::kSuccess,
      make_scoped_ptr(base::Value::CreateNullValue()));
}

util::CommandResult<protocol::Capabilities>
WebDriverModule::CreateSessionInternal(
    const protocol::RequestedCapabilities& requested_capabilities) {
  typedef util::CommandResult<protocol::Capabilities> CommandResult;

  if (session_) {
    // A session has already been created. We can only create one.
    return CommandResult(protocol::Response::kSessionNotCreatedException,
                         kMaxSessionsCreatedMessage);
  }

  // We will only ever create sessions with one set of capabilities. So ignore
  // the desired capabilities (for now).
  if (requested_capabilities.required() &&
      !requested_capabilities.required()->AreCapabilitiesSupported()) {
    return CommandResult(protocol::Response::kSessionNotCreatedException,
                         kUnsupportedCapabilities);
  }

  session_ =
      create_session_driver_cb_.Run(protocol::SessionId(kWebDriverSessionId));
  if (!session_) {
    // Some failure to create the new session.
    return CommandResult(protocol::Response::kUnknownError,
                         kUnknownSessionCreationError);
  }

  // If proxy settings were requested when the session was created, set them
  // now.
  base::optional<protocol::Proxy> proxy_settings =
      requested_capabilities.desired().proxy();
  if (!proxy_settings && requested_capabilities.required()) {
    proxy_settings = requested_capabilities.required()->proxy();
  }
  if (proxy_settings) {
    set_proxy_function_.Run(proxy_settings->rules());
  }

  return session_->GetCapabilities();
}

util::CommandResult<std::string> WebDriverModule::RequestScreenshotInternal() {
  typedef util::CommandResult<std::string> CommandResult;

  // Request the screenshot and wait for the PNG data.
  ScreenshotResultContext context;
  get_screenshot_function_.Run(
      base::Bind(&OnPNGEncodeComplete, base::Unretained(&context)));
  context.complete_event.Wait();
  DCHECK(context.compressed_file);

  uint32 file_size_in_bytes =
      context.compressed_file->GetEstimatedSizeInBytes();
  if (file_size_in_bytes == 0 || !context.compressed_file->GetMemory()) {
    return CommandResult(protocol::Response::kUnknownError,
                         "Failed to take screenshot.");
  }

  // Encode the PNG data as a base64 encoded string.
  std::string encoded;
  {
    // base64 encode the contents of the file to be returned to the client.
    if (!base::Base64Encode(
            base::StringPiece(
                reinterpret_cast<char*>(context.compressed_file->GetMemory()),
                file_size_in_bytes),
            &encoded)) {
      return CommandResult(protocol::Response::kUnknownError,
                           "Failed to base64 encode screenshot file contents.");
    }
  }
  return CommandResult(encoded);
}

}  // namespace webdriver
}  // namespace cobalt
