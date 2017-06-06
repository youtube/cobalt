// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/uwp/application_uwp.h"

#include <windows.h>

#include <string>
#include <vector>

#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/uwp/window_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

using starboard::shared::starboard::Application;
using starboard::shared::starboard::CommandLine;
using starboard::shared::uwp::ApplicationUwp;
using starboard::shared::uwp::GetArgvZero;
using starboard::shared::win32::wchar_tToUTF8;
using Windows::ApplicationModel::Activation::ActivationKind;
using Windows::ApplicationModel::Activation::IActivatedEventArgs;
using Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs;
using Windows::ApplicationModel::Core::CoreApplication;
using Windows::ApplicationModel::Core::CoreApplicationView;
using Windows::ApplicationModel::Core::IFrameworkView;
using Windows::ApplicationModel::Core::IFrameworkViewSource;
using Windows::Foundation::TypedEventHandler;
using Windows::Foundation::Uri;
using Windows::UI::Core::CoreWindow;
using Windows::UI::Core::CoreProcessEventsOption;

namespace {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

// Parses a starboard: URI scheme by splitting args at ';' boundries.
std::vector<std::string> ParseStarboardUri(const std::string& uri) {
  std::vector<std::string> result;
  result.push_back(GetArgvZero());

  size_t index = uri.find(':');
  if (index == std::string::npos) {
    return result;
  }

  std::string args = uri.substr(index + 1);

  while (!args.empty()) {
    size_t next = args.find(';');
    result.push_back(args.substr(0, next));
    if (next == std::string::npos) {
      return result;
    }
    args = args.substr(next + 1);
  }
  return result;
}

#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

}  // namespace

ref class App sealed : public IFrameworkView {
 public:
  App() : previously_activated_(false) {}

  // IFrameworkView methods.
  virtual void Initialize(
      Windows::ApplicationModel::Core::CoreApplicationView^ applicationView) {
    applicationView->Activated +=
        ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(
            this, &App::OnActivated);
  }
  virtual void SetWindow(CoreWindow^ window) {}
  virtual void Load(Platform::String^ entryPoint) {}
  virtual void Run() {
    CoreWindow ^window = CoreWindow::GetForCurrentThread();
    window->Activate();
    window->Dispatcher->ProcessEvents(
        CoreProcessEventsOption::ProcessUntilQuit);
  }
  virtual void Uninitialize() {
  }

  void OnActivated(
      CoreApplicationView^ applicationView, IActivatedEventArgs^ args) {
    // Please see application lifecyle description:
    // https://docs.microsoft.com/en-us/windows/uwp/launch-resume/app-lifecycle
    // Note that this document was written for Xaml apps not core apps,
    // so for us the precise API is a little different.
    // The substance is that, while OnActiviated is definitely called the
    // first time the application is started, it may additionally called
    // in other cases while the process is already running. Starboard
    // applications cannot fully restart in a process lifecycle,
    // so we interpret the first activation and the subsequent ones differently.
    if (args->Kind == ActivationKind::Protocol) {
      Uri^ uri = dynamic_cast<IProtocolActivatedEventArgs^>(args)->Uri;

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      // The starboard: scheme provides commandline arguments, but that's
      // only allowed during a process's first activation.
      if (!previously_activated_ && uri->SchemeName->Equals("starboard")) {
        std::string uri(wchar_tToUTF8(uri->RawUri->Data()));
        // args_ is a vector of std::string, but argv_ is a vector of
        // char* into args_ so as to compose a char**.
        args_ = ParseStarboardUri(uri);
        for (const std::string& arg : args_) {
          argv_.push_back(arg.c_str());
        }

        ApplicationUwp::Get()->SetCommandLine(
          static_cast<int>(argv_.size()), argv_.data());
      }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
    }
    previously_activated_ = true;
    previous_activation_kind_ = args->Kind;
    CoreWindow::GetForCurrentThread()->Activate();
    ApplicationUwp::Get()->DispatchStart();
  }
 private:
  bool previously_activated_;
  // Only valid if previously_activated_ is true
  ActivationKind previous_activation_kind_;
  std::vector<std::string> args_;
  std::vector<const char *> argv_;
};

ref class Direct3DApplicationSource sealed : IFrameworkViewSource {
 public:
  Direct3DApplicationSource() {}
  virtual IFrameworkView^ CreateView() {
    return ref new App();
  }
};

namespace starboard {
namespace shared {
namespace uwp {

// If an argv[0] is required, fill it in with the result of
// GetModuleFileName()
std::string GetArgvZero() {
  const size_t kMaxModuleNameSize = 256;
  wchar_t buffer[kMaxModuleNameSize];
  DWORD result = GetModuleFileName(NULL, buffer, kMaxModuleNameSize);
  std::string arg;
  if (result == 0) {
    arg = "unknown";
  } else {
    arg = wchar_tToUTF8(buffer, result).c_str();
  }
  return arg;
}

ApplicationUwp::ApplicationUwp() : window_(kSbWindowInvalid) {}

ApplicationUwp::~ApplicationUwp() {}

void ApplicationUwp::Initialize() {}

void ApplicationUwp::Teardown() {}

Application::Event* ApplicationUwp::GetNextEvent() {
  SB_NOTREACHED();
  return nullptr;
}

// CreateWindow is a macro in windows.h
#undef CreateWindow
SbWindow ApplicationUwp::CreateWindow(const SbWindowOptions* options) {
  // TODO: Determine why SB_DCHECK(IsCurrentThread()) fails in nplb, fix it,
  // and add back this check.

  if (SbWindowIsValid(window_)) {
    return kSbWindowInvalid;
  }

  window_ = new SbWindowPrivate(options);
  return window_;
}

bool ApplicationUwp::DestroyWindow(SbWindow window) {
  // TODO: Determine why SB_DCHECK(IsCurrentThread()) fails in nplb, fix it,
  // and add back this check.

  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid context.";
    return false;
  }

  SB_DCHECK(window_ == window);
  delete window;
  window_ = kSbWindowInvalid;

  return true;
}

bool ApplicationUwp::DispatchNextEvent() {
  auto direct3DApplicationSource = ref new Direct3DApplicationSource();
  CoreApplication::Run(direct3DApplicationSource);
  return false;
}

void ApplicationUwp::Inject(Application::Event* event) {
  CoreWindow::GetForCurrentThread()->Dispatcher->RunAsync(
    Windows::UI::Core::CoreDispatcherPriority::Normal,
    ref new Windows::UI::Core::DispatchedHandler([this, event]() {
      bool result = DispatchAndDelete(event);
      if (!result) {
        CoreApplication::Exit();
      }
    }));
}

void ApplicationUwp::InjectTimedEvent(Application::TimedEvent* timed_event) {
  SB_NOTIMPLEMENTED();
}

void ApplicationUwp::CancelTimedEvent(SbEventId event_id) {
  SB_NOTIMPLEMENTED();
}

Application::TimedEvent* ApplicationUwp::GetNextDueTimedEvent() {
  SB_NOTIMPLEMENTED();
  return nullptr;
}

SbTimeMonotonic ApplicationUwp::GetNextTimedEventTargetTime() {
  SB_NOTIMPLEMENTED();
  return 0;
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
