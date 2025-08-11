#include "starboard/android/shared/video_surface_view.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard::android::shared {

pthread_once_t s_once_flag = PTHREAD_ONCE_INIT;
pthread_key_t s_thread_local_key = 0;

void InitThreadLocalKey() {
  [[maybe_unused]] int res = pthread_key_create(&s_thread_local_key, NULL);
  SB_DCHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInited() {
  pthread_once(&s_once_flag, InitThreadLocalKey);
}

void* GetSurfaceViewForCurrentThread() {
  EnsureThreadLocalKeyInited();
  // If the key is not valid or there is no value associated
  // with the key, it returns 0.
  return reinterpret_cast<uintptr_t>(pthread_getspecific(s_thread_local_key));
}

void SetVideoSurfaceViewForCurrentThread(void* surface_view) {
  EnsureThreadLocalKeyInited();
  pthread_setspecific(s_thread_local_key,
                      reinterpret_cast<void*>(surface_view));
}

}  // namespace starboard::android::shared
