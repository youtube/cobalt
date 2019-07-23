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

#include <memory>
#include <queue>
#include <string>

#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "cobalt/loader/image/image_encoder.h"
#include "cobalt/math/rect.h"
#include "cobalt/webdriver/dispatcher.h"
#include "cobalt/webdriver/screenshot.h"
#include "cobalt/webdriver/util/command_result.h"

namespace cobalt {
namespace webdriver {
namespace screencast {

struct WaitingRequest : public base::RefCounted<WaitingRequest> {
  std::unique_ptr<webdriver::WebDriverDispatcher::CommandResultHandler>
      result_handler;
  int request_id;
};

// The Screencast Module waits for requests from the server. When the
// WebDriverDispatcher gets a request for a screenshot it puts the request in a
// queue.  A timer takes screenshots at the current frame rate. When a
// screenshot is finished, it will be sent to the first valid request on the
// queue. The server recognises one command - /screenshot:id. That will return a
// JPEG image. The client should repeatedly ask for screenshots from this server
// to get the desired "screencast" affect, and it can layer these requests for
// the optimal framerate.
class ScreencastModule {
 public:
  typedef Screenshot::GetScreenshotFunction GetScreenshotFunction;

  ScreencastModule(int server_port, const std::string& listen_ip,
                   const GetScreenshotFunction& screenshot_function);

  ~ScreencastModule();

 private:
  void TakeScreenshot();
  void StartServer(int server_port, const std::string& listen_ip);

  void StopTimer();
  void StopServer();

  void PutRequestInQueue(
      const base::Value* parameters,
      const WebDriverDispatcher::PathVariableMap* path_variables,
      std::unique_ptr<WebDriverDispatcher::CommandResultHandler>
          result_handler);

  // This method will search the queue for the first valid request. A valid
  // request must have an id higher than the last valid request. When an invalid
  // request is popped off the queue, the screencast server will respond to the
  // request with an error. At the first valid request, the server will respond
  // with an image and return.
  void SendScreenshotToNextInQueue(
      const scoped_refptr<loader::image::EncodedStaticImage>& screenshot);

  // All operations including HTTP server will occur on this thread.
  base::Thread screenshot_thread_;

  THREAD_CHECKER(thread_checker_);

  // Thus command dispatcher is responsible for pattern recognition of the URL
  // request, mapping requests to handler functions, handling invalid requests,
  // and sending response objects.
  std::unique_ptr<WebDriverDispatcher> screenshot_dispatcher_;

  // The |screenshot_dispatcher_| sends responses through this HTTP server.
  std::unique_ptr<WebDriverServer> screenshot_server_;

  // The id of the last valid request the screencast server responded to.
  int last_served_request_;

  // A queue of all the requests, valid or invalid, that are waiting on a
  // response from the screencast server.
  std::queue<scoped_refptr<WaitingRequest>> incoming_requests_;

  // Takes a screenshot of the most current frame.
  GetScreenshotFunction screenshot_function_;

  // This timer is responsible for taking screenshots at regular intervals.
  std::unique_ptr<base::RepeatingTimer> screenshot_timer_;

  // This event is responsible for a safe shutdown. The event will be signalled
  // only after the timer has been shutdown and will not request any more
  // screenshots, and the |num_screenshots_processing_| is zero. Without this
  // check, the processing screenshots would try to call
  // SendScreenshotToNextInQueue after the object is destroyed, causing an
  // error.
  base::WaitableEvent no_screenshots_pending_;

  // The number of screenshots that |screenshot_timer_| has triggered that are
  // not complete.
  int num_screenshots_processing_;
  // The maximum number of requests that will process simultaneously.
  int max_num_screenshots_processing_ = 2;
};

}  // namespace screencast
}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_SCREENCAST_SCREENCAST_MODULE_H_
