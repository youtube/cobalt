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

#include "starboard/shared/win32/application_win32.h"

#include <windows.h>  // NOLINT(build/include_order)
#include <windowsx.h>  // NOLINT(build/include_order)

#include <cstdio>
#include <string>

#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/win32/dialog.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/minidump.h"
#include "starboard/shared/win32/thread_private.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/shared/win32/window_internal.h"
#include "starboard/system.h"

using starboard::shared::starboard::Application;
using starboard::shared::starboard::CommandLine;
using starboard::shared::win32::ApplicationWin32;
using starboard::shared::win32::CStringToWString;
using starboard::shared::win32::DebugLogWinError;

namespace {

static const int kSbMouseDeviceId = 1;

static const TCHAR kWindowClassName[] = L"window_class_name";
const char kMiniDumpFilePath[] = "mini_dump_file_path";

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  return ApplicationWin32::Get()->WindowProcess(hWnd, msg, w_param, l_param);
}

bool RegisterWindowClass() {
  WNDCLASSEX window_class;
  window_class.cbSize = sizeof(WNDCLASSEX);
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ff729176(v=vs.85).aspx
  window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  window_class.lpfnWndProc = WndProc;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = GetModuleHandle(nullptr);
  // TODO: Add YouTube icon.
  window_class.hIcon = LoadIcon(window_class.hInstance, IDI_APPLICATION);
  window_class.hIconSm = LoadIcon(window_class.hInstance, IDI_APPLICATION);
  window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  window_class.lpszMenuName = NULL;
  window_class.lpszClassName = kWindowClassName;

  if (!::RegisterClassEx(&window_class)) {
    SB_LOG(ERROR) << "Failed to register window";
    DebugLogWinError();
    return false;
  }
  return true;
}

// Create a Windows window.
HWND CreateWindowInstance(const SbWindowOptions& options) {
  DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS |
                  WS_CLIPCHILDREN | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  if (options.windowed) {
    dwStyle |= WS_MAXIMIZE;
  }
  const std::wstring wide_window_name = CStringToWString(options.name);
  const HWND window = CreateWindow(
      kWindowClassName, wide_window_name.c_str(), dwStyle, CW_USEDEFAULT,
      CW_USEDEFAULT, options.size.width, options.size.height, nullptr, nullptr,
      GetModuleHandle(nullptr), nullptr);
  SetForegroundWindow(window);
  if (window == nullptr) {
    SB_LOG(ERROR) << "Failed to create window.";
    DebugLogWinError();
  }

  return window;
}

void AttachMiniDumpHandler(const CommandLine& cmd_line) {
  std::string file_path;
  if (cmd_line.HasSwitch(kMiniDumpFilePath)) {
    // If there is a mini dump file path then use that.
    file_path = cmd_line.GetSwitchValue(kMiniDumpFilePath);
  } else {
    // Otherwise use the file path and append ".dmp" to it.
    const auto& args = cmd_line.argv();
    if (!args.empty()) {
      file_path = args[0];
      file_path.append(".dmp");
    }
  }

  if (!file_path.empty()) {
    starboard::shared::win32::InitMiniDumpHandler(file_path.c_str());
  }
}

}  // namespace

// Note that this is a "struct" and not a "class" because
// that's how it's defined in starboard/system.h
struct SbSystemPlatformErrorPrivate {
  SbSystemPlatformErrorPrivate(const SbSystemPlatformErrorPrivate&) = delete;
  SbSystemPlatformErrorPrivate& operator=(const SbSystemPlatformErrorPrivate&) =
      delete;

  SbSystemPlatformErrorPrivate(SbSystemPlatformErrorType type,
                               SbSystemPlatformErrorCallback callback,
                               void* user_data)
      : callback_(callback), user_data_(user_data) {
    if (type != kSbSystemPlatformErrorTypeConnectionError)
      SB_NOTREACHED();

    ApplicationWin32* app = ApplicationWin32::Get();
    const bool created_dialog = starboard::shared::win32::ShowOkCancelDialog(
        app->GetCoreWindow()->GetWindowHandle(),
        "",  // No title.
        app->GetLocalizedString("UNABLE_TO_CONTACT_YOUTUBE_1",
                                "Sorry, could not connect to YouTube."),
        app->GetLocalizedString("RETRY_BUTTON", "Retry"),
        [this, callback, user_data]() {
          callback(kSbSystemPlatformErrorResponsePositive, user_data);
        },
        app->GetLocalizedString("EXIT_BUTTON", "Exit"),
        [this, callback, user_data]() {
          callback(kSbSystemPlatformErrorResponseNegative, user_data);
        });
    SB_DCHECK(!created_dialog);
    if (!created_dialog) {
      SB_LOG(ERROR) << "Failed to create dialog!";
    }
  }

  void Clear() { starboard::shared::win32::CancelDialog(); }

 private:
  SbSystemPlatformErrorCallback callback_;
  void* user_data_;
};

