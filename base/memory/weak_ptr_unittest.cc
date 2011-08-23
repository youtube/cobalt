// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"

namespace base {
namespace {

template <class T>
class OffThreadObjectCreator {
 public:
  static T* NewObject() {
    T* result;
    {
      Thread creator_thread("creator_thread");
      creator_thread.Start();
      creator_thread.message_loop()->PostTask(
          FROM_HERE,
          NewRunnableFunction(OffThreadObjectCreator::CreateObject, &result));
    }
    DCHECK(result);  // We synchronized on thread destruction above.
    return result;
  }
 private:
  static void CreateObject(T** result) {
    *result = new T;
  }
};

struct Base {};
struct Derived : Base {};

struct Producer : SupportsWeakPtr<Producer> {};
struct Consumer { WeakPtr<Producer> producer; };

// Helper class to create and destroy weak pointer copies
// and delete objects on a background thread.
class BackgroundThread : public Thread {
 public:
  BackgroundThread()
      : Thread("owner_thread") {
  }

  void CreateConsumerFromProducer(Consumer** consumer, Producer* producer) {
    WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&BackgroundThread::DoCreateFromProducer,
                            consumer,
                            producer,
                            &completion));
    completion.Wait();
  }

  void CreateConsumerFromConsumer(Consumer** consumer, const Consumer* other) {
    WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&BackgroundThread::DoCreateFromConsumer,
                            consumer,
                            other,
                            &completion));
    completion.Wait();
  }

  void DeleteProducer(Producer* object) {
    WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&BackgroundThread::DoDeleteProducer,
                            object,
                            &completion));
    completion.Wait();
  }

  void DeleteConsumer(Consumer* object) {
    WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&BackgroundThread::DoDeleteConsumer,
                            object,
                            &completion));
    completion.Wait();
  }

  Producer* DeRef(const Consumer* consumer) {
    WaitableEvent completion(true, false);
    Producer* result = NULL;
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&BackgroundThread::DoDeRef,
                            consumer,
                            &result,
                            &completion));
    completion.Wait();
    return result;
  }

 protected:
  static void DoCreateFromConsumer(Consumer** consumer,
                                   const Consumer* other,
                                   WaitableEvent* completion) {
    *consumer = new Consumer;
    **consumer = *other;
    completion->Signal();
  }

  static void DoCreateFromProducer(Consumer** consumer,
                                   Producer* producer,
                                   WaitableEvent* completion) {
    *consumer = new Consumer;
    (*consumer)->producer = producer->AsWeakPtr();
    completion->Signal();
  }

  static void DoDeRef(const Consumer* consumer,
                      Producer** result,
                      WaitableEvent* completion) {
    *result = consumer->producer.get();
    completion->Signal();
  }

  static void DoDeleteProducer(Producer* object, WaitableEvent* completion) {
    delete object;
    completion->Signal();
  }

  static void DoDeleteConsumer(Consumer* object, WaitableEvent* completion) {
    delete object;
    completion->Signal();
  }
};

}  // namespace

TEST(WeakPtrTest, Basic) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
}

TEST(WeakPtrTest, Comparison) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  WeakPtr<int> ptr2 = ptr;
  EXPECT_TRUE(ptr == ptr2);
}

TEST(WeakPtrTest, OutOfScope) {
  WeakPtr<int> ptr;
  EXPECT_TRUE(ptr.get() == NULL);
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    ptr = factory.GetWeakPtr();
  }
  EXPECT_TRUE(ptr.get() == NULL);
}

TEST(WeakPtrTest, Multiple) {
  WeakPtr<int> a, b;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    b = factory.GetWeakPtr();
    EXPECT_EQ(&data, a.get());
    EXPECT_EQ(&data, b.get());
  }
  EXPECT_TRUE(a.get() == NULL);
  EXPECT_TRUE(b.get() == NULL);
}

TEST(WeakPtrTest, MultipleStaged) {
  WeakPtr<int> a;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    {
      WeakPtr<int> b = factory.GetWeakPtr();
    }
    EXPECT_TRUE(a.get() != NULL);
  }
  EXPECT_TRUE(a.get() == NULL);
}

TEST(WeakPtrTest, UpCast) {
  Derived data;
  WeakPtrFactory<Derived> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  ptr = factory.GetWeakPtr();
  EXPECT_EQ(ptr.get(), &data);
}

TEST(WeakPtrTest, SupportsWeakPtr) {
  Producer f;
  WeakPtr<Producer> ptr = f.AsWeakPtr();
  EXPECT_EQ(&f, ptr.get());
}

TEST(WeakPtrTest, InvalidateWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrs();
  EXPECT_TRUE(ptr.get() == NULL);
  EXPECT_FALSE(factory.HasWeakPtrs());
}

