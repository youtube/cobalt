// Copyright 2016 Google Inc. All Rights Reserved.
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

// Adapted from base's atomicops_unittest.

#include "starboard/atomic.h"
#include "starboard/memory.h"
#include "starboard/thread.h"
#include "starboard/time.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Sets up an empty test fixture, required for typed tests.
// BasicSbAtomicTest contains test cases all enabled atomic types should pass.
template <class SbAtomicType>
class BasicSbAtomicTest : public testing::Test {
 public:
  BasicSbAtomicTest() {}
  virtual ~BasicSbAtomicTest() {}
};

// AdvancedSbAtomicTest provides test coverage for all atomic functions.
// Atomic8 is right now the only atomic type not required to pass
// AdvancedSbAtomicTest, as it does not support implementations for the full
// set of atomic functions like the other types do.
template <class SbAtomicType>
class AdvancedSbAtomicTest : public BasicSbAtomicTest<SbAtomicType> {
 public:
  AdvancedSbAtomicTest() {}
  virtual ~AdvancedSbAtomicTest() {}
};

typedef testing::Types<
#if SB_API_VERSION >= SB_INTRODUCE_ATOMIC8_VERSION
                       SbAtomic8,
#endif
                       SbAtomic32,
#if SB_HAS(64_BIT_ATOMICS)
                       SbAtomic64,
#endif
                       SbAtomicPtr> BasicSbAtomicTestTypes;
typedef testing::Types<SbAtomic32,
#if SB_HAS(64_BIT_ATOMICS)
                       SbAtomic64,
#endif
                       SbAtomicPtr> AdvancedSbAtomicTestTypes;

#if SB_API_VERSION >= SB_INTRODUCE_ATOMIC8_VERSION
TYPED_TEST_CASE(BasicSbAtomicTest, BasicSbAtomicTestTypes);
#else
TYPED_TEST_CASE(BasicSbAtomicTest, AdvancedSbAtomicTestTypes);
#endif

TYPED_TEST_CASE(AdvancedSbAtomicTest, AdvancedSbAtomicTestTypes);

// Return an SbAtomicType with the value 0xa5a5a5...
template <class SbAtomicType>
SbAtomicType TestFillValue() {
  SbAtomicType val = 0;
  SbMemorySet(&val, 0xa5, sizeof(SbAtomicType));
  return val;
}

// Returns the number of bits in a type.
template <class T>
T Bits() {
  return (sizeof(T) * 8);
}

// Returns a value with the high bit |bits| from the most significant bit set.
template <class SbAtomicType>
SbAtomicType GetHighBitAt(int bits) {
  return (SbAtomicType(1) << (Bits<SbAtomicType>() - (bits + 1)));
}

// Produces a test value that has non-zero bits in both halves, for testing
// 64-bit implementation on 32-bit platforms.
template <class SbAtomicType>
SbAtomicType GetTestValue() {
  return GetHighBitAt<SbAtomicType>(1) + 11;
}

TYPED_TEST(BasicSbAtomicTest, ReleaseCompareAndSwapSingleThread) {
  TypeParam value = 0;
  TypeParam prev = atomic::Release_CompareAndSwap(&value, 0, 1);
  EXPECT_EQ(1, value);
  EXPECT_EQ(0, prev);

  const TypeParam kTestValue = GetTestValue<TypeParam>();
  value = kTestValue;
  prev = atomic::Release_CompareAndSwap(&value, 0, 5);
  EXPECT_EQ(kTestValue, value);
  EXPECT_EQ(kTestValue, prev);

  value = kTestValue;
  prev = atomic::Release_CompareAndSwap(&value, kTestValue, 5);
  EXPECT_EQ(5, value);
  EXPECT_EQ(kTestValue, prev);
}

TYPED_TEST(BasicSbAtomicTest, NoBarrierStoreSingleThread) {
  const TypeParam kVal1 = TestFillValue<TypeParam>();
  const TypeParam kVal2 = static_cast<TypeParam>(-1);

  TypeParam value;

  atomic::NoBarrier_Store(&value, kVal1);
  EXPECT_EQ(kVal1, value);
  atomic::NoBarrier_Store(&value, kVal2);
  EXPECT_EQ(kVal2, value);
}

