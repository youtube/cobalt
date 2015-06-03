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

#include "cobalt/renderer/backend/default_graphics_system.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/bind.h"

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"

namespace cobalt {
namespace renderer {
namespace backend {

namespace {
const wchar_t* kClassName = L"CobaltDefaultWindow";
const wchar_t* kWindowTitle = L"Cobalt";

LRESULT CALLBACK
WndProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
  switch (message) {
    case WM_CLOSE: {
      // The user expressed a desire to close the window
      // (e.g. clicked on the X button).  We ignore this for now.
      return 0;
    } break;
  }

  return DefWindowProc(window, message, w_param, l_param);
}

struct ThreadInfo {
  ThreadInfo()
      : thread("Win32 Message Loop"),
        window_initialized(true, false),
        window(NULL) {}

  base::Thread thread;
  base::WaitableEvent window_initialized;
  HWND window;
};

void Win32WindowThread(ThreadInfo* thread_info) {
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

  thread_info->window = CreateWindowEx(
      WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, kClassName, kWindowTitle,
      WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW, 0, 0, 1920, 1080,
      NULL, NULL, module_instance, NULL);
  CHECK(thread_info->window);

  ShowWindow(thread_info->window, SW_SHOW);

  thread_info->window_initialized.Signal();

  MSG message;
  while (GetMessage(&message, NULL, 0, 0)) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  CHECK(DestroyWindow(thread_info->window));
  CHECK(UnregisterClass(kClassName, module_instance));
}

class WindowThreadManager {
 public:
  static WindowThreadManager* GetInstance() {
    return Singleton<WindowThreadManager>::get();
  }

  HWND StartWindow();
  void EndWindow(HWND window);

 private:
  WindowThreadManager() {}
  friend struct DefaultSingletonTraits<WindowThreadManager>;

  base::hash_map<HWND, ThreadInfo*> thread_map_;

  DISALLOW_COPY_AND_ASSIGN(WindowThreadManager);
};

HWND WindowThreadManager::StartWindow() {
  ThreadInfo* thread_info = new ThreadInfo();
  thread_info->thread.Start();

  thread_info->thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&Win32WindowThread, thread_info));
  thread_info->window_initialized.Wait();

  thread_map_[thread_info->window] = thread_info;
  return thread_info->window;
}

void WindowThreadManager::EndWindow(HWND window) {
  DCHECK(thread_map_.find(window) != thread_map_.end());
  ThreadInfo* thread_info = thread_map_[window];
  thread_map_.erase(thread_info->window);

  PostMessage(thread_info->window, WM_QUIT, 0, 0);

  delete thread_info;
}
}  // namespace

scoped_ptr<GraphicsSystem> CreateDefaultGraphicsSystem() {
  return scoped_ptr<GraphicsSystem>(new GraphicsSystemEGL(
      base::Bind(&WindowThreadManager::StartWindow,
                 base::Unretained(WindowThreadManager::GetInstance())),
      base::Bind(&WindowThreadManager::EndWindow,
                 base::Unretained(WindowThreadManager::GetInstance()))));
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
