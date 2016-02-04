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

#include "cobalt/system_window/win/system_window.h"

#include <csignal>

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "cobalt/system_window/application_event.h"
#include "cobalt/system_window/keyboard_event.h"

namespace cobalt {
namespace system_window {

namespace {
const wchar_t* kClassName = L"CobaltMainWindow";
const wchar_t* kWindowTitle = L"Cobalt";
const char kThreadName[] = "Win32 Message Loop";

// Lazily created thread local storage to access the SystemWindowWin object
// defined in the current thread. The WndProc function starts getting called
// with messages before CreateWindowEx even returns, so this allows us to
// access the corresponding SystemWindowWin object in that function.
// This assumes that the function is always called on the same thread that the
// window was created on and the messages are processed on
// (the thread running Win32WindowThread).
base::LazyInstance<base::ThreadLocalPointer<SystemWindowWin> > lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

inline bool IsKeyDown(int keycode) {
  // Key down state is indicated by high-order bit of SHORT.
  return (GetKeyState(keycode) & 0x8000) != 0;
}

unsigned int GetModifierKeyState() {
  unsigned int modifiers = KeyboardEvent::kNoModifier;
  if (IsKeyDown(VK_MENU)) {
    modifiers |= KeyboardEvent::kAltKey;
  }
  if (IsKeyDown(VK_CONTROL)) {
    modifiers |= KeyboardEvent::kCtrlKey;
  }
  if (IsKeyDown(VK_SHIFT)) {
    modifiers |= KeyboardEvent::kShiftKey;
  }
  return modifiers;
}
}  // namespace

SystemWindowWin::SystemWindowWin(base::EventDispatcher* event_dispatcher,
                                 const math::Size& window_size)
    : SystemWindow(event_dispatcher, window_size),
      thread(kThreadName),
      window_initialized(true, false),
      window_handle_(NULL) {
  StartWindow();
}

SystemWindowWin::~SystemWindowWin() { EndWindow(); }

void SystemWindowWin::StartWindow() {
  thread.Start();
  thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SystemWindowWin::Win32WindowThread, base::Unretained(this)));

  window_initialized.Wait();
}

LRESULT CALLBACK SystemWindowWin::WndProc(HWND window_handle, UINT message,
                                          WPARAM w_param, LPARAM l_param) {
  // Get the associated SystemWindowWin instance from thread local storage.
  // This assumes this function is always called on the same thread.
  SystemWindowWin* system_window = lazy_tls_ptr.Pointer()->Get();
  DCHECK(system_window);

  switch (message) {
    case WM_CLOSE: {
      DLOG(INFO) << "User clicked X button on window " << system_window;
      scoped_ptr<ApplicationEvent> quit_event(
          new ApplicationEvent(ApplicationEvent::kQuit));
      system_window->event_dispatcher()->DispatchEvent(
          quit_event.PassAs<base::Event>());
      return 0;
    } break;
    case WM_KEYDOWN: {
      int key_code = w_param;
      bool is_repeat = IsKeyRepeat(l_param);
      scoped_ptr<KeyboardEvent> keyboard_event(new KeyboardEvent(
          KeyboardEvent::kKeyDown, key_code, GetModifierKeyState(), is_repeat));
      system_window->event_dispatcher()->DispatchEvent(
          keyboard_event.PassAs<base::Event>());
    } break;
    case WM_KEYUP: {
      // The user has released a key on the keyboard.
      int key_code = w_param;
      scoped_ptr<KeyboardEvent> keyboard_event(new KeyboardEvent(
          KeyboardEvent::kKeyUp, key_code, GetModifierKeyState(), false));
      system_window->event_dispatcher()->DispatchEvent(
          keyboard_event.PassAs<base::Event>());
    } break;
  }

  return DefWindowProc(window_handle, message, w_param, l_param);
}

bool SystemWindowWin::IsKeyRepeat(LPARAM l_param) {
  const LPARAM kPreviousStateMask = 1 << 30;
  return (l_param & kPreviousStateMask) != 0L;
}

void SystemWindowWin::EndWindow() {
  PostMessage(window_handle_, WM_QUIT, 0, 0);
}

void SystemWindowWin::Win32WindowThread() {
  // Store the pointer to this object in thread-local storage.
  // This lets us access this object in the WndProc function, assuming that
  // function is always called on this thread.
  DCHECK(lazy_tls_ptr.Pointer()->Get() == NULL);
  lazy_tls_ptr.Pointer()->Set(this);
  DCHECK(lazy_tls_ptr.Pointer()->Get() == this);

  HINSTANCE module_instance = GetModuleHandle(NULL);

  WNDCLASS window_class;
  window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  window_class.lpfnWndProc = (WNDPROC)WndProc;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = module_instance;
  window_class.hIcon = LoadIcon(NULL, IDI_WINLOGO);
  window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  window_class.hbrBackground = NULL;
  window_class.lpszMenuName = NULL;
  window_class.lpszClassName = kClassName;
  CHECK(RegisterClass(&window_class));

  window_handle_ = CreateWindowEx(
      WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, kClassName, kWindowTitle,
      WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW, 0, 0,
      window_size().width(), window_size().height(), NULL, NULL,
      module_instance, NULL);
  CHECK(window_handle_);

  ShowWindow(window_handle_, SW_SHOW);

  window_initialized.Signal();

  MSG message;
  while (GetMessage(&message, NULL, 0, 0)) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  CHECK(DestroyWindow(window_handle()));
  CHECK(UnregisterClass(kClassName, module_instance));
  lazy_tls_ptr.Pointer()->Set(NULL);
  DCHECK(lazy_tls_ptr.Pointer()->Get() == NULL);
}

scoped_ptr<SystemWindow> CreateSystemWindow(
    base::EventDispatcher* event_dispatcher, const math::Size& window_size) {
  return scoped_ptr<SystemWindow>(
      new SystemWindowWin(event_dispatcher, window_size));
}

}  // namespace system_window
}  // namespace cobalt