TYPED_TEST(BasicSbAtomicTest, NoBarrierLoadSingleThread) {
  const TypeParam kVal1 = TestFillValue<TypeParam>();
  const TypeParam kVal2 = static_cast<TypeParam>(-1);

  TypeParam value;

  value = kVal1;
  EXPECT_EQ(kVal1, atomic::NoBarrier_Load(&value));
  value = kVal2;
  EXPECT_EQ(kVal2, atomic::NoBarrier_Load(&value));
}

TYPED_TEST(AdvancedSbAtomicTest, IncrementSingleThread) {
  // use a guard value to make sure the NoBarrier_AtomicIncrement doesn't go
  // outside the expected address bounds.  This is in particular to
  // test that some future change to the asm code doesn't cause the
  // 32-bit NoBarrier_AtomicIncrement doesn't do the wrong thing on 64-bit
  // machines.
  struct {
    TypeParam prev_word;
    TypeParam count;
    TypeParam next_word;
  } s;

  TypeParam prev_word_value, next_word_value;
  SbMemorySet(&prev_word_value, 0xFF, sizeof(TypeParam));
  SbMemorySet(&next_word_value, 0xEE, sizeof(TypeParam));

  s.prev_word = prev_word_value;
  s.count = 0;
  s.next_word = next_word_value;

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, 1), 1);
  EXPECT_EQ(s.count, 1);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, 2), 3);
  EXPECT_EQ(s.count, 3);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, 3), 6);
  EXPECT_EQ(s.count, 6);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, -3), 3);
  EXPECT_EQ(s.count, 3);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, -2), 1);
  EXPECT_EQ(s.count, 1);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, -1), 0);
  EXPECT_EQ(s.count, 0);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, -1), -1);
  EXPECT_EQ(s.count, -1);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, -4), -5);
  EXPECT_EQ(s.count, -5);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);

  EXPECT_EQ(atomic::NoBarrier_Increment(&s.count, 5), 0);
  EXPECT_EQ(s.count, 0);
  EXPECT_EQ(s.prev_word, prev_word_value);
  EXPECT_EQ(s.next_word, next_word_value);
}

TYPED_TEST(AdvancedSbAtomicTest, NoBarrierCompareAndSwapSingleThread) {
  TypeParam value = 0;
  TypeParam prev = atomic::NoBarrier_CompareAndSwap(&value, 0, 1);
  EXPECT_EQ(1, value);
  EXPECT_EQ(0, prev);

  const TypeParam kTestValue = GetTestValue<TypeParam>();
  value = kTestValue;
  prev = atomic::NoBarrier_CompareAndSwap(&value, 0, 5);
  EXPECT_EQ(kTestValue, value);
  EXPECT_EQ(kTestValue, prev);

  value = kTestValue;
  prev = atomic::NoBarrier_CompareAndSwap(&value, kTestValue, 5);
  EXPECT_EQ(5, value);
  EXPECT_EQ(kTestValue, prev);
}

TYPED_TEST(AdvancedSbAtomicTest, ExchangeSingleThread) {
  TypeParam value = 0;
  TypeParam new_value = atomic::NoBarrier_Exchange(&value, 1);
  EXPECT_EQ(1, value);
  EXPECT_EQ(0, new_value);

  const TypeParam kTestValue = GetTestValue<TypeParam>();
  value = kTestValue;
  new_value = atomic::NoBarrier_Exchange(&value, kTestValue);
  EXPECT_EQ(kTestValue, value);
  EXPECT_EQ(kTestValue, new_value);

  value = kTestValue;
  new_value = atomic::NoBarrier_Exchange(&value, 5);
  EXPECT_EQ(5, value);
  EXPECT_EQ(kTestValue, new_value);
}

TYPED_TEST(AdvancedSbAtomicTest, IncrementBoundsSingleThread) {
  // Test at rollover boundary between int_max and int_min
  TypeParam test_val = GetHighBitAt<TypeParam>(0);
  TypeParam value = -1 ^ test_val;
  TypeParam new_value = atomic::NoBarrier_Increment(&value, 1);
  EXPECT_EQ(test_val, value);
  EXPECT_EQ(value, new_value);

  atomic::NoBarrier_Increment(&value, -1);
  EXPECT_EQ(-1 ^ test_val, value);

  // Test at 32-bit boundary for 64-bit atomic type.
  test_val = SB_UINT64_C(1) << (Bits<TypeParam>() / 2);
  value = test_val - 1;
  new_value = atomic::NoBarrier_Increment(&value, 1);
  EXPECT_EQ(test_val, value);
  EXPECT_EQ(value, new_value);

  atomic::NoBarrier_Increment(&value, -1);
  EXPECT_EQ(test_val - 1, value);
}

