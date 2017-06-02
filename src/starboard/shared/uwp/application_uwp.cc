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

#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/uwp/window_internal.h"

using starboard::shared::starboard::Application;
using starboard::shared::starboard::CommandLine;
using starboard::shared::uwp::ApplicationUwp;
using Windows::ApplicationModel::Activation::IActivatedEventArgs;
using Windows::ApplicationModel::Core::CoreApplication;
using Windows::ApplicationModel::Core::CoreApplicationView;
using Windows::ApplicationModel::Core::IFrameworkView;
using Windows::ApplicationModel::Core::IFrameworkViewSource;
using Windows::Foundation::TypedEventHandler;
using Windows::UI::Core::CoreWindow;
using Windows::UI::Core::CoreProcessEventsOption;

ref class App sealed : public IFrameworkView {
 public:
  App() {}

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
    CoreWindow::GetForCurrentThread()->Activate();
    ApplicationUwp::Get()->DispatchStart();
  }
};

ref class Direct3DApplicationSource sealed : IFrameworkViewSource {
 public:
  Direct3DApplicationSource() {
    SB_LOG(INFO) << "Direct3DApplicationSource";
  }
  virtual IFrameworkView^ CreateView() {
    return ref new App();
  }
};

namespace starboard {
namespace shared {
namespace uwp {

ApplicationUwp::ApplicationUwp() : window_(kSbWindowInvalid) {}

ApplicationUwp::~ApplicationUwp() {}

void ApplicationUwp::Initialize() {}

void ApplicationUwp::Teardown() {}

Application::Event* ApplicationUwp::GetNextEvent() {
  SB_NOTREACHED();
  return nullptr;
}

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
  // TODO: Implement with CoreWindow->GetForCurrentThread->Dispatcher->RunAsync
  SB_NOTIMPLEMENTED();
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
