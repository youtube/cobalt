// Copyright 2022 Google LLC. All rights reserved.

#include <dlfcn.h>

#include "../cast_starboard_api.h"

#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/egl.h"
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/gles.h"
#include "starboard/system.h"
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CastStarboardApiTest : public ::testing::Test {
 public:
  struct CastStarboardApi {
    // Cast-specific APIs
    decltype(CastStarboardApiInitialize)* Initialize;
    decltype(CastStarboardApiFinalize)* Finalize;

    // Starboard APIs
    decltype(SbEventSchedule)* EventSchedule;
    decltype(SbGetEglInterface)* GetEglInterface;
    decltype(SbGetGlesInterface)* GetGlesInterface;
    decltype(SbWindowCreate)* WindowCreate;
    decltype(SbWindowGetPlatformHandle)* WindowGetPlatformHandle;
  };

  CastStarboardApiTest();
  ~CastStarboardApiTest();

  // Receives events from the static |EventHandle|. If |event| is non-null,
  // add its type to |received_| so tests can see it. If |event| is null,
  // then it was a manually scheduled event and we signal |started_cond|.
  void EventHandleInternal(const SbEvent* event);

  // Waits until |started_cond| has been signalled; used to ensure the event
  // loop is running.
  void WaitForEventCallback();

  CastStarboardApi& api() { return api_; }
  const std::vector<SbEventType>& events() { return received_; }

 private:
  template <class FuncType>
  void DlSym(void* lib, const char* func_name, FuncType* func) {
    *func = (FuncType)(dlsym(lib, func_name));
    EXPECT_NE(func, nullptr) << func_name << " could not be loaded";
  }

  CastStarboardApi api_;
  std::vector<SbEventType> received_;
  starboard::Mutex started_mutex;
  std::unique_ptr<starboard::ConditionVariable> started_cond;
};

// A bug in the default implementation prevents dlclose from being used on
// this library, so we must only open it once.
void* g_lib = nullptr;

// |EventHandle| is a static function but it must be able to operate on the
// |g_test_instance|, so we store it here as a static global.
CastStarboardApiTest* g_test_instance = nullptr;

CastStarboardApiTest::CastStarboardApiTest() {
  g_test_instance = this;
  started_cond = std::make_unique<starboard::ConditionVariable>(started_mutex);

  // Ensure libcast_starboard_api.so has been opened.
  if (!g_lib) {
    std::vector<char> content_path(kSbFileMaxPath + 1);
    EXPECT_TRUE(SbSystemGetPath(kSbSystemPathContentDirectory,
                                content_path.data(), kSbFileMaxPath + 1));
    const std::string lib_path = std::string(content_path.data()) +
                                 kSbFileSepChar + "libcast_starboard_api.so";
    g_lib = dlopen(lib_path.c_str(), RTLD_LAZY);
    EXPECT_NE(g_lib, nullptr)
        << lib_path << " could not be loaded: " << dlerror();
  }

  // Assign the |api_| methods.
  DlSym(g_lib, "CastStarboardApiInitialize", &api_.Initialize);
  DlSym(g_lib, "CastStarboardApiFinalize", &api_.Finalize);
  DlSym(g_lib, "SbEventSchedule", &api_.EventSchedule);
  DlSym(g_lib, "SbGetEglInterface", &api_.GetEglInterface);
  DlSym(g_lib, "SbGetGlesInterface", &api_.GetGlesInterface);
  DlSym(g_lib, "SbWindowCreate", &api_.WindowCreate);
  DlSym(g_lib, "SbWindowGetPlatformHandle", &api_.WindowGetPlatformHandle);
}

CastStarboardApiTest::~CastStarboardApiTest() {
  g_test_instance = nullptr;
}

void CastStarboardApiTest::EventHandleInternal(const SbEvent* event) {
  if (!event) {
    started_cond->Signal();
  } else {
    received_.push_back(event->type);
  }
}