TYPED_TEST(AdvancedSbAtomicTest, BarrierStoreSingleThread) {
  const TypeParam kVal1 = TestFillValue<TypeParam>();
  const TypeParam kVal2 = static_cast<TypeParam>(-1);

  TypeParam value;

  atomic::Acquire_Store(&value, kVal1);
  EXPECT_EQ(kVal1, value);
  atomic::Acquire_Store(&value, kVal2);
  EXPECT_EQ(kVal2, value);

  atomic::Release_Store(&value, kVal1);
  EXPECT_EQ(kVal1, value);
  atomic::Release_Store(&value, kVal2);
  EXPECT_EQ(kVal2, value);
}

TYPED_TEST(AdvancedSbAtomicTest, BarrierLoadSingleThread) {
  const TypeParam kVal1 = TestFillValue<TypeParam>();
  const TypeParam kVal2 = static_cast<TypeParam>(-1);

  TypeParam value;

  value = kVal1;
  EXPECT_EQ(kVal1, atomic::Acquire_Load(&value));
  value = kVal2;
  EXPECT_EQ(kVal2, atomic::Acquire_Load(&value));

  value = kVal1;
  EXPECT_EQ(kVal1, atomic::Release_Load(&value));
  value = kVal2;
  EXPECT_EQ(kVal2, atomic::Release_Load(&value));
}

// Uses |state| to control initialization of |out_data| to |data| from multiple
// threads. This function was adapted from base/lazy_instance, to ensure I got
// the barrier operations correct.
template <class SbAtomicType>
void SetData(SbAtomicType* state,
             char* out_data,
             const char* data,
             int data_size) {
  const SbAtomicType kUninitialized = 0;
  const SbAtomicType kInitializing = 1;
  const SbAtomicType kInitialized = 2;

  // Try to grab the initialization. If we're the first, will go from
  // kUninitialized to kInitializing, otherwise we've been beaten.  The memory
  // access has no memory ordering as kUninitialized and kInitializing have no
  // associated data (memory barriers are all about ordering of memory accesses
  // to *associated* data).
  if (atomic::NoBarrier_CompareAndSwap(state, kUninitialized, kInitializing) ==
      kUninitialized) {
    // We've locked the state, now we will initialize the data.
    SbMemoryCopy(out_data, data, data_size);
    // Signal the initialization has completed.
    atomic::Release_Store(state, kInitialized);
    return;
  }

  // It's either in the process of initializing, or already initialized. Spin.
  // The load has acquire memory ordering as a thread which sees state_ ==
  // kInitialized needs to acquire visibility over the associated data
  // (out_data). The paired Release_Store is above in the initialization
  // clause.
  while (atomic::Acquire_Load(state) == kInitializing) {
    SbThreadYield();
  }
}

// A struct to pass through all the thread parameters.
template <class SbAtomicType>
struct TestOnceContext {
  const char* data;
  char* out_data;
  SbAtomicType* state;
  int size;
};

// The entry point for all the threads spawned during TestOnce().
template <class SbAtomicType>
void* TestOnceEntryPoint(void* raw_context) {
  // Force every thread to sleep immediately so the first thread doesn't always
  // just win.
  SbThreadSleep(kSbTimeMillisecond);
  TestOnceContext<SbAtomicType>* context =
      reinterpret_cast<TestOnceContext<SbAtomicType>*>(raw_context);
  SetData(context->state, context->out_data, context->data, context->size);
  return NULL;
}

