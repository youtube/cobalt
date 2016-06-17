// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/thread.h"
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

  SbThreadLocalKey key;
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
  SbThreadSetLocalValue(real_context->key, real_context->in_value);
  real_context->out_value = SbThreadGetLocalValue(real_context->key);
  ThreadLocalValue* thread_local_value =
      static_cast<ThreadLocalValue*>(real_context->out_value);
  real_context->destroyed_before_exit = thread_local_value->destroyed;

  return NULL;
}

void DoSunnyDayTest(bool use_destructor) {
  const int kThreads = 16;
  ThreadLocalValue values[kThreads];
  Context contexts[kThreads];
  SbThread threads[kThreads];
  ThreadLocalValue my_value;

  SbThreadLocalKey key =
      SbThreadCreateLocalKey(use_destructor ? DestroyThreadLocalValue : NULL);
  EXPECT_TRUE(SbThreadIsValidLocalKey(key));
  SbThreadSetLocalValue(key, &my_value);
  for (int i = 0; i < kThreads; ++i) {
    contexts[i].key = key;
    contexts[i].in_value = &values[i];
  }

  for (int i = 0; i < kThreads; ++i) {
    threads[i] = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                true, NULL, EntryPoint, &contexts[i]);
  }

  for (int i = 0; i < kThreads; ++i) {
    EXPECT_TRUE(SbThreadIsValid(threads[i]));
    EXPECT_TRUE(SbThreadJoin(threads[i], NULL));
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
  SbThreadDestroyLocalKey(key);

  // The destructor will never be called on my_value.
  EXPECT_FALSE(my_value.destroyed);
}

TEST(SbThreadLocalValueTest, SunnyDay) {
  DoSunnyDayTest(true);
}

TEST(SbThreadLocalValueTest, SunnyDayNoDestructor) {
  DoSunnyDayTest(false);
}

TEST(SbThreadLocalValueTest, SunnyDayFreshlyCreatedValuesAreNull) {
  SbThreadLocalKey key = SbThreadCreateLocalKey(NULL);
  EXPECT_EQ(NULL, SbThreadGetLocalValue(key));

  // Some Starboard implementations may recycle the original key, so this test
  // ensures that in that case it will be reset to NULL.
  SbThreadSetLocalValue(key, reinterpret_cast<void*>(1));
  SbThreadDestroyLocalKey(key);

  key = SbThreadCreateLocalKey(NULL);
  EXPECT_EQ(NULL, SbThreadGetLocalValue(key));
  SbThreadDestroyLocalKey(key);
}

TEST(SbThreadLocalValueTest, SunnyDayMany) {
  const int kMany = (2 * SB_MAX_THREAD_LOCAL_KEYS) / 3;
  SbThreadLocalKey keys[kMany];

  for (int i = 0; i < kMany; ++i) {
    keys[i] = SbThreadCreateLocalKey(NULL);
    EXPECT_TRUE(SbThreadIsValidLocalKey(keys[i])) << "key #" << i;
  }

  for (int i = 0; i < kMany; ++i) {
    SbThreadDestroyLocalKey(keys[i]);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
