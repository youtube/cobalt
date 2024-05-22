// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>

#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

struct ThreadLocalValue {
  ThreadLocalValue() : destroyed(false) {}
  bool destroyed;
};

struct Context {
  Context() : key(), in_value(), out_value(), destroyed_before_exit(false) {}

  pthread_key_t key;
  void* in_value;
  void* out_value;
  bool destroyed_before_exit;
};

void DestroyThreadLocalValue(void* value) {
  ThreadLocalValue* thread_local_value = static_cast<ThreadLocalValue*>(value);
  thread_local_value->destroyed = true;
}

void* EntryPoint(void* context) {
  Context* real_context = static_cast<Context*>(context);
  pthread_setspecific(real_context->key, real_context->in_value);
  real_context->out_value = pthread_getspecific(real_context->key);
  ThreadLocalValue* thread_local_value =
      static_cast<ThreadLocalValue*>(real_context->out_value);
  real_context->destroyed_before_exit = thread_local_value->destroyed;

  return NULL;
}

// Sets a thread local non-NULL value, and then sets it back to NULL.
static void* ThreadEntryPoint(void* ptr) {
  pthread_setname_np(pthread_self(), "TestThread");
  pthread_key_t key = *static_cast<pthread_key_t*>(ptr);
  EXPECT_EQ(NULL, pthread_getspecific(key));
  // Set the value and then NULL it out. We expect that because the final
  // value set was NULL, that the destructor attached to the thread's
  // at-exit function will not run.
  pthread_setspecific(key, reinterpret_cast<void*>(1));
  pthread_setspecific(key, NULL);
  return NULL;
}

void DoSunnyDayTest(bool use_destructor) {
  const int kThreads = 16;
  ThreadLocalValue values[kThreads];
  Context contexts[kThreads];
  pthread_t threads[kThreads];
  ThreadLocalValue my_value;

  pthread_key_t key = 0;
  EXPECT_EQ(
      pthread_key_create(&key, use_destructor ? DestroyThreadLocalValue : NULL),
      0);
  pthread_setspecific(key, &my_value);
  for (int i = 0; i < kThreads; ++i) {
    contexts[i].key = key;
    contexts[i].in_value = &values[i];
  }

  for (int i = 0; i < kThreads; ++i) {
    pthread_create(&threads[i], NULL, EntryPoint, &contexts[i]);
  }

  for (int i = 0; i < kThreads; ++i) {
    EXPECT_TRUE(threads[i] != 0);
    EXPECT_TRUE(pthread_join(threads[i], NULL) == 0);
    EXPECT_EQ(contexts[i].in_value, contexts[i].out_value);

    // The destructor for all thread-local values will be called at thread exit
    // time.
    EXPECT_FALSE(contexts[i].destroyed_before_exit);
    if (use_destructor) {
      EXPECT_TRUE(values[i].destroyed);
    } else {
      EXPECT_FALSE(values[i].destroyed);
    }
  }

  EXPECT_FALSE(my_value.destroyed);
  pthread_key_delete(key);

  // The destructor will never be called on my_value.
  EXPECT_FALSE(my_value.destroyed);
}

// Helper function that ensures that the returned key is not recycled.
pthread_key_t CreateTLSKey_NoRecycle(void (*dtor)(void*)) {
  pthread_key_t key = 0;
  pthread_key_create(&key, NULL);
  EXPECT_EQ(NULL, pthread_getspecific(key));
  // Some Starboard implementations may recycle the original key, so this test
  // ensures that in that case it will be reset to NULL.
  pthread_setspecific(key, reinterpret_cast<void*>(1));
  pthread_key_delete(key);
  pthread_key_create(&key, DestroyThreadLocalValue);
  return key;
}

// Tests the expectation that thread at-exit destructors don't
// run for ThreadLocal pointers that are set to NULL.
TEST(PosixThreadLocalValueTest, NoDestructorsForNullValue) {
  static int s_num_destructor_calls = 0;  // Must be initialized to 0.
  s_num_destructor_calls = 0;             // Allows test to be re-run.

  // Setup the thread key and bind the fake test destructor.
  pthread_key_t key =
      CreateTLSKey_NoRecycle([](void* value) { s_num_destructor_calls++; });
  EXPECT_EQ(NULL, pthread_getspecific(key));

  // Spawn the thread.
  pthread_t thread = 0;
  pthread_create(&thread, NULL, ThreadEntryPoint, static_cast<void*>(&key));

  ASSERT_TRUE(thread != 0) << "Thread creation not successful";
  // 2nd param is return value from ThreadEntryPoint, which is always NULL.
  ASSERT_TRUE(pthread_join(thread, NULL) == 0);

  // No destructors should have run.
  EXPECT_EQ(0, s_num_destructor_calls);

  pthread_key_delete(key);
}

TEST(PosixThreadLocalValueTest, SunnyDay) {
  DoSunnyDayTest(true);
}

TEST(PosixThreadLocalValueTest, SunnyDayNoDestructor) {
  DoSunnyDayTest(false);
}

TEST(PosixThreadLocalValueTest, SunnyDayFreshlyCreatedValuesAreNull) {
  pthread_key_t key = CreateTLSKey_NoRecycle(NULL);  // NULL dtor.
  EXPECT_EQ(NULL, pthread_getspecific(key));

  EXPECT_EQ(NULL, pthread_getspecific(key));
  pthread_key_delete(key);
}

TEST(PosixThreadLocalValueTest, SunnyDayMany) {
  const int kMany = (2 * kSbMaxThreadLocalKeys) / 3;
  std::vector<pthread_key_t> keys(kMany);

  for (int i = 0; i < kMany; ++i) {
    pthread_key_create(&keys[i], NULL);
    EXPECT_TRUE(keys[i] != 0) << "key #" << i;
  }

  for (int i = 0; i < kMany; ++i) {
    pthread_key_delete(keys[i]);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
