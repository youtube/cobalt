/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "nb/atomic.h"

#include <algorithm>
#include <vector>

#include "nb/test_thread.h"
#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

///////////////////////////////////////////////////////////////////////////////
// Boilerplate for test setup.
///////////////////////////////////////////////////////////////////////////////

// Defines a typelist for all atomic types.
typedef ::testing::Types<atomic_int32_t, atomic_int64_t,
                         atomic_float, atomic_double,
                         atomic_bool,
                         atomic_pointer<int*> > AllAtomicTypes;

// Defines a typelist for just atomic number types.
typedef ::testing::Types<atomic_int32_t, atomic_int64_t,
                         atomic_float, atomic_double> AtomicNumberTypes;

// Defines test type that will be instantiated using each type in
// AllAtomicTypes type list.
template <typename T>
class AtomicTest : public ::testing::Test {};
TYPED_TEST_CASE(AtomicTest, AllAtomicTypes);  // Registration.

// Defines test type that will be instantiated using each type in
// AtomicNumberTypes type list.
template <typename T>
class AtomicNumberTest : public ::testing::Test {};
TYPED_TEST_CASE(AtomicNumberTest, AtomicNumberTypes);  // Registration.


///////////////////////////////////////////////////////////////////////////////
// Singlethreaded tests.
///////////////////////////////////////////////////////////////////////////////

// Tests default constructor and single-argument constructor.
TYPED_TEST(AtomicTest, Constructors) {
  typedef TypeParam AtomicT;
  typedef typename AtomicT::ValueType T;

  const T zero(0);
  const T one = zero + 1;  // Allows AtomicPointer<T*>.

  AtomicT atomic_default;

  // Tests that default value is zero.
  ASSERT_EQ(atomic_default.load(), zero);
  AtomicT atomic_val(one);
  ASSERT_EQ(one, atomic_val.load());
}

// Tests load() and exchange().
TYPED_TEST(AtomicTest, Load_Exchange_SingleThread) {
  typedef TypeParam AtomicT;
  typedef typename AtomicT::ValueType T;

  const T zero(0);
  const T one = zero + 1;  // Allows AtomicPointer<T*>.

  AtomicT atomic;
  ASSERT_EQ(atomic.load(), zero);      // Default is 0.
  ASSERT_EQ(zero, atomic.exchange(one));  // Old value was 0.

  // Tests that AtomicType has  const get function.
  const AtomicT& const_atomic = atomic;
  ASSERT_EQ(one, const_atomic.load());
}

// Tests compare_exchange_strong().
TYPED_TEST(AtomicNumberTest, CompareExchangeStrong_SingleThread) {
  typedef TypeParam AtomicT;
  typedef typename AtomicT::ValueType T;

  const T zero(0);
  const T one = zero + 1;  // Allows AtomicPointer<T*>.

  AtomicT atomic;
  ASSERT_EQ(atomic.load(), zero); // Default is 0.
  T expected_value = zero;
  // Should succeed.
  ASSERT_TRUE(atomic.compare_exchange_strong(&expected_value,
                                             one));          // New value.

  ASSERT_EQ(zero, expected_value);
  ASSERT_EQ(one, atomic.load());  // Expect that value was set.

  expected_value = zero;
  // Asserts that when the expected and actual value is mismatched that the
  // compare_exchange_strong() fails.
  ASSERT_FALSE(atomic.compare_exchange_strong(&expected_value,  // Mismatched.
                                              zero));           // New value.

  // Failed and this means that expected_value should be set to what the
  // internal value was. In this case, one.
  ASSERT_EQ(expected_value, one);
  ASSERT_EQ(one, atomic.load());

  ASSERT_TRUE(atomic.compare_exchange_strong(&expected_value, // Matches.
                                             zero));
  ASSERT_EQ(expected_value, one);
}

// Tests atomic fetching and adding.
TYPED_TEST(AtomicNumberTest, FetchAdd_SingleThread) {
  typedef TypeParam AtomicT;
  typedef typename AtomicT::ValueType T;

  const T zero(0);
  const T one = zero + 1;  // Allows atomic_pointer<T*>.
  const T two = zero + 2;

  AtomicT atomic;
  ASSERT_EQ(atomic.load(), zero);         // Default is 0.
  ASSERT_EQ(zero, atomic.fetch_add(one)); // Prev value was 0.
  ASSERT_EQ(one, atomic.load());          // Now value is this.
  ASSERT_EQ(one, atomic.fetch_add(one));  // Prev value was 1.
  ASSERT_EQ(two, atomic.exchange(one));   // Old value was 2.
}

// Tests atomic fetching and subtracting.
TYPED_TEST(AtomicNumberTest, FetchSub_SingleThread) {
  typedef TypeParam AtomicT;
  typedef typename AtomicT::ValueType T;

  const T zero(0);
  const T one = zero + 1;  // Allows AtomicPointer<T*>.
  const T two = zero + 2;
  const T neg_two(zero-2);

  AtomicT atomic;
  ASSERT_EQ(atomic.load(), zero);            // Default is 0.
  atomic.exchange(two);
  ASSERT_EQ(two, atomic.fetch_sub(one));     // Prev value was 2.
  ASSERT_EQ(one, atomic.load());             // New value.
  ASSERT_EQ(one, atomic.fetch_sub(one));     // Prev value was one.
  ASSERT_EQ(zero, atomic.load());            // New 0.
  ASSERT_EQ(zero, atomic.fetch_sub(two));
  ASSERT_EQ(neg_two, atomic.load());         // 0-2 = -2
}

