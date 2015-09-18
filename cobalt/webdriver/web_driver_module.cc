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

#include "cobalt/webdriver/web_driver_module.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "cobalt/webdriver/dispatcher.h"
#include "cobalt/webdriver/protocol/capabilities.h"
#include "cobalt/webdriver/protocol/window_id.h"
#include "cobalt/webdriver/server.h"
#include "cobalt/webdriver/session_driver.h"
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

// Error messages related to session creation.
const char kMaxSessionsCreatedMessage[] =
    "Maximum number of sessions have been created.";
const char kUnsupportedCapabilities[] =
    "An unsupported capability was requested.";
const char kUnknownSessionCreationError[] =
    "An unknown error occurred trying to create a new session.";

}  // namespace

WebDriverModule::WebDriverModule(
    int server_port, const CreateSessionDriverCB& create_session_driver_cb)
    : create_session_driver_cb_(create_session_driver_cb),
      webdriver_dispatcher_(new WebDriverDispatcher()) {
  // Create a new dispatcher and register some commands.
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
      WebDriverServer::kGet, StringPrintf("/session/%s", kSessionIdVariable),
      base::Bind(&WebDriverModule::GetSessionCapabilities,
                 base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kDelete, StringPrintf("/session/%s", kSessionIdVariable),
      base::Bind(&WebDriverModule::DeleteSession, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/window_handle", kSessionIdVariable),
      base::Bind(&WebDriverModule::GetCurrentWindow, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/window_handles", kSessionIdVariable),
      base::Bind(&WebDriverModule::GetWindowHandles, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet,
      StringPrintf("/session/%s/window/%s/size", kSessionIdVariable,
                   kWindowHandleVariable),
      base::Bind(&WebDriverModule::GetWindowSize, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kPost,
      StringPrintf("/session/%s/element", kSessionIdVariable),
      base::Bind(&WebDriverModule::FindElement, base::Unretained(this)));
  webdriver_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, StringPrintf("/session/%s/element/%s/name",
                                          kSessionIdVariable, kElementId),
      base::Bind(&WebDriverModule::GetElementName, base::Unretained(this)));

  // The WebDriver API implementation will be called on the HTTP server thread.
  thread_checker_.DetachFromThread();

  // Create a new WebDriverServer and pass in the Dispatcher.
  webdriver_server_.reset(new WebDriverServer(
      server_port,
      base::Bind(&WebDriverDispatcher::HandleWebDriverServerRequest,
                 base::Unretained(webdriver_dispatcher_.get()))));
}

WebDriverModule::~WebDriverModule() {}

SessionDriver* WebDriverModule::GetSession(
    const protocol::SessionId& session_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (session_ && (session_->session_id() == session_id)) {
    return session_.get();
  }
  return NULL;
}

WindowDriver* WebDriverModule::GetWindow(SessionDriver* session,
                                         const protocol::WindowId& window_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(session);
  // A WebDriver implementation should return a NoSuchWindow error if the
  // currently selected window has been closed, but we do not support closing
  // windows in Cobalt.
  return session->GetWindow(window_id);
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
  // There will only ever be one or zero sessions.
  scoped_ptr<base::ListValue> sessions(new base::ListValue());
  if (session_) {
    sessions->AppendString(kWebDriverSessionId);
  }
  result_handler->SendResult(base::nullopt, protocol::Response::kSuccess,
                             sessions.PassAs<base::Value>());
}

// https://code.google.com/p/selenium/wiki/JsonWireProtocol#GET_/session/:sessionId
void WebDriverModule::CreateSession(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<protocol::RequestedCapabilities> requested_capabilities =
      protocol::RequestedCapabilities::FromValue(parameters);
  if (!requested_capabilities) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidParameters, "");
    return;
  }

  // We will only ever create sessions with one set of capabilities. So ignore
  // the desired capabilities (for now).
  if (requested_capabilities->required() &&
      !requested_capabilities->required()->AreCapabilitiesSupported()) {
    result_handler->SendResult(
        base::nullopt, protocol::Response::kSessionNotCreatedException,
        protocol::Response::CreateErrorResponse(kUnsupportedCapabilities));
    return;
  }

  if (session_) {
    // A session has already been created. We can only create one.
    result_handler->SendResult(
        base::nullopt, protocol::Response::kSessionNotCreatedException,
        protocol::Response::CreateErrorResponse(kMaxSessionsCreatedMessage));
    return;
  }


  session_ =
      create_session_driver_cb_.Run(protocol::SessionId(kWebDriverSessionId));
  if (!session_) {
    // Some failure to create the new session.
    result_handler->SendResult(
        base::nullopt, protocol::Response::kUnknownError,
        protocol::Response::CreateErrorResponse(kUnknownSessionCreationError));
    return;
  }

  // New session successfully created.
  result_handler->SendResult(
      session_->session_id(), protocol::Response::kSuccess,
      protocol::Capabilities::ToValue(*session_->capabilities()));
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

void WebDriverModule::GetSessionCapabilities(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  protocol::SessionId session_id(
      path_variables->GetVariable(kSessionIdVariable));

  SessionDriver* session = GetSession(session_id);
  // If there is no session with this ID, then return an error.
  if (!session) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        session_id.id());
    return;
  }

  result_handler->SendResult(
      session->session_id(), protocol::Response::kSuccess,
      protocol::Capabilities::ToValue(*session->capabilities()));
}

