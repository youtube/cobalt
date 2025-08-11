#include "starboard/android/shared/video_surface_view.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard::android::shared {

pthread_once_t s_once_flag_for_video_surface_view = PTHREAD_ONCE_INIT;
pthread_key_t s_thread_local_key_for_video_surface_view = 0;

void InitThreadLocalKeyForVideoSurfaceView() {
  [[maybe_unused]] int res =
      pthread_key_create(&s_thread_local_key_for_video_surface_view, NULL);
  SB_DCHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInitedForVideoSurfaceView() {
  pthread_once(&s_once_flag_for_video_surface_view,
               InitThreadLocalKeyForVideoSurfaceView);
}

void* GetSurfaceViewForCurrentThread() {
  EnsureThreadLocalKeyInitedForVideoSurfaceView();
  // If the key is not valid or there is no value associated
  // with the key, it returns 0.
  return pthread_getspecific(s_thread_local_key_for_video_surface_view);
}

void SetVideoSurfaceViewForCurrentThread(void* surface_view) {
  EnsureThreadLocalKeyInitedForVideoSurfaceView();
  pthread_setspecific(s_thread_local_key_for_video_surface_view,
                      reinterpret_cast<void*>(surface_view));
}

}  // namespace starboard::android::shared
