// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/file_util.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

COMPILE_ASSERT(DefaultSingletonTraits<int>::kRegisterAtExit == true, a);

typedef void (*CallbackFunc)();

class IntSingleton {
 public:
  static IntSingleton* GetInstance() {
    return Singleton<IntSingleton>::get();
  }

  int value_;
};

class Init5Singleton {
 public:
  struct Trait;

  static Init5Singleton* GetInstance() {
    return Singleton<Init5Singleton, Trait>::get();
  }

  int value_;
};

struct Init5Singleton::Trait : public DefaultSingletonTraits<Init5Singleton> {
  static Init5Singleton* New() {
    Init5Singleton* instance = new Init5Singleton();
    instance->value_ = 5;
    return instance;
  }
};

int* SingletonInt() {
  return &IntSingleton::GetInstance()->value_;
}

int* SingletonInt5() {
  return &Init5Singleton::GetInstance()->value_;
}

template <typename Type>
struct CallbackTrait : public DefaultSingletonTraits<Type> {
  static void Delete(Type* instance) {
    if (instance->callback_)
      (instance->callback_)();
    DefaultSingletonTraits<Type>::Delete(instance);
  }
};

class CallbackSingleton {
 public:
  CallbackSingleton() : callback_(NULL) { }
  CallbackFunc callback_;
};

class CallbackSingletonWithNoLeakTrait : public CallbackSingleton {
 public:
  struct Trait : public CallbackTrait<CallbackSingletonWithNoLeakTrait> { };

  CallbackSingletonWithNoLeakTrait() : CallbackSingleton() { }

  static CallbackSingletonWithNoLeakTrait* GetInstance() {
    return Singleton<CallbackSingletonWithNoLeakTrait, Trait>::get();
  }
};

class CallbackSingletonWithLeakTrait : public CallbackSingleton {
 public:
  struct Trait : public CallbackTrait<CallbackSingletonWithLeakTrait> {
    static const bool kRegisterAtExit = false;
  };

  CallbackSingletonWithLeakTrait() : CallbackSingleton() { }

  static CallbackSingletonWithLeakTrait* GetInstance() {
    return Singleton<CallbackSingletonWithLeakTrait, Trait>::get();
  }
};

class CallbackSingletonWithStaticTrait : public CallbackSingleton {
 public:
  struct Trait;

  CallbackSingletonWithStaticTrait() : CallbackSingleton() { }

  static CallbackSingletonWithStaticTrait* GetInstance() {
    return Singleton<CallbackSingletonWithStaticTrait, Trait>::get();
  }
};

struct CallbackSingletonWithStaticTrait::Trait
    : public StaticMemorySingletonTraits<CallbackSingletonWithStaticTrait> {
  static void Delete(CallbackSingletonWithStaticTrait* instance) {
    if (instance->callback_)
      (instance->callback_)();
    StaticMemorySingletonTraits<CallbackSingletonWithStaticTrait>::Delete(
        instance);
  }
};


void SingletonNoLeak(CallbackFunc CallOnQuit) {
  CallbackSingletonWithNoLeakTrait::GetInstance()->callback_ = CallOnQuit;
}

void SingletonLeak(CallbackFunc CallOnQuit) {
  CallbackSingletonWithLeakTrait::GetInstance()->callback_ = CallOnQuit;
}

CallbackFunc* GetLeakySingleton() {
  return &CallbackSingletonWithLeakTrait::GetInstance()->callback_;
}

void DeleteLeakySingleton() {
  DefaultSingletonTraits<CallbackSingletonWithLeakTrait>::Delete(
      CallbackSingletonWithLeakTrait::GetInstance());
}

void SingletonStatic(CallbackFunc CallOnQuit) {
  CallbackSingletonWithStaticTrait::GetInstance()->callback_ = CallOnQuit;
}

CallbackFunc* GetStaticSingleton() {
  return &CallbackSingletonWithStaticTrait::GetInstance()->callback_;
}

void ResurrectStaticSingleton() {
}

}  // namespace

class SingletonTest : public testing::Test {
 public:
  SingletonTest() { }

  virtual void SetUp() {
    non_leak_called_ = false;
    leaky_called_ = false;
    static_called_ = false;
  }

 protected:
  void VerifiesCallbacks() {
    EXPECT_TRUE(non_leak_called_);
    EXPECT_FALSE(leaky_called_);
    EXPECT_TRUE(static_called_);
    non_leak_called_ = false;
    leaky_called_ = false;
    static_called_ = false;
  }

  void VerifiesCallbacksNotCalled() {
    EXPECT_FALSE(non_leak_called_);
    EXPECT_FALSE(leaky_called_);
    EXPECT_FALSE(static_called_);
    non_leak_called_ = false;
    leaky_called_ = false;
    static_called_ = false;
  }

  static void CallbackNoLeak() {
    non_leak_called_ = true;
  }

  static void CallbackLeak() {
    leaky_called_ = true;
  }

  static void CallbackStatic() {
    static_called_ = true;
  }

 private:
  static bool non_leak_called_;
  static bool leaky_called_;
  static bool static_called_;
};

bool SingletonTest::non_leak_called_ = false;
bool SingletonTest::leaky_called_ = false;
bool SingletonTest::static_called_ = false;

TEST_F(SingletonTest, Basic) {
  int* singleton_int;
  int* singleton_int_5;
  CallbackFunc* leaky_singleton;
  CallbackFunc* static_singleton;

  {
    base::ShadowingAtExitManager sem;
    {
      singleton_int = SingletonInt();
    }
    // Ensure POD type initialization.
    EXPECT_EQ(*singleton_int, 0);
    *singleton_int = 1;

    EXPECT_EQ(singleton_int, SingletonInt());
    EXPECT_EQ(*singleton_int, 1);

    {
      singleton_int_5 = SingletonInt5();
    }
    // Is default initialized to 5.
    EXPECT_EQ(*singleton_int_5, 5);

    SingletonNoLeak(&CallbackNoLeak);
    SingletonLeak(&CallbackLeak);
    SingletonStatic(&CallbackStatic);
    static_singleton = GetStaticSingleton();
    leaky_singleton = GetLeakySingleton();
    EXPECT_TRUE(leaky_singleton);
  }

  // Verify that only the expected callback has been called.
  VerifiesCallbacks();
  // Delete the leaky singleton. It is interesting to note that Purify does
  // *not* detect the leak when this call is commented out. :(
  DeleteLeakySingleton();

  // The static singleton can't be acquired post-atexit.
  EXPECT_EQ(NULL, GetStaticSingleton());

  {
    base::ShadowingAtExitManager sem;
    // Verifiy that the variables were reset.
    {
      singleton_int = SingletonInt();
      EXPECT_EQ(*singleton_int, 0);
    }
    {
      singleton_int_5 = SingletonInt5();
      EXPECT_EQ(*singleton_int_5, 5);
    }
    {
      // Resurrect the static singleton, and assert that it
      // still points to the same (static) memory.
      CallbackSingletonWithStaticTrait::Trait::Resurrect();
      EXPECT_EQ(GetStaticSingleton(), static_singleton);
    }
  }
  // The leaky singleton shouldn't leak since SingletonLeak has not been called.
  VerifiesCallbacksNotCalled();
}