TEST(WeakPtrTest, HasWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  {
    WeakPtr<int> ptr = factory.GetWeakPtr();
    EXPECT_TRUE(factory.HasWeakPtrs());
  }
  EXPECT_FALSE(factory.HasWeakPtrs());
}

TEST(WeakPtrTest, SingleThreaded1) {
  // Test that it is OK to create a class that supports weak references on one
  // thread, but use it on another.  This tests that we do not trip runtime
  // checks that ensure that a weak reference is not used by multiple threads.
  scoped_ptr<Producer> producer(OffThreadObjectCreator<Producer>::NewObject());
  WeakPtr<Producer> weak_producer = producer->AsWeakPtr();
  EXPECT_EQ(producer.get(), weak_producer.get());
}

TEST(WeakPtrTest, SingleThreaded2) {
  // Test that it is OK to create a class that has a WeakPtr member on one
  // thread, but use it on another.  This tests that we do not trip runtime
  // checks that ensure that a weak reference is not used by multiple threads.
  scoped_ptr<Consumer> consumer(OffThreadObjectCreator<Consumer>::NewObject());
  Producer producer;
  consumer->producer = producer.AsWeakPtr();
  EXPECT_EQ(&producer, consumer->producer.get());
}

TEST(WeakPtrTest, MoveOwnershipImplicit) {
  // Move object ownership to other thread by releasing all weak pointers
  // on the original thread first. Establishing weak pointers on a different
  // thread after previous pointers have been destroyed implicitly reattaches
  // the thread checks.
  // - Thread A creates object and weak pointer
  // - Thread A deletes the weak pointer
  // - Thread B creates weak pointer
  // - Thread B derefs weak pointer
  // - Thread B deletes object
  BackgroundThread thread;
  thread.Start();
  Producer* producer = new Producer();
  {
    WeakPtr<Producer> weak_ptr = producer->AsWeakPtr();
  }
  Consumer* consumer;
  thread.CreateConsumerFromProducer(&consumer, producer);
  EXPECT_EQ(thread.DeRef(consumer), producer);
  thread.DeleteProducer(producer);
  thread.DeleteConsumer(consumer);
}

TEST(WeakPtrTest, MoveOwnershipExplicit) {
  // Test that we do not trip any checks if we establish weak references
  // on one thread and delete the object on another thread after explicit
  // detachment.
  // - Thread A creates object
  // - Thread B creates weak pointer
  // - Thread B releases weak pointer
  // - Detach owner from Thread B
  // - Thread A destroys object
  BackgroundThread thread;
  thread.Start();
  Producer producer;
  Consumer* consumer;
  thread.CreateConsumerFromProducer(&consumer, &producer);
  EXPECT_EQ(thread.DeRef(consumer), &producer);
  thread.DeleteConsumer(consumer);
  producer.DetachFromThread();
}

TEST(WeakPtrTest, ThreadARefOutlivesThreadBRef) {
  // Originating thread has a WeakPtr that outlives others.
  // - Thread A creates WeakPtr<> and passes copy to Thread B
  // - Destruct the pointer on Thread B
  // - Destruct the pointer on Thread A
  BackgroundThread thread;
  thread.Start();
  Producer producer;
  Consumer consumer;
  consumer.producer = producer.AsWeakPtr();
  Consumer* consumer_copy;
  thread.CreateConsumerFromConsumer(&consumer_copy, &consumer);
  EXPECT_EQ(consumer_copy->producer, &producer);
  thread.DeleteConsumer(consumer_copy);
}

TEST(WeakPtrTest, ThreadBRefOutlivesThreadARef) {
  // Originating thread drops all references before another thread.
  // - Thread A creates WeakPtr<> and passes copy to Thread B
  // - Destruct the pointer on Thread A
  // - Destruct the pointer on Thread B
  BackgroundThread thread;
  thread.Start();
  Producer producer;
  Consumer* consumer_copy;
  {
    Consumer consumer;
    consumer.producer = producer.AsWeakPtr();
    thread.CreateConsumerFromConsumer(&consumer_copy, &consumer);
  }
  EXPECT_EQ(consumer_copy->producer, &producer);
  thread.DeleteConsumer(consumer_copy);
}

TEST(WeakPtrTest, OwnerThreadDeletesObject) {
  // Originating thread invalidates WeakPtrs while its held by other thread.
  // - Thread A creates WeakPtr<> and passes Copy to Thread B
  // - WeakReferenceOwner gets destroyed on Thread A
  // - WeakPtr gets destroyed on Thread B
  BackgroundThread thread;
  thread.Start();
  Consumer* consumer_copy;
  {
    Producer producer;
    Consumer consumer;
    consumer.producer = producer.AsWeakPtr();
    thread.CreateConsumerFromConsumer(&consumer_copy, &consumer);
  }
  EXPECT_TRUE(consumer_copy->producer == NULL);
  thread.DeleteConsumer(consumer_copy);
}

}  // namespace base