void CastStarboardApiTest::WaitForEventCallback() {
  started_mutex.Acquire();
  started_cond->Wait();
  started_mutex.Release();
}

// Static callback for manually scheduled events; forwards to
// |EventHandleInternal|.
void EventCallback(void* context) {
  static_cast<CastStarboardApiTest*>(context)->EventHandleInternal(nullptr);
}
// Static callback for SbEvent(s); forwards to |EventHandleInternal|.
void EventHandle(const SbEvent* event) {
  g_test_instance->EventHandleInternal(event);
}

// Ensure the event loop is running after initialization, and that SbEvent(s)
// are being received.
TEST_F(CastStarboardApiTest, EventLoop) {
  api().Initialize(0, nullptr, &EventHandle);
  api().EventSchedule(&EventCallback, this, 0);
  WaitForEventCallback();
  EXPECT_FALSE(events().empty());
  EXPECT_EQ(events().front(), kSbEventTypeStart);
  api().Finalize();
}

// Ensure that a window surface can be created which supports the config
// required by Cast.
TEST_F(CastStarboardApiTest, CastWindowSurface) {
  api().Initialize(0, nullptr, &EventHandle);

  const SbEglInterface* egl = api().GetEglInterface();
  SbEglDisplay display = egl->eglGetDisplay(SB_EGL_DEFAULT_DISPLAY);
  EXPECT_TRUE(egl->eglInitialize(display, nullptr, nullptr))
      << "Failed eglInitialize for display: " << egl->eglGetError();

  SbWindow window = api().WindowCreate(nullptr);
  SbEglNativeWindowType window_type = reinterpret_cast<SbEglNativeWindowType>(
      api().WindowGetPlatformHandle(window));

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
                                 SB_EGL_WINDOW_BIT,
                                 SB_EGL_NONE};

  int32_t num_configs;
  EXPECT_TRUE(
      egl->eglChooseConfig(display, config_attribs, nullptr, 0, &num_configs))
      << "Failed eglChooseConfig for the specified config_attribs: "
      << egl->eglGetError();
  EXPECT_NE(num_configs, 0) << "No suitable EGL configs found.";

  std::unique_ptr<SbEglConfig[]> configs(new SbEglConfig[num_configs]);
  EXPECT_TRUE(egl->eglChooseConfig(display, config_attribs, configs.get(),
                                   num_configs, &num_configs))
      << "Failed eglChooseConfig: " << egl->eglGetError();

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

  EXPECT_NE(surface, nullptr)
      << "Failed eglCreateWindowSurface for all configs: "
      << egl->eglGetError();
  const SbGlesInterface* gles = api().GetGlesInterface();

  const SbEglInt32 context_attribs[] = {SB_EGL_CONTEXT_CLIENT_VERSION, 2,
                                        SB_EGL_NONE};

  SbEglContext context =
      egl->eglCreateContext(display, config, NULL, context_attribs);
  egl->eglMakeCurrent(display, SB_EGL_NO_SURFACE, SB_EGL_NO_SURFACE, context);
  std::string version(
      reinterpret_cast<const char*>(gles->glGetString(SB_GL_VERSION)));
  const std::string prefix = "OpenGL ES ";
  EXPECT_TRUE(version.find(prefix, 0) == 0);
  EXPECT_GT(version.length(), prefix.length() + 1);

  char actual_version = version.at(prefix.length());
  if (!(actual_version == '2' || actual_version == '3')) {
    // Normally we could check whether gles->glGetStringi (or other OpenGL 3+
    // functions) are non-null, but various Starboard implementations actually
    // report the underlying version even if OpenGL 3+ functions are not
    // exposed. Cast will automatically treat any valid OpenGL 2+ version as
    // OpenGL ES 2.0.
    FAIL() << "Expected OpenGL ES 2 or 3.";
  }

  api().Finalize();
}

}  // namespace