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

#include <string>

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "base/string_piece.h"
#include "base/synchronization/waitable_event.h"

#include "cobalt/webdriver/screencast/screencast_module.h"

namespace cobalt {
namespace webdriver {
namespace screencast {

namespace {

// Helper struct for getting a JPEG screenshot synchronously.
struct ScreenshotResultContext {
  ScreenshotResultContext() : complete_event(true, false) {}
  scoped_refptr<loader::image::EncodedStaticImage> compressed_file;
  base::WaitableEvent complete_event;
};

// Callback function to be called when JPEG encoding is complete.
void OnJPEGEncodeComplete(
    ScreenshotResultContext* context,
    const scoped_refptr<loader::image::EncodedStaticImage>&
        compressed_image_data) {
  DCHECK(context);
  DCHECK(compressed_image_data->GetImageFormat() ==
         loader::image::EncodedStaticImage::ImageFormat::kJPEG);
  context->compressed_file = compressed_image_data;
  context->complete_event.Signal();
}

}  // namespace

RepeatingScreenshotTaker::RepeatingScreenshotTaker(
    base::TimeDelta screenshot_interval,
    const GetScreenshotFunction& screenshot_function)
    : screenshot_function_(screenshot_function) {
  const bool retain_user_task = true;
  const bool is_repeating = true;
  timed_screenshots_.reset(new base::Timer(retain_user_task, is_repeating));

  const base::Closure screenshot_event = base::Bind(
      &RepeatingScreenshotTaker::TakeScreenshot, base::Unretained(this));
  timed_screenshots_->Start(FROM_HERE, screenshot_interval, screenshot_event);
}

std::string RepeatingScreenshotTaker::GetCurrentScreenshot() {
  return current_screenshot_;
}

void RepeatingScreenshotTaker::TakeScreenshot() {
  TRACE_EVENT0("cobalt::Screencast", "ScreenshotTaker::TakeScreenshot()");
  ScreenshotResultContext context;
  screenshot_function_.Run(
      loader::image::EncodedStaticImage::ImageFormat::kJPEG,
      base::Bind(&OnJPEGEncodeComplete, base::Unretained(&context)));

  context.complete_event.Wait();
  DCHECK(context.compressed_file);

  uint32 file_size_in_bytes =
      context.compressed_file->GetEstimatedSizeInBytes();
  if (file_size_in_bytes == 0 || !context.compressed_file->GetMemory()) {
    return;
  }

  // Encode the JPEG data as a base64 encoded string.
  std::string encoded;
  {
    // base64 encode the contents of the file to be returned to the client.
    if (!base::Base64Encode(
            base::StringPiece(
                reinterpret_cast<char*>(context.compressed_file->GetMemory()),
                file_size_in_bytes),
            &encoded)) {
      return;
    }
  }

  current_screenshot_ = encoded;
}

ScreencastModule::ScreencastModule(
    int server_port, const std::string& listen_ip,
    const GetScreenshotFunction& screenshot_function)
    : screenshot_dispatcher_(new webdriver::WebDriverDispatcher()),
      screenshot_thread_("Screencast Driver thread"),
      screenshot_taker_(base::TimeDelta::FromMillisecondsD(
                            COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS),
                        screenshot_function) {
  screenshot_dispatcher_->RegisterCommand(
      webdriver::WebDriverServer::kGet, "/screenshot",
      base::Bind(&ScreencastModule::GetRecentScreenshot,
                 base::Unretained(this)));

  // Start the thread and create the HTTP server on that thread.
  screenshot_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));
  screenshot_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&ScreencastModule::StartServer,
                            base::Unretained(this), server_port, listen_ip));
}

ScreencastModule::~ScreencastModule() {
  screenshot_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ScreencastModule::StopServer, base::Unretained(this)));
  screenshot_thread_.Stop();
}

void ScreencastModule::StartServer(int server_port,
                                   const std::string& listen_ip) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Create a new WebDriverServer and pass in the Dispatcher.
  screenshot_server_.reset(new WebDriverServer(
      server_port, listen_ip,
      base::Bind(&WebDriverDispatcher::HandleWebDriverServerRequest,
                 base::Unretained(screenshot_dispatcher_.get())),
      "Cobalt.Server.Screencast"));
}

void ScreencastModule::StopServer() { screenshot_server_.reset(); }

void ScreencastModule::GetRecentScreenshot(
    const base::Value* parameters,
    const WebDriverDispatcher::PathVariableMap* path_variables,
    scoped_ptr<WebDriverDispatcher::CommandResultHandler> result_handler) {
  TRACE_EVENT0("cobalt::Screencast", "ScreencastModule::GetRecentScreenshot()");
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<base::Value> message = scoped_ptr<base::Value>(
      new base::StringValue(screenshot_taker_.GetCurrentScreenshot()));
  result_handler->SendResult(base::nullopt, protocol::Response::kSuccess,
                             message.Pass());
}

}  // namespace screencast
}  // namespace webdriver
}  // namespace cobalt
