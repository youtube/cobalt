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

#ifndef WEBDRIVER_WEB_DRIVER_MODULE_H_
#define WEBDRIVER_WEB_DRIVER_MODULE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/webdriver/dispatcher.h"
#include "cobalt/webdriver/protocol/server_status.h"
#include "cobalt/webdriver/protocol/session_id.h"
#include "cobalt/webdriver/protocol/window_id.h"

namespace cobalt {
namespace webdriver {

class SessionDriver;
class WebDriverServer;
class WindowDriver;

class WebDriverModule {
 public:
  typedef base::Callback<scoped_ptr<SessionDriver>(const protocol::SessionId&)>
      CreateSessionDriverCB;
  WebDriverModule(int server_port,
                  const CreateSessionDriverCB& create_session_driver_cb);
  ~WebDriverModule();

 private:
  SessionDriver* GetSession(const protocol::SessionId& session_id);
  WindowDriver* GetWindow(SessionDriver* session,
                          const protocol::WindowId& window_id);

  void GetServerStatus(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetActiveSessions(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void CreateSession(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void DeleteSession(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetSessionCapabilities(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetCurrentWindow(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetWindowHandles(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetWindowSize(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void FindElement(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetElementName(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);

  base::ThreadChecker thread_checker_;

  // Create a new WebDriver session through this callback.
  CreateSessionDriverCB create_session_driver_cb_;

  // The WebDriver command dispatcher
  scoped_ptr<WebDriverDispatcher> webdriver_dispatcher_;

  // The WebDriver server
  scoped_ptr<WebDriverServer> webdriver_server_;

  // The current (and only) WebDriver session, if it has been created.
  scoped_ptr<SessionDriver> session_;

  // The WebDriver server status.
  const protocol::ServerStatus status_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // WEBDRIVER_WEB_DRIVER_MODULE_H_