///////////////////////////////////////////////////////////////////////////////
// Multithreaded tests.
///////////////////////////////////////////////////////////////////////////////

// A thread that will execute compare_exhange_strong() and write out a result
// to a shared output.
template <typename AtomicT>
class CompareExchangeThread : public TestThread {
 public:
  typedef typename AtomicT::ValueType T;
  CompareExchangeThread(int start_num,
                        int end_num,
                        AtomicT* atomic_value,
                        std::vector<T>* output,
                        starboard::Mutex* output_mutex)
      : start_num_(start_num), end_num_(end_num),
        atomic_value_(atomic_value), output_(output),
        output_mutex_(output_mutex) {
  }

  virtual void Run() {
    std::vector<T> output_buffer;
    output_buffer.reserve(end_num_ - start_num_);
    for (int i = start_num_; i < end_num_; ++i) {
      T new_value = T(i);
      while (true) {
        if (std::rand() % 3 == 0) {
          // 1 in 3 chance of yeilding.
          // Attempt to cause more contention by giving other threads a chance
          // to run.
          SbThreadYield();
        }

        const T prev_value = atomic_value_->load();
        T expected_value = prev_value;
        const bool success =
            atomic_value_->compare_exchange_strong(&expected_value,
                                                   new_value);
        if (success) {
          output_buffer.push_back(prev_value);
          break;
        }
      }
    }

    // Lock the output to append this output buffer.
    starboard::ScopedLock lock(*output_mutex_);
    output_->insert(output_->end(),
                    output_buffer.begin(),
                    output_buffer.end());
  }
 private:
  const int start_num_;
  const int end_num_;
  AtomicT*const atomic_value_;
  std::vector<T>*const output_;
  starboard::Mutex*const output_mutex_;
};

// Tests Atomic<T>::compare_exchange_strong(). Each thread has a unique
// sequential range [0,1,2,3 ... ), [5,6,8, ...) that it will generate.
// The numbers are sequentially written to the shared Atomic type and then
// exposed to other threads:
//
//    Generates [0,1,2,...,n/2)
//   +------+ Thread A <--------+        (Write Exchanged Value)
//   |                          |
//   |    compare_exchange()    +---> exchanged? ---+
//   +----> +------------+ +----+                   v
//          | AtomicType |                   Output vector
//   +----> +------------+ +----+                   ^
//   |    compare_exchange()    +---> exchanged? ---+
//   |                          |
//   +------+ Thread B <--------+
//    Generates [n/2,n/2+1,...,n)
//
// By repeatedly calling atomic<T>::compare_exchange_strong() by each of the
// threads, each will see the previous value of the shared variable when their
// exchange (atomic swap) operation is successful. If all of the swapped out
// values are recombined then it will form the original generated sequence from
// all threads.
//
//            TEST PHASE
//  sort( output vector ) AND TEST THAT
//  output vector Contains [0,1,2,...,n)
//
// The test passes when the output array is tested that it contains every
// expected generated number from all threads. If compare_exchange_strong() is
// written incorrectly for an atomic type then the output array will have
// duplicates or otherwise not be equal to the expected natural number set.
TYPED_TEST(AtomicNumberTest, Test_CompareExchange_MultiThreaded) {
  typedef TypeParam AtomicT;
  typedef typename AtomicT::ValueType T;

  static const int kNumElements = 1000;
  static const int kNumThreads = 4;

  AtomicT atomic_value(T(-1));
  std::vector<TestThread*> threads;
  std::vector<T> output_values;
  starboard::Mutex output_mutex;

  for (int i = 0; i < kNumThreads; ++i) {
    const int start_num = (kNumElements * i) / kNumThreads;
    const int end_num = (kNumElements * (i + 1)) / kNumThreads;
    threads.push_back(
        new CompareExchangeThread<AtomicT>(
            start_num,  // defines the number range to generate.
            end_num,
            &atomic_value,
            &output_values,
            &output_mutex));
  }

  // These threads will generate unique numbers in their range and then
  // write them to the output array.
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i]->Start();
  }

  for (int i = 0; i < kNumThreads; ++i) {
    threads[i]->Join();
  }
  // Cleanup threads.
  for (int i = 0; i < kNumThreads; ++i) {
    delete threads[i];
  }
  threads.clear();

  // Final value needs to be written out. The last thread to join doesn't
  // know it's the last and therefore the final value in the shared
  // has not be pushed to the output array.
  output_values.push_back(atomic_value.load());
  std::sort(output_values.begin(), output_values.end());

  // We expect the -1 value because it was the explicit initial value of the
  // shared atomic.
  ASSERT_EQ(T(-1), output_values[0]);
  ASSERT_EQ(T(0), output_values[1]);
  output_values.erase(output_values.begin());  // Chop off the -1 at the front.

  // Finally, assert that the output array is equal to the natural numbers
  // after it has been sorted.
  ASSERT_EQ(output_values.size(), kNumElements);
  // All of the elements should be equal too.
  for (int i = 0; i < output_values.size(); ++i) {
    ASSERT_EQ(output_values[i], T(i));
  }
}

}  // namespace
}  // namespace nb