void WebDriverModule::GetCurrentWindow(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(path_variables);
  protocol::SessionId session_id(
      path_variables->GetVariable(kSessionIdVariable));

  SessionDriver* session = GetSession(session_id);
  // If there is no session with this ID, then return an error.
  if (!session) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        session_id.id());
    return;
  }

  WindowDriver* window_driver = session->GetCurrentWindow();
  // A WebDriver implementation should return a NoSuchWindow error if the
  // currently selected window has been closed, but we do not support closing
  // windows in Cobalt.
  DCHECK(window_driver);

  result_handler->SendResult(
      session->session_id(), protocol::Response::kSuccess,
      protocol::WindowId::ToValue(window_driver->window_id()));
}

void WebDriverModule::GetWindowHandles(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(path_variables);
  protocol::SessionId session_id(
      path_variables->GetVariable(kSessionIdVariable));

  SessionDriver* session = GetSession(session_id);
  // If there is no session with this ID, then return an error.
  if (!session) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        session_id.id());
    return;
  }

  // Only one window is supported in Cobalt.
  WindowDriver* window_driver = session->GetCurrentWindow();
  DCHECK(window_driver);

  scoped_ptr<base::ListValue> all_windows(new base::ListValue());
  all_windows->Append(
      protocol::WindowId::ToValue(window_driver->window_id()).release());

  result_handler->SendResult(session->session_id(),
                             protocol::Response::kSuccess,
                             all_windows.PassAs<base::Value>());
}

void WebDriverModule::GetWindowSize(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(path_variables);
  protocol::SessionId session_id(
      path_variables->GetVariable(kSessionIdVariable));

  SessionDriver* session = GetSession(session_id);
  // If there is no session with this ID, then return an error.
  if (!session) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        session_id.id());
    return;
  }

  protocol::WindowId window_id(
      path_variables->GetVariable(kWindowHandleVariable));
  WindowDriver* window_driver = GetWindow(session, window_id);
  // If there is no session with this ID, then return an error.
  if (!window_driver) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        window_id.id());
    return;
  }

  protocol::Size window_size = window_driver->GetWindowSize();

  result_handler->SendResult(session->session_id(),
                             protocol::Response::kSuccess,
                             protocol::Size::ToValue(window_size));
}

void WebDriverModule::FindElement(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(path_variables);
  protocol::SessionId session_id(
      path_variables->GetVariable(kSessionIdVariable));

  SessionDriver* session = GetSession(session_id);
  // If there is no session with this ID, then return an error.
  if (!session) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        session_id.id());
    return;
  }

  WindowDriver* window_driver = session->GetCurrentWindow();

  scoped_ptr<protocol::SearchStrategy> search_strategy =
      protocol::SearchStrategy::FromValue(parameters);
  if (!search_strategy) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidParameters, "");
    return;
  }

  ElementDriver* element_driver =
      window_driver->FindElement(*search_strategy.get());
  if (element_driver) {
    result_handler->SendResult(
        session->session_id(), protocol::Response::kSuccess,
        protocol::ElementId::ToValue(element_driver->element_id()));
  } else {
    result_handler->SendResult(session->session_id(),
                               protocol::Response::kNoSuchElement,
                               protocol::Response::CreateErrorResponse(""));
  }
}

void WebDriverModule::GetElementName(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  DCHECK(path_variables);
  protocol::SessionId session_id(
      path_variables->GetVariable(kSessionIdVariable));

  SessionDriver* session = GetSession(session_id);
  // If there is no session with this ID, then return an error.
  if (!session) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        session_id.id());
    return;
  }

  WindowDriver* window_driver = session->GetCurrentWindow();

  protocol::ElementId element_id(path_variables->GetVariable(kElementId));
  ElementDriver* element_driver = window_driver->GetElementDriver(element_id);
  if (!element_driver) {
    result_handler->SendInvalidRequestResponse(
        WebDriverDispatcher::CommandResultHandler::kInvalidPathVariable,
        session_id.id());
    return;
  }

  scoped_ptr<base::StringValue> name_value(
      new base::StringValue(element_driver->GetTagName()));
  result_handler->SendResult(session->session_id(),
                             protocol::Response::kSuccess,
                             name_value.PassAs<base::Value>());
}

}  // namespace webdriver
}  // namespace cobalt
