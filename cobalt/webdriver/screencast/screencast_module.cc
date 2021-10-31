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

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"

#include "cobalt/webdriver/screencast/screencast_module.h"

namespace cobalt {
namespace webdriver {
namespace screencast {

namespace {
const char kJpegContentType[] = "image/jpeg";
// Add screencast frame rate as 30 fps.
const int kScreencastFramesPerSecond = 30;
}

ScreencastModule::ScreencastModule(
    int server_port, const std::string& listen_ip,
    const GetScreenshotFunction& screenshot_function)
    : screenshot_dispatcher_(new WebDriverDispatcher()),
      screenshot_thread_("ScreencstDrvThd"),
      incoming_requests_(),
      last_served_request_(-1),
      screenshot_function_(screenshot_function),
      no_screenshots_pending_(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED),
      num_screenshots_processing_(0) {
  DETACH_FROM_THREAD(thread_checker_);

  screenshot_dispatcher_->RegisterCommand(
      WebDriverServer::kGet, "/screenshot/:id",
      base::Bind(&ScreencastModule::PutRequestInQueue, base::Unretained(this)));
  // Start the thread and create the HTTP server on that thread.
  screenshot_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  screenshot_thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ScreencastModule::StartServer,
                            base::Unretained(this), server_port, listen_ip));
}

ScreencastModule::~ScreencastModule() {
  TRACE_EVENT0("cobalt::Screencast", "ScreencastModule::~ScreencastModule()");
  screenshot_thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ScreencastModule::StopTimer, base::Unretained(this)));

  no_screenshots_pending_.Wait();

  screenshot_thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ScreencastModule::StopServer, base::Unretained(this)));
  screenshot_thread_.Stop();
}

void ScreencastModule::StartServer(int server_port,
                                   const std::string& listen_ip) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Create a new WebDriverServer and pass in the Dispatcher.

  screenshot_server_.reset(new WebDriverServer(
      server_port, listen_ip,
      base::Bind(&WebDriverDispatcher::HandleWebDriverServerRequest,
                 base::Unretained(screenshot_dispatcher_.get())),
      "Cobalt.Server.Screencast"));

  screenshot_timer_.reset(new base::RepeatingTimer());

  const base::Closure screenshot_event =
      base::Bind(&ScreencastModule::TakeScreenshot, base::Unretained(this));
  screenshot_timer_->Start(FROM_HERE,
                           base::TimeDelta::FromMilliseconds(
                               1000.0f / kScreencastFramesPerSecond),
                           screenshot_event);
}

void ScreencastModule::StopTimer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  screenshot_timer_.reset();

  if (num_screenshots_processing_ < 1) {
    no_screenshots_pending_.Signal();
  }
}

void ScreencastModule::StopServer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Clear out queue of requests.
  while (!incoming_requests_.empty()) {
    scoped_refptr<WaitingRequest> next_request = incoming_requests_.front();
    incoming_requests_.pop();
    std::unique_ptr<base::Value> message = std::unique_ptr<base::Value>();
    // Send rejection to request with invalid ID.
    next_request->result_handler->SendResult(
        base::nullopt, protocol::Response::kUnknownError, std::move(message));
  }
  screenshot_server_.reset();
}

void ScreencastModule::PutRequestInQueue(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    std::unique_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  TRACE_EVENT0("cobalt::Screencast", "ScreencastModule::PutRequestInQueue()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  scoped_refptr<WaitingRequest> current_request = new WaitingRequest;
  current_request->result_handler = std::move(result_handler);
  // The id of request, e.g. screencast/2 would be 2.
  if (base::StringToInt(path_variables->GetVariable(":id"),
                        &(current_request->request_id))) {
    incoming_requests_.push(current_request);
  } else {
    // Send rejection to request with invalid ID.
    std::unique_ptr<base::Value> message = std::unique_ptr<base::Value>();
    result_handler->SendResult(base::nullopt, protocol::Response::kUnknownError,
                               std::move(message));
  }
}

void ScreencastModule::SendScreenshotToNextInQueue(
    const scoped_refptr<loader::image::EncodedStaticImage>& screenshot) {
  TRACE_EVENT0("cobalt::Screencast",
               "ScreencastModule::SendScreenshotToNextInQueue()");

  if (base::MessageLoop::current() != screenshot_thread_.message_loop()) {
    screenshot_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&ScreencastModule::SendScreenshotToNextInQueue,
                              base::Unretained(this), screenshot));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  num_screenshots_processing_--;
  // If the timer is off we can check if it's ready to be shutdown.
  if (screenshot_timer_.get() == nullptr && num_screenshots_processing_ < 1) {
    no_screenshots_pending_.Signal();
  }

  while (!incoming_requests_.empty()) {
    scoped_refptr<WaitingRequest> next_request = incoming_requests_.front();
    incoming_requests_.pop();
    std::unique_ptr<base::Value> message = std::unique_ptr<base::Value>();
    // Check if request is valid.
    if (next_request->request_id > last_served_request_) {
      // Send screenshot.
      last_served_request_ = next_request->request_id;
      next_request->result_handler->SendResultWithContentType(
          protocol::Response::kSuccess, kJpegContentType,
          reinterpret_cast<char*>(screenshot->GetMemory()),
          screenshot->GetEstimatedSizeInBytes());
      return;
    } else {
      // Send rejection to request with invalid ID.
      next_request->result_handler->SendResult(
          base::nullopt, protocol::Response::kUnknownError, std::move(message));
    }
  }
}

void ScreencastModule::TakeScreenshot() {
  TRACE_EVENT0("cobalt::Screencast", "ScreencastModule::TakeScreenshot()");
  if (num_screenshots_processing_ < max_num_screenshots_processing_) {
    num_screenshots_processing_++;
    screenshot_function_.Run(
        loader::image::EncodedStaticImage::ImageFormat::kJPEG,
        /*clip_rect=*/base::nullopt,
        base::Bind(&ScreencastModule::SendScreenshotToNextInQueue,
                   base::Unretained(this)));
  }
}

}  // namespace screencast
}  // namespace webdriver
}  // namespace cobalt
