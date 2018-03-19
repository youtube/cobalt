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

#ifndef COBALT_WEBDRIVER_WEB_DRIVER_MODULE_H_
#define COBALT_WEBDRIVER_WEB_DRIVER_MODULE_H_

#include <string>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/window.h"
#include "cobalt/webdriver/dispatcher.h"
#include "cobalt/webdriver/protocol/button.h"
#include "cobalt/webdriver/protocol/capabilities.h"
#include "cobalt/webdriver/protocol/element_id.h"
#include "cobalt/webdriver/protocol/moveto.h"
#include "cobalt/webdriver/protocol/server_status.h"
#include "cobalt/webdriver/protocol/session_id.h"
#include "cobalt/webdriver/protocol/window_id.h"
#include "cobalt/webdriver/util/command_result.h"

namespace cobalt {
namespace webdriver {

class ElementDriver;
class SessionDriver;
class WebDriverServer;
class WindowDriver;

class WebDriverModule {
 public:
  typedef base::Callback<scoped_ptr<SessionDriver>(const protocol::SessionId&)>
      CreateSessionDriverCB;
  typedef base::Callback<void(
      const scoped_refptr<loader::image::EncodedStaticImage>& image_data)>
      ScreenshotCompleteCallback;
  typedef base::Callback<void(const ScreenshotCompleteCallback&)>
      GetScreenshotFunction;
  typedef base::Callback<void(const std::string&)> SetProxyFunction;
  // Use this as the default listen_ip. It means "any interface on the local
  // machine" eg INADDR_ANY.
  static const char kDefaultListenIp[];
  WebDriverModule(int server_port, const std::string& listen_ip,
                  const CreateSessionDriverCB& create_session_driver_cb,
                  const GetScreenshotFunction& get_screenshot_function,
                  const SetProxyFunction& set_proxy_function,
                  const base::Closure& shutdown_cb);
  ~WebDriverModule();

  // This will be called when the WebModule, which owns the Window object in the
  // WindowDriver, is destroyed and recreated in response to navigation. This
  // unfortunately does leave a window where a WebDriver command may be run
  // before RefreshWindowDriver has been called. In this case, the weak Window
  // reference will be invalidated and the WebDriver command will fail. This is
  // a side-effect of Cobalt destroying and re-creating a Window on navigation,
  // rather than the navigation occurring in place.
  void OnWindowRecreated();

 private:
  void StartServer(int server_port, const std::string& listen_ip);
  void StopServer();
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
  void RequestScreenshot(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void Shutdown(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);

  void ElementEquals(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetAttribute(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetCssProperty(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);
  void GetCookieByName(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);

  void IgnoreCommand(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);

  SessionDriver* GetSessionDriver(const protocol::SessionId& session_id);

  util::CommandResult<protocol::Capabilities> CreateSessionInternal(
      const protocol::RequestedCapabilities& capabilities);

  util::CommandResult<std::string> RequestScreenshotInternal();

  base::ThreadChecker thread_checker_;

  // All WebDriver operations including HTTP server will occur on this thread.
  base::Thread webdriver_thread_;

  // Create a new WebDriver session through this callback.
  CreateSessionDriverCB create_session_driver_cb_;

  // Request a screenshot to be created and written to a file.
  GetScreenshotFunction get_screenshot_function_;

  // Configure custom proxy settings.
  SetProxyFunction set_proxy_function_;

  // Callback to shut down the application.
  base::Closure shutdown_cb_;

  // The WebDriver command dispatcher
  scoped_ptr<WebDriverDispatcher> webdriver_dispatcher_;

  // The WebDriver server
  scoped_ptr<WebDriverServer> webdriver_server_;

  // The current (and only) WebDriver session, if it has been created.
  scoped_ptr<SessionDriver> session_;

  // The WebDriver server status.
  const protocol::ServerStatus status_;

  base::Callback<SessionDriver*(const protocol::SessionId&)>
      get_session_driver_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_WEB_DRIVER_MODULE_H_