// Tests a "once"-like functionality implemented with Atomics. This should
// effectively test multi-threaded CompareAndSwap, Release_Store and
// Acquire_Load.
TYPED_TEST(AdvancedSbAtomicTest, OnceMultipleThreads) {
  const int kNumTrials = 25;
  const int kNumThreads = 12;
  // Ensure each thread has slightly different data by offsetting it a bit as
  // the 256-value pattern repeats.
  const int kDataPerThread = 65534;
  const int kTotalData = kDataPerThread * kNumThreads;

  for (int trial = 0; trial < kNumTrials; ++trial) {
    // The control variable to use atomics to synchronize on.
    TypeParam state = 0;

    // The target buffer to store the data.
    char* target_data = new char[kDataPerThread];
    SbMemorySet(target_data, 0, kDataPerThread);

    // Each thread has a different set of data that it will try to set.
    char* data = new char[kTotalData];
    for (int i = 0; i < kTotalData; ++i) {
      data[i] = static_cast<char>(i & 0xFF);
    }

    // Start up kNumThreads to fight over initializing |target_data|.
    TestOnceContext<TypeParam> contexts[kNumThreads] = {0};
    SbThread threads[kNumThreads] = {0};
    for (int i = 0; i < kNumThreads; ++i) {
      contexts[i].data = data + i * kDataPerThread;
      contexts[i].out_data = target_data;
      contexts[i].state = &state;
      contexts[i].size = kDataPerThread;
      threads[i] = SbThreadCreate(
          0, kSbThreadNoPriority, kSbThreadNoAffinity, true, "TestOnceThread",
          TestOnceEntryPoint<TypeParam>, &(contexts[i]));
      EXPECT_TRUE(SbThreadIsValid(threads[i]));
    }

    // Wait for all threads to complete, and clean up their resources.
    for (int i = 0; i < kNumThreads; ++i) {
      EXPECT_TRUE(SbThreadJoin(threads[i], NULL));
    }

    // Ensure that exactly one thread initialized the data.
    bool match = false;
    for (int i = 0; i < kNumThreads; ++i) {
      if (SbMemoryCompare(target_data, data + i * kDataPerThread,
                          kDataPerThread) == 0) {
        match = true;
        break;
      }
    }

    EXPECT_TRUE(match) << "trial " << trial;
    delete[] data;
    delete[] target_data;
  }
}

const int kNumIncrements = 4000;

template <class SbAtomicType>
void* IncrementEntryPoint(void* raw_context) {
  SbAtomicType* target = reinterpret_cast<SbAtomicType*>(raw_context);
  for (int i = 0; i < kNumIncrements; ++i) {
    atomic::NoBarrier_Increment(target, 1);
  }
  return NULL;
}

template <class SbAtomicType>
void* DecrementEntryPoint(void* raw_context) {
  SbAtomicType* target = reinterpret_cast<SbAtomicType*>(raw_context);
  for (int i = 0; i < kNumIncrements; ++i) {
    atomic::NoBarrier_Increment(target, -1);
  }
  return NULL;
}

TYPED_TEST(AdvancedSbAtomicTest, IncrementDecrementMultipleThreads) {
  const int kNumTrials = 15;
  const int kNumThreads = 12;
  SB_COMPILE_ASSERT(kNumThreads % 2 == 0, kNumThreads_is_even);

  const TypeParam kTestValue = GetTestValue<TypeParam>();
  for (int trial = 0; trial < kNumTrials; ++trial) {
    // The control variable to increment and decrement.
    TypeParam value = kTestValue;

    // Start up kNumThreads to fight.
    SbThread threads[kNumThreads] = {0};

    // First half are incrementers.
    for (int i = 0; i < kNumThreads / 2; ++i) {
      threads[i] = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                  true, "TestIncrementThread",
                                  IncrementEntryPoint<TypeParam>, &value);
      EXPECT_TRUE(SbThreadIsValid(threads[i]));
    }

    // Second half are decrementers.
    for (int i = kNumThreads / 2; i < kNumThreads; ++i) {
      threads[i] = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                  true, "TestDecrementThread",
                                  DecrementEntryPoint<TypeParam>, &value);
      EXPECT_TRUE(SbThreadIsValid(threads[i]));
    }

    // Wait for all threads to complete, and clean up their resources.
    for (int i = 0; i < kNumThreads; ++i) {
      EXPECT_TRUE(SbThreadJoin(threads[i], NULL));
    }

    // |value| should be back to its original value. If the increment/decrement
    // isn't atomic, then the value should drift as the load + increment + store
    // operations collide in funny ways.
    EXPECT_EQ(kTestValue, value) << "trial " << trial;
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
