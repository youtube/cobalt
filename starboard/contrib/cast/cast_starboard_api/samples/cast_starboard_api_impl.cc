// Copyright 2022 Google LLC. All rights reserved.

#include "../cast_starboard_api.h"

#include <memory>

#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/thread.h"
#include "starboard/egl.h"
#include "starboard/system.h"

#ifdef CAST_STARBOARD_API_X11
#include <X11/Xlib.h>
#include "starboard/shared/x11/application_x11.h"
#endif  // CAST_STARBOARD_API_X11

#ifndef CAST_STARBOARD_API_X11
extern "C" int StarboardMain(int argc, char** argv);
#endif

namespace {
class CastStarboardApiThread : public starboard::Thread {
 public:
  CastStarboardApiThread() : starboard::Thread("cast_thread") {}

  void Run() override {
#ifdef CAST_STARBOARD_API_X11
    // In this sample implementation, setting up the application is
    // required for some APIs to function correctly. For example,`
    // SbWindowCreate will crash without an existing ApplicationX11.
    starboard::shared::starboard::CommandLine command_line(0, nullptr);
    app = std::make_unique<starboard::shared::x11::ApplicationX11>();

    // This never returns until kSbEventTypeStop is sent to the application,
    // so it can act as the thread loop.
    app->Run(command_line);
#else   // CAST_STARBOARD_API_X11
    // **NOTE:** Calling StarboardMain(...) here should be enough, without
    // needing any other content. Typically StarboardMain is redefined from
    // main in builds where `final_executable_type == "shared_library"`.
    StarboardMain(0, nullptr);
#endif  // CAST_STARBOARD_API_X11
  }

 private:
#ifdef CAST_STARBOARD_API_X11
  std::unique_ptr<starboard::shared::x11::ApplicationX11> app;
#endif  // CAST_STARBOARD_API_X11
};

void (*g_callback)(const SbEvent*) = nullptr;
starboard::Mutex g_started_mutex;
std::unique_ptr<starboard::ConditionVariable> g_started_cond;
std::unique_ptr<CastStarboardApiThread> g_thread;
bool g_initialized = false;
}  // namespace

int CastStarboardApiInitialize(int argc,
                               char** argv,
                               void (*callback)(const SbEvent*)) {
  SB_CHECK(!g_thread) << "CastStarboardApiInitialize may only be called once";
  SB_CHECK(callback) << "Argument 'callback' must not be NULL";

  // Events given to SbEventHandle will be forwarded to |callback|.
  g_callback = callback;

  // Create event for initialization completion.
  g_started_cond =
      std::make_unique<starboard::ConditionVariable>(g_started_mutex);

  // Create the main Starboard thread.
  g_thread = std::make_unique<CastStarboardApiThread>();
  g_thread->Start();

  // Watch event for initialation completion.
  g_started_mutex.Acquire();
  g_started_cond->Wait();
  g_started_mutex.Release();

  return 0;
}

void CastStarboardApiFinalize() {
  SB_CHECK(g_thread) << "CastStarboardApiFinalize may only be called after "
                        "CastStarboardApiInitialize";

  // |g_thread| cannot join until its internal event loop stops.
  SbSystemRequestStop(0);
  g_thread->Join();

  // The thread is stopped so it's safe to reset.
  g_initialized = false;
  g_thread.reset();
  g_started_cond.reset();
  g_callback = nullptr;
}

void SbEventHandle(const SbEvent* event) {
  SB_DCHECK(g_callback);
  // Signal that initialization is complete.
  if (event->type == kSbEventTypeStart) {
    g_started_cond->Signal();
    g_initialized = true;
  }

  if (g_initialized) {
    g_callback(event);
  }
}
