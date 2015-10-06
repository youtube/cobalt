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
#include "cobalt/dom/event_names.h"

namespace cobalt {
namespace system_window {

namespace {
const wchar_t* kClassName = L"CobaltMainWindow";
const wchar_t* kWindowTitle = L"Cobalt";
const char kThreadName[] = "Win32 Message Loop";
const int kWindowWidth = 1920;
const int kWindowHeight = 1080;

// Lazily created thread local storage to access the SystemWindowWin object
// defined in the current thread. The WndProc function starts getting called
// with messages before CreateWindowEx even returns, so this allows us to
// access the corresponding SystemWindowWin object in that function.
// This assumes that the function is always called on the same thread that the
// window was created on and the messages are processed on
// (the thread running Win32WindowThread).
base::LazyInstance<base::ThreadLocalPointer<SystemWindowWin> > lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

SystemWindowWin::SystemWindowWin()
    : thread(kThreadName),
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
      // The user expressed a desire to close the window
      // TODO(***REMOVED***) - Should really exit the loop and pass control
      // back to the browser to correctly clean up. However, this is
      // equivalent to Ctrl+C, which is what we are forced to use now.
      DLOG(INFO) << "User clicked X button on window " << system_window;
      raise(SIGINT);
      return 0;
    } break;
    case WM_KEYDOWN: {
      // The user has pressed a key on the keyboard.
      dom::KeyboardEvent::KeyLocationCode location =
          dom::KeyboardEvent::KeyCodeToKeyLocation(w_param);
      scoped_refptr<dom::KeyboardEvent> keyboard_event(
          new dom::KeyboardEvent(dom::EventNames::GetInstance()->keydown(),
                                 location, dom::KeyboardEvent::kNoModifier,
                                 w_param, w_param, IsKeyRepeat(l_param)));
      system_window->HandleKeyboardEvent(keyboard_event);
    } break;
    case WM_KEYUP: {
      // The user has released a key on the keyboard.
      dom::KeyboardEvent::KeyLocationCode location =
          dom::KeyboardEvent::KeyCodeToKeyLocation(w_param);
      scoped_refptr<dom::KeyboardEvent> keyboard_event(new dom::KeyboardEvent(
          dom::EventNames::GetInstance()->keyup(), location,
          dom::KeyboardEvent::kNoModifier, w_param, w_param, false));
      system_window->HandleKeyboardEvent(keyboard_event);
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
      kWindowWidth, kWindowHeight, NULL, NULL, module_instance, NULL);
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

}  // namespace system_window
}  // namespace cobalt
