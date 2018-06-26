// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_SCREENCAST_SCREENCAST_MODULE_H_
#define COBALT_WEBDRIVER_SCREENCAST_SCREENCAST_MODULE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "cobalt/loader/image/image_encoder.h"
#include "cobalt/webdriver/dispatcher.h"
#include "cobalt/webdriver/util/command_result.h"

namespace cobalt {
namespace webdriver {
namespace screencast {

// This class is responsible for taking screenshots at regular intervals for the
// Screencast Module. It's main components are a timer and a callback that takes
// screenshots The class keeps the last encoded screenshot as a base64 encoded
// string.
class RepeatingScreenshotTaker {
 public:
  typedef base::Callback<void(
      const scoped_refptr<loader::image::EncodedStaticImage>& image_data)>
      ScreenshotCompleteCallback;
  typedef base::Callback<void(loader::image::EncodedStaticImage::ImageFormat,
                              const ScreenshotCompleteCallback&)>
      GetScreenshotFunction;

  RepeatingScreenshotTaker(base::TimeDelta screenshot_interval,
                           const GetScreenshotFunction& screenshot_function);

  std::string GetCurrentScreenshot();

 private:
  // Takes a screenshot, encodes it as a JPEG, converts it to a base64 string,
  // and saves it as current_screenshot.
  void TakeScreenshot();

  // The latest available screenshot
  std::string current_screenshot_;

  scoped_ptr<base::Timer> timed_screenshots_;

  GetScreenshotFunction screenshot_function_;
};

// The Screencast Module waits for requests from the server. When the
// WebDriverDispatcher gets a request for a screenshot it responds with the
// latest taken screenshot from the RepeatingScreenshotTaker. The server
// recognises one command - /screenshot. That will return a base64 encoded JPEG
// image. The client should repeatedly ask for screenshots from this server to
// get the desired "screencast" affect.
class ScreencastModule {
 public:
  typedef base::Callback<void(
      const scoped_refptr<loader::image::EncodedStaticImage>& image_data)>
      ScreenshotCompleteCallback;
  typedef base::Callback<void(loader::image::EncodedStaticImage::ImageFormat,
                              const ScreenshotCompleteCallback&)>
      GetScreenshotFunction;

  ScreencastModule(int server_port, const std::string& listen_ip,
                   const GetScreenshotFunction& screenshot_function);

  ~ScreencastModule();

 private:
  void StartServer(int server_port, const std::string& listen_ip);

  void StopServer();

  void GetRecentScreenshot(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler);

  // All operations including HTTP server will occur on this thread.
  base::Thread screenshot_thread_;

  base::ThreadChecker thread_checker_;

  // Thus command dispatcher is responsible for pattern recognition of the URL
  // request, mapping requests to handler functions, handling invalid requests,
  // and sending response objects.
  scoped_ptr<WebDriverDispatcher> screenshot_dispatcher_;

  // The |screenshot_dispatcher_| sends responses through this HTTP server.
  scoped_ptr<WebDriverServer> screenshot_server_;

  // This constantly compresses screenshots
  RepeatingScreenshotTaker screenshot_taker_;
};

}  // namespace screencast
}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_SCREENCAST_SCREENCAST_MODULE_H_