namespace starboard {
namespace shared {
namespace win32 {

ApplicationWin32::ApplicationWin32()
    : localized_strings_(SbSystemGetLocaleId()) {}
ApplicationWin32::~ApplicationWin32() {}

SbWindow ApplicationWin32::CreateWindowForWin32(
    const SbWindowOptions* options) {
  if (SbWindowIsValid(window_.get())) {
    SB_LOG(WARNING) << "Returning existing window instance.";
    return window_.get();
  }

  RegisterWindowClass();
  HWND window;
  if (options) {
    window = CreateWindowInstance(*options);
    window_.reset(new SbWindowPrivate(options, window));
  } else {
    SbWindowOptions default_options;
    SbWindowSetDefaultOptions(&default_options);
    window = CreateWindowInstance(default_options);
    window_.reset(new SbWindowPrivate(&default_options, window));
  }
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
  return window_.get();
}

bool ApplicationWin32::DestroyWindow(SbWindow window) {
  if (!SbWindowIsValid(window) || window != window_.get()) {
    return false;
  }
  window_.reset();
  return true;
}

SbSystemPlatformError ApplicationWin32::OnSbSystemRaisePlatformError(
    SbSystemPlatformErrorType type,
    SbSystemPlatformErrorCallback callback,
    void* user_data) {
  return new SbSystemPlatformErrorPrivate(type, callback, user_data);
}

void ApplicationWin32::OnSbSystemClearPlatformError(
    SbSystemPlatformError handle) {
  if (handle == kSbSystemPlatformErrorInvalid) {
    return;
  }
  static_cast<SbSystemPlatformErrorPrivate*>(handle)->Clear();
  // TODO: Determine if this should actually be deleted and if so, delete or
  // don't consistently across platforms.
  delete handle;
}

Application::Event* ApplicationWin32::WaitForSystemEventWithTimeout(
    SbTime time) {
  ProcessNextSystemMessage();
  if (pending_event_) {
    Event* out = pending_event_;
    pending_event_ = nullptr;
    return out;
  }

  ScopedLock lock(stop_waiting_for_system_events_mutex_);
  if (time <= SbTimeGetMonotonicNow() || stop_waiting_for_system_events_) {
    stop_waiting_for_system_events_ = false;
    return nullptr;
  }

  return WaitForSystemEventWithTimeout(time);
}

LRESULT ApplicationWin32::WindowProcess(HWND hWnd,
                                        UINT msg,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  switch (msg) {
    // Input message handling.
    case WM_MBUTTONDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
      pending_event_ =
          ProcessWinMouseEvent(GetCoreWindow(), msg, w_param, l_param);
      break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      pending_event_ =
          ProcessWinKeyEvent(GetCoreWindow(), msg, w_param, l_param);
      break;
    case WM_DESTROY:
      SB_LOG(INFO) << "Received destroy message; posting Quit message";
      // Pause and suspend the application first so we can do some cleanup
      // before the window is destroyed (e.g. stopping rasterization).
      DispatchAndDelete(new Event(kSbEventTypePause, NULL, NULL));
      DispatchAndDelete(new Event(kSbEventTypeSuspend, NULL, NULL));
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProcW(hWnd, msg, w_param, l_param);
  }
  return 0;
}

int ApplicationWin32::Run(int argc, char** argv) {
  CommandLine cmd_line(argc, argv);
  AttachMiniDumpHandler(cmd_line);
  int return_val = Application::Run(argc, argv);
  return return_val;
}

void ApplicationWin32::ProcessNextSystemMessage() {
  MSG msg;
  BOOL peek_message_return = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
  if (peek_message_return == 0) {  // 0 indicates no messages available.
    return;
  }

  if (!DialogHandleMessage(&msg)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  if (msg.message == WM_QUIT) {
    SB_LOG(INFO) << "Received Quit message; stopping application";
    SbSystemRequestStop(msg.wParam);
  }
}

Application::Event* ApplicationWin32::ProcessWinMouseEvent(SbWindow window,
                                                           UINT msg,
                                                           WPARAM w_param,
                                                           LPARAM l_param) {
  SbInputData* data = new SbInputData();
  SbMemorySet(data, 0, sizeof(*data));

  data->window = window;
  data->device_type = kSbInputDeviceTypeMouse;
  data->device_id = kSbMouseDeviceId;
  switch (msg) {
    case WM_LBUTTONDOWN:
      data->key = kSbKeyMouse1;
      data->type = kSbInputEventTypePress;
      break;
    case WM_RBUTTONDOWN:
      data->key = kSbKeyMouse2;
      data->type = kSbInputEventTypePress;
      break;
    case WM_MBUTTONDOWN:
      data->key = kSbKeyMouse3;
      data->type = kSbInputEventTypePress;
      break;
    case WM_LBUTTONUP:
      data->key = kSbKeyMouse1;
      data->type = kSbInputEventTypeUnpress;
      break;
    case WM_RBUTTONUP:
      data->key = kSbKeyMouse2;
      data->type = kSbInputEventTypeUnpress;
      break;
    case WM_MBUTTONUP:
      data->key = kSbKeyMouse3;
      data->type = kSbInputEventTypeUnpress;
      break;
    case WM_MOUSEMOVE:
      data->type = kSbInputEventTypeMove;
      break;
    case WM_MOUSEWHEEL: {
      data->type = kSbInputEventTypeWheel;
      int wheel_delta = GET_WHEEL_DELTA_WPARAM(w_param);
      // Per MSFT, standard mouse wheel increments are multiples of 120. For
      // smooth scrolling, this may be less.
      // https://msdn.microsoft.com/en-us/library/windows/desktop/ms645617(v=vs.85).aspx
      data->delta.y = wheel_delta / 120.0f;
    } break;
    default:
      SB_LOG(WARNING) << "Received unrecognized MSG code " << msg;
      return nullptr;
  }

  data->pressure = NAN;
  data->size = {NAN, NAN};
  data->tilt = {NAN, NAN};
  data->position.x = GET_X_LPARAM(l_param);
  data->position.y = GET_Y_LPARAM(l_param);

  return new Application::Event(kSbEventTypeInput, data,
                                &Application::DeleteDestructor<SbInputData>);
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
