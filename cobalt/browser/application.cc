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

#include "cobalt/browser/application.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stringprintf.h"
#include "cobalt/browser/switches.h"

#if defined(__LB_LINUX__)
#define VENDOR "NoVendor"
#define PLATFORM "Linux"
#elif defined(__LB_PS3__)
#define VENDOR "Sony"
#define PLATFORM "PS3"
#elif defined(COBALT_WIN)
#define VENDOR "Microsoft"
#define PLATFORM "Windows"
#else
#error Undefined platform
#endif

#if defined(COBALT_BUILD_TYPE_DEBUG)
#define COBALT_BUILD_TYPE "Debug"
#elif defined(COBALT_BUILD_TYPE_DEVEL)
#define COBALT_BUILD_TYPE "Devel"
#elif defined(COBALT_BUILD_TYPE_QA)
#define COBALT_BUILD_TYPE "QA"
#elif defined(COBALT_BUILD_TYPE_GOLD)
#define COBALT_BUILD_TYPE "Gold"
#else
#error Unknown build type
#endif

// TODO(***REMOVED***): The following values are hardcoded for now.
#define CHROME_VERSION "25.0.1364.70"
#define COBALT_VERSION "0.01"
#define LANGUAGE_CODE "en"
#define COUNTRY_CODE "US"

namespace cobalt {
namespace browser {

namespace {

std::string GetInitialURL() {
  // Allow the user to override the default initial URL via a command line
  // parameter.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInitialURL)) {
    return command_line->GetSwitchValueASCII(switches::kInitialURL);
  }

  static const char* kDefaultInitialURL = "https://www.youtube.com/tv";
  return std::string(kDefaultInitialURL);
}

std::string GetUserAgent() {
  std::string user_agent;

  std::string product =
      base::StringPrintf("Chrome/%s Cobalt/%s %s build", CHROME_VERSION,
                         COBALT_VERSION, COBALT_BUILD_TYPE);
  user_agent.append(base::StringPrintf(
      "Mozilla/5.0 (%s) (KHTML, like Gecko) %s", PLATFORM, product.c_str()));

  std::string vendor = VENDOR;
  std::string device = PLATFORM;
  std::string firmware_version = "";  // Okay to leave blank
  std::string model = PLATFORM;
  std::string sku = "";  // Okay to leave blank
  std::string language_code = LANGUAGE_CODE;
  std::string country_code = COUNTRY_CODE;
  user_agent.append(base::StringPrintf(
      " %s %s/%s (%s, %s, %s, %s)", vendor.c_str(), device.c_str(),
      firmware_version.c_str(), model.c_str(), sku.c_str(),
      language_code.c_str(), country_code.c_str()));

  return user_agent;
}

}  // namespace

Application::Application()
    : ui_message_loop_(MessageLoop::TYPE_UI) {
  std::string url = GetInitialURL();
  std::string user_agent = GetUserAgent();
  DLOG(INFO) << "Initial URL: " << url;
  DLOG(INFO) << "User Agent: " << user_agent;
  // Create the main components of our browser.
  BrowserModule::Options options;
  options.url = GURL(url);
  options.fake_resource_loader_factory_options.create_fake_io_thread = true;
  browser_module_.reset(new BrowserModule(user_agent, options));
}

Application::~Application() { DCHECK(!ui_message_loop_.is_running()); }

void Application::Quit() {
  if (!quit_closure_.is_null()) {
    quit_closure_.Run();
  }
}

void Application::Run() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace browser
}  // namespace cobalt
