#include <condition_variable>
#include <iostream>
#include <mutex>
#include <ostream>
#include <vector>

#include "../cast_starboard_api.h"
#include "starboard/egl.h"
#include "starboard/system.h"

namespace {
std::mutex g_condvar_mutext;
std::condition_variable g_event_cond;
std::vector<SbEventType> g_received;
bool g_started_event = false;
bool g_other_event = false;

// Static callback for SbEvent(s); forwards to |EventHandleInternal|.
void EventHandle(const SbEvent* event) {
  g_received.push_back(event->type);

  if (event->type == kSbEventTypeStart) {
    g_started_event = true;
  } else if (event->type == kSbEventTypeFocus) {
    g_other_event = true;
  }

  g_event_cond.notify_all();
}
}  // namespace

int main(int argc, char** argv) {
  std::cout << "Calling CastStarboardApiInitialize.\n" << std::flush;
  CastStarboardApiInitialize(argc, argv, &EventHandle);

  std::cout << "Waiting for EventHandle to be called.\n" << std::flush;
  {
    std::unique_lock<std::mutex> lock(g_condvar_mutext);
    g_event_cond.wait(lock, [] { return g_started_event; });
    lock.unlock();
  }

  std::cout << "Checking that the first event was kSbEventTypeStart.\n"
            << std::flush;
  if (!(!g_received.empty() && g_received.front() == kSbEventTypeStart)) {
    std::cout << "Failed because the first event was not kSbEventTypeStart : "
              << g_received.front() << std::flush;
    exit(1);
  }

  std::cout << "Checking that SbGetEglInterface() returns a value.\n"
            << std::flush;
  const SbEglInterface* egl = SbGetEglInterface();
  if (!egl) {
    std::cout << "Failed because SbGetEglInterface() did not return a value: "
              << (void*)egl << std::flush;
    exit(1);
  }

  std::cout << "Calling eglGetDisplay(SB_EGL_DEFAULT_DISPLAY).\n"
            << std::flush;
  SbEglDisplay display = egl->eglGetDisplay(SB_EGL_DEFAULT_DISPLAY);
  if (!(egl->eglInitialize(display, nullptr, nullptr))) {
    std::cout << "Failed eglInitialize for display: " << egl->eglGetError()
              << std::flush;
    exit(1);
  }

  std::cout << "Calling SbWindowCreate(...) with default parameters.\n"
            << std::flush;
  SbWindow window = SbWindowCreate(nullptr);
  std::cout
      << "Calling SbWindowGetPlatformHandle(...) with the returned window.\n"
      << std::flush;
  SbEglNativeWindowType window_type = reinterpret_cast<SbEglNativeWindowType>(
      SbWindowGetPlatformHandle(window));

  SbEglInt32 config_attribs[] = {SB_EGL_BUFFER_SIZE,
                                 32,
                                 SB_EGL_ALPHA_SIZE,
                                 8,
                                 SB_EGL_BLUE_SIZE,
                                 8,
                                 SB_EGL_GREEN_SIZE,
                                 8,
                                 SB_EGL_RED_SIZE,
                                 8,
                                 SB_EGL_RENDERABLE_TYPE,
                                 SB_EGL_OPENGL_ES2_BIT,
                                 SB_EGL_SURFACE_TYPE,
                                 SB_EGL_WINDOW_BIT | SB_EGL_PBUFFER_BIT,
                                 SB_EGL_NONE};

  std::cout
      << "Calling eglChooseConfig(...) with a config_attribs used by Cast.\n"
      << std::flush;
  int32_t num_configs = 0;
  if (!(egl->eglChooseConfig(display, config_attribs, nullptr, 0,
                             &num_configs))) {
    std::cout << "Failed eglChooseConfig for the specified config_attribs: "
              << egl->eglGetError() << std::flush;
    exit(1);
  }

  if (num_configs == 0) {
    std::cout << "No suitable EGL configs found." << std::flush;
    exit(1);
  }

  std::unique_ptr<SbEglConfig[]> configs(new SbEglConfig[num_configs]);
  if (!(egl->eglChooseConfig(display, config_attribs, configs.get(),
                             num_configs, &num_configs))) {
    std::cout << "Failed eglChooseConfig: " << egl->eglGetError() << std::flush;
    exit(1);
  }

  std::cout
      << "Calling eglCreateWindowSurface(...) with the returned configs.\n"
      << std::flush;
  SbEglSurface surface = nullptr;
  SbEglConfig config;
  for (int i = 0; i < num_configs; i++) {
    surface =
        egl->eglCreateWindowSurface(display, configs[i], window_type, NULL);
    if (surface) {
      config = configs[i];
      egl->eglDestroySurface(display, surface);
      break;
    }
  }

  if (!(surface != nullptr)) {
    std::cout << "Failed eglCreateWindowSurface for all configs: "
              << egl->eglGetError() << std::flush;
    exit(1);
  }

  const SbEglInt32 context_attribs[] = {SB_EGL_CONTEXT_CLIENT_VERSION, 2,
                                        SB_EGL_NONE};

  std::cout << "Calling eglCreateContext(...) with the working config and "
               "eglCreateContext(...) with the returned context.\n"
            << std::flush;
  SbEglContext context =
      egl->eglCreateContext(display, config, NULL, context_attribs);
  egl->eglMakeCurrent(display, SB_EGL_NO_SURFACE, SB_EGL_NO_SURFACE, context);

  std::cout << "Checking that SbGetGlesInterface() returns a value.\n"
            << std::flush;
  const SbGlesInterface* gles = SbGetGlesInterface();
  if (!gles) {
    std::cout << "Failed because SbGetGlesInterface() did not return a value: "
              << (void*)gles << std::flush;
    exit(1);
  }

  std::cout << "Calling CastStarboardApiFinalize.\n" << std::flush;
  CastStarboardApiFinalize();
  std::cout << "All tests passed.\n" << std::flush;
}