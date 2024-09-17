// Copyright 2023 The Cobalt Authors. All rights reserved.

#include <dlfcn.h>

#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/thread.h"
#include "starboard/configuration_constants.h"
#include "starboard/egl.h"
#include "starboard/event.h"
#include "starboard/gles.h"
#include "starboard/system.h"
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CastStarboardApiTest : public ::testing::Test {
 public:
  // This acts as a proxy to the Starboard implementation provided by
  // `libcast_starboard_api.so`.
  struct CastStarboardApi {
    decltype(SbRunStarboardMain)* SbRunStarboardMain;
    decltype(SbEventSchedule)* SbEventSchedule;
    decltype(SbGetEglInterface)* SbGetEglInterface;
    decltype(SbGetGlesInterface)* SbGetGlesInterface;
    decltype(SbWindowCreate)* SbWindowCreate;
    decltype(SbWindowDestroy)* SbWindowDestroy;
    decltype(SbWindowGetPlatformHandle)* SbWindowGetPlatformHandle;
    decltype(SbSystemRequestStop)* SbSystemRequestStop;
  };

  CastStarboardApiTest();
  ~CastStarboardApiTest();

  // Receives events from the static |EventHandle|. If |event| is non-null,
  // add its type to |received_| so tests can see it. If |event| is null,
  // then it was a manually scheduled event and we signal |received_cond_|.
  void EventHandleInternal(const SbEvent* event);

  // Waits until |received_cond_| has been signalled; used to ensure the event
  // loop is running.
  void WaitForEventCallback();

  // Signals |received_cond_|.
  void EventCallbackInternal();

  CastStarboardApi& api() { return api_; }
  const std::vector<SbEventType>& events() { return received_; }

 private:
  class CastStarboardApiThread : public starboard::Thread {
   public:
    explicit CastStarboardApiThread(CastStarboardApi* api)
        : starboard::Thread("cast_thread"), api_(api) {}

    void Run() override;

   private:
    CastStarboardApi* api_;
  };

  template <class FuncType>
  void DlSym(void* lib, const char* func_name, FuncType* func) {
    *func = (FuncType)(dlsym(lib, func_name));
    EXPECT_NE(func, nullptr) << func_name << " could not be loaded";
  }

  CastStarboardApi api_;

  // These properties are used to initialize the main Starboard thread.
  std::unique_ptr<CastStarboardApiThread> sb_thread_;
  std::unique_ptr<starboard::ConditionVariable> started_cond_;
  starboard::Mutex started_mutex_;

  // These properties are used to track event dispatch during tests.
  std::vector<SbEventType> received_;
  starboard::Mutex received_mutex_;
  std::unique_ptr<starboard::ConditionVariable> received_cond_;
};

// A behavior in the default implementation prevents dlclose from being used on
// this library, so we must only open it once.
void* g_lib = nullptr;

// |EventHandleStatic| must be able to operate on the |g_test_instance|, so it's
// tracked here.
CastStarboardApiTest* g_test_instance = nullptr;

// Static callback for SbEvent(s); forwards to |EventHandleInternal|.
void EventHandleStatic(const SbEvent* event) {
  g_test_instance->EventHandleInternal(event);
}

// Static callback for manually scheduled events; forwards to
// |EventHandleInternal|.
void EventCallbackStatic(void* context) {
  static_cast<CastStarboardApiTest*>(context)->EventCallbackInternal();
}

CastStarboardApiTest::CastStarboardApiTest() {
  g_test_instance = this;
  started_cond_ =
      std::make_unique<starboard::ConditionVariable>(started_mutex_);
  received_cond_ =
      std::make_unique<starboard::ConditionVariable>(received_mutex_);

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
  DlSym(g_lib, "SbRunStarboardMain", &api_.SbRunStarboardMain);
  DlSym(g_lib, "SbEventSchedule", &api_.SbEventSchedule);
  DlSym(g_lib, "SbGetEglInterface", &api_.SbGetEglInterface);
  DlSym(g_lib, "SbGetGlesInterface", &api_.SbGetGlesInterface);
  DlSym(g_lib, "SbWindowCreate", &api_.SbWindowCreate);
  DlSym(g_lib, "SbWindowDestroy", &api_.SbWindowDestroy);
  DlSym(g_lib, "SbWindowGetPlatformHandle", &api_.SbWindowGetPlatformHandle);
  DlSym(g_lib, "SbSystemRequestStop", &api_.SbSystemRequestStop);

  // Start the Starboard thread and wait for kSbEventTypeStart to propagate.
  sb_thread_ = std::make_unique<CastStarboardApiThread>(&api_);
  sb_thread_->Start();

  // Watch event for initialation completion.
  started_mutex_.Acquire();
  started_cond_->Wait();
  started_mutex_.Release();
}

CastStarboardApiTest::~CastStarboardApiTest() {
  api().SbSystemRequestStop(0);
  sb_thread_->Join();
  sb_thread_.reset();
  started_cond_.reset();
  g_test_instance = nullptr;
}

void CastStarboardApiTest::EventHandleInternal(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart:
      started_cond_->Signal();
      break;
    default:
      break;
  }

  received_.push_back(event->type);
}

void CastStarboardApiTest::EventCallbackInternal() {
  received_cond_->Signal();
}

void CastStarboardApiTest::WaitForEventCallback() {
  received_mutex_.Acquire();
  received_cond_->Wait();
  received_mutex_.Release();
}

void CastStarboardApiTest::CastStarboardApiThread::Run() {
  api_->SbRunStarboardMain(0, nullptr, &EventHandleStatic);
}

// The default Application only be started once in the lifetime of the
// executable. To simplify and avoid race conditions in the test framework, both
// test scenarios are implemented in a single test case.
TEST_F(CastStarboardApiTest, EventLoop_CastWindowSurface) {
  // Test 1:
  // Ensure the event loop is running after initialization, and that SbEvent(s)
  // are being received. Effectively tests |SbRunStarboardMain|.
  api().SbEventSchedule(&EventCallbackStatic, this, 0);
  WaitForEventCallback();
  EXPECT_FALSE(events().empty());
  EXPECT_EQ(events().front(), kSbEventTypeStart);

  // Test 2:
  // Ensure that a window surface can be created which supports the config
  // required by Cast.
  const SbEglInterface* egl = api().SbGetEglInterface();

  // Cast expects `SB_EGL_DEFAULT_DISPLAY` to refer to the correct display.
  SbEglDisplay display = egl->eglGetDisplay(SB_EGL_DEFAULT_DISPLAY);
  EXPECT_TRUE(egl->eglInitialize(display, nullptr, nullptr))
      << "Failed eglInitialize for display: " << egl->eglGetError();

  SbWindow window = api().SbWindowCreate(nullptr);
  SbEglNativeWindowType window_type = reinterpret_cast<SbEglNativeWindowType>(
      api().SbWindowGetPlatformHandle(window));

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
  const SbGlesInterface* gles = api().SbGetGlesInterface();

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

  api().SbWindowDestroy(window);
}

}  // namespace
