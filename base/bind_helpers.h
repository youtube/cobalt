// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines a set of argument wrappers and related factory methods that
// can be used specify the refcounting and reference semantics of arguments
// that are bound by the Bind() function in base/bind.h.
//
// The public functions are base::Unretained(), base::Owned(),
// base::ConstRef(), and base::IgnoreReturn().
//
// Unretained() allows Bind() to bind a non-refcounted class, and to disable
// refcounting on arguments that are refcounted objects.
// Owned() transfers ownership of an object to the Callback resulting from
// bind; the object will be deleted when the Callback is deleted.
// ConstRef() allows binding a constant reference to an argument rather
// than a copy.
// IgnoreReturn() is used to adapt a 0-argument Callback with a return type to
// a Closure. This is useful if you need to PostTask with a function that has
// a return value that you don't care about.
//
//
// EXAMPLE OF Unretained():
//
//   class Foo {
//    public:
//     void func() { cout << "Foo:f" << endl; }
//   };
//
//   // In some function somewhere.
//   Foo foo;
//   Closure foo_callback =
//       Bind(&Foo::func, Unretained(&foo));
//   foo_callback.Run();  // Prints "Foo:f".
//
// Without the Unretained() wrapper on |&foo|, the above call would fail
// to compile because Foo does not support the AddRef() and Release() methods.
//
//
// EXAMPLE OF Owned():
//
//   void foo(int* arg) { cout << *arg << endl }
//
//   int* pn = new int(1);
//   Closure foo_callback = Bind(&foo, Owned(pn));
//
//   foo_callback.Run();  // Prints "1"
//   foo_callback.Run();  // Prints "1"
//   *n = 2;
//   foo_callback.Run();  // Prints "2"
//
//   foo_callback.Reset();  // |pn| is deleted.  Also will happen when
//                          // |foo_callback| goes out of scope.
//
// Without Owned(), someone would have to know to delete |pn| when the last
// reference to the Callback is deleted.
//
//
// EXAMPLE OF ConstRef():
//
//   void foo(int arg) { cout << arg << endl }
//
//   int n = 1;
//   Closure no_ref = Bind(&foo, n);
//   Closure has_ref = Bind(&foo, ConstRef(n));
//
//   no_ref.Run();  // Prints "1"
//   has_ref.Run();  // Prints "1"
//
//   n = 2;
//   no_ref.Run();  // Prints "1"
//   has_ref.Run();  // Prints "2"
//
// Note that because ConstRef() takes a reference on |n|, |n| must outlive all
// its bound callbacks.
//
//
// EXAMPLE OF IgnoreReturn():
//
//   int DoSomething(int arg) { cout << arg << endl; }
//   Callback<int(void)> cb = Bind(&DoSomething, 1);
//   Closure c = IgnoreReturn(cb);  // Prints "1"
//       or
//   ml->PostTask(FROM_HERE, IgnoreReturn(cb));  // Prints "1" on |ml|

#ifndef BASE_BIND_HELPERS_H_
#define BASE_BIND_HELPERS_H_
#pragma once

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/template_util.h"

namespace base {
namespace internal {

// Use the Substitution Failure Is Not An Error (SFINAE) trick to inspect T
// for the existence of AddRef() and Release() functions of the correct
// signature.
//
// http://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error
// http://stackoverflow.com/questions/257288/is-it-possible-to-write-a-c-template-to-check-for-a-functions-existence
// http://stackoverflow.com/questions/4358584/sfinae-approach-comparison
// http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
//
// The last link in particular show the method used below.
//
// For SFINAE to work with inherited methods, we need to pull some extra tricks
// with multiple inheritance.  In the more standard formulation, the overloads
// of Check would be:
//
//   template <typename C>
//   Yes NotTheCheckWeWant(Helper<&C::TargetFunc>*);
//
//   template <typename C>
//   No NotTheCheckWeWant(...);
//
//   static const bool value = sizeof(NotTheCheckWeWant<T>(0)) == sizeof(Yes);
//
// The problem here is that template resolution will not match
// C::TargetFunc if TargetFunc does not exist directly in C.  That is, if
// TargetFunc in inherited from an ancestor, &C::TargetFunc will not match,
// |value| will be false.  This formulation only checks for whether or
// not TargetFunc exist directly in the class being introspected.
//
// To get around this, we play a dirty trick with multiple inheritance.
// First, We create a class BaseMixin that declares each function that we
// want to probe for.  Then we create a class Base that inherits from both T
// (the class we wish to probe) and BaseMixin.  Note that the function
// signature in BaseMixin does not need to match the signature of the function
// we are probing for; thus it's easiest to just use void(void).
//
// Now, if TargetFunc exists somewhere in T, then &Base::TargetFunc has an
// ambiguous resolution between BaseMixin and T.  This lets us write the
// following:
//
//   template <typename C>
//   No GoodCheck(Helper<&C::TargetFunc>*);
//
//   template <typename C>
//   Yes GoodCheck(...);
//
//   static const bool value = sizeof(GoodCheck<Base>(0)) == sizeof(Yes);
//
// Notice here that the variadic version of GoodCheck() returns Yes here
// instead of No like the previous one. Also notice that we calculate |value|
// by specializing GoodCheck() on Base instead of T.
//
// We've reversed the roles of the variadic, and Helper overloads.
// GoodCheck(Helper<&C::TargetFunc>*), when C = Base, fails to be a valid
// substitution if T::TargetFunc exists. Thus GoodCheck<Base>(0) will resolve
// to the variadic version if T has TargetFunc.  If T::TargetFunc does not
// exist, then &C::TargetFunc is not ambiguous, and the overload resolution
// will prefer GoodCheck(Helper<&C::TargetFunc>*).
//
// This method of SFINAE will correctly probe for inherited names, but it cannot
// typecheck those names.  It's still a good enough sanity check though.
//
// Works on gcc-4.2, gcc-4.4, and Visual Studio 2008.
//
// TODO(ajwong): Move to ref_counted.h or template_util.h when we've vetted
// this works well.
//
// TODO(ajwong): Make this check for Release() as well.
// See http://crbug.com/82038.
template <typename T>
class SupportsAddRefAndRelease {
  typedef char Yes[1];
  typedef char No[2];

  struct BaseMixin {
    void AddRef();
  };

// MSVC warns when you try to use Base if T has a private destructor, the
// common pattern for refcounted types. It does this even though no attempt to
// instantiate Base is made.  We disable the warning for this definition.
#if defined(OS_WIN)
#pragma warning(disable:4624)
#endif
  struct Base : public T, public BaseMixin {
  };
#if defined(OS_WIN)
#pragma warning(default:4624)
#endif

  template <void(BaseMixin::*)(void)> struct Helper {};

  template <typename C>
  static No& Check(Helper<&C::AddRef>*);

  template <typename >
  static Yes& Check(...);

 public:
  static const bool value = sizeof(Check<Base>(0)) == sizeof(Yes);
};

// Helpers to assert that arguments of a recounted type are bound with a
// scoped_refptr.
template <bool IsClasstype, typename T>
struct UnsafeBindtoRefCountedArgHelper : false_type {
};

template <typename T>
struct UnsafeBindtoRefCountedArgHelper<true, T>
    : integral_constant<bool, SupportsAddRefAndRelease<T>::value> {
};

template <typename T>
struct UnsafeBindtoRefCountedArg : false_type {
};

template <typename T>
struct UnsafeBindtoRefCountedArg<T*>
    : UnsafeBindtoRefCountedArgHelper<is_class<T>::value, T> {
};

template <typename T>
class HasIsMethodTag {
  typedef char Yes[1];
  typedef char No[2];

  template <typename U>
  static Yes& Check(typename U::IsMethod*);

  template <typename U>
  static No& Check(...);

 public:
  static const bool value = sizeof(Check<T>(0)) == sizeof(Yes);
};

template <typename T>
class UnretainedWrapper {
 public:
  explicit UnretainedWrapper(T* o) : ptr_(o) {}
  T* get() const { return ptr_; }
 private:
  T* ptr_;
};

template <typename T>
class ConstRefWrapper {
 public:
  explicit ConstRefWrapper(const T& o) : ptr_(&o) {}
  const T& get() const { return *ptr_; }
 private:
  const T* ptr_;
};

template <typename T>
struct IgnoreResultHelper {
  explicit IgnoreResultHelper(T functor) : functor_(functor) {}

  T functor_;
};

template <typename T>
struct IgnoreResultHelper<Callback<T> > {
  explicit IgnoreResultHelper(const Callback<T>& functor) : functor_(functor) {}

  const Callback<T>& functor_;
};

// An alternate implementation is to avoid the destructive copy, and instead
// specialize ParamTraits<> for OwnedWrapper<> to change the StorageType to
// a class that is essentially a scoped_ptr<>.
//
// The current implementation has the benefit though of leaving ParamTraits<>
// fully in callback_internal.h as well as avoiding type conversions during
// storage.
template <typename T>
class OwnedWrapper {
 public:
  explicit OwnedWrapper(T* o) : ptr_(o) {}
  ~OwnedWrapper() { delete ptr_; }
  T* get() const { return ptr_; }
  OwnedWrapper(const OwnedWrapper& other) {
    ptr_ = other.ptr_;
    other.ptr_ = NULL;
  }

 private:
  mutable T* ptr_;
};

// Unwrap the stored parameters for the wrappers above.
template <typename T>
struct UnwrapTraits {
  typedef const T& ForwardType;
  static ForwardType Unwrap(const T& o) { return o; }
};

template <typename T>
struct UnwrapTraits<UnretainedWrapper<T> > {
  typedef T* ForwardType;
  static ForwardType Unwrap(UnretainedWrapper<T> unretained) {
    return unretained.get();
  }
};

template <typename T>
struct UnwrapTraits<ConstRefWrapper<T> > {
  typedef const T& ForwardType;
  static ForwardType Unwrap(ConstRefWrapper<T> const_ref) {
    return const_ref.get();
  }
};

template <typename T>
struct UnwrapTraits<scoped_refptr<T> > {
  typedef T* ForwardType;
  static ForwardType Unwrap(const scoped_refptr<T>& o) { return o.get(); }
};

template <typename T>
struct UnwrapTraits<WeakPtr<T> > {
  typedef const WeakPtr<T>& ForwardType;
  static ForwardType Unwrap(const WeakPtr<T>& o) { return o; }
};

template <typename T>
struct UnwrapTraits<OwnedWrapper<T> > {
  typedef T* ForwardType;
  static ForwardType Unwrap(const OwnedWrapper<T>& o) {
    return o.get();
  }
};

// Utility for handling different refcounting semantics in the Bind()
// function.
template <bool, typename T>
struct MaybeRefcount;

template <typename T>
struct MaybeRefcount<false, T> {
  static void AddRef(const T&) {}
  static void Release(const T&) {}
};

template <typename T, size_t n>
struct MaybeRefcount<false, T[n]> {
  static void AddRef(const T*) {}
  static void Release(const T*) {}
};

template <typename T>
struct MaybeRefcount<true, T*> {
  static void AddRef(T* o) { o->AddRef(); }
  static void Release(T* o) { o->Release(); }
};

template <typename T>
struct MaybeRefcount<true, UnretainedWrapper<T> > {
  static void AddRef(const UnretainedWrapper<T>&) {}
  static void Release(const UnretainedWrapper<T>&) {}
};

template <typename T>
struct MaybeRefcount<true, OwnedWrapper<T> > {
  static void AddRef(const OwnedWrapper<T>&) {}
  static void Release(const OwnedWrapper<T>&) {}
};

// No need to additionally AddRef() and Release() since we are storing a
// scoped_refptr<> inside the storage object already.
template <typename T>
struct MaybeRefcount<true, scoped_refptr<T> > {
  static void AddRef(const scoped_refptr<T>& o) {}
  static void Release(const scoped_refptr<T>& o) {}
};

template <typename T>
struct MaybeRefcount<true, const T*> {
  static void AddRef(const T* o) { o->AddRef(); }
  static void Release(const T* o) { o->Release(); }
};

template <typename T>
struct MaybeRefcount<true, WeakPtr<T> > {
  static void AddRef(const WeakPtr<T>&) {}
  static void Release(const WeakPtr<T>&) {}
};

template <typename R>
void VoidReturnAdapter(Callback<R(void)> callback) {
  callback.Run();
}

// IsWeakMethod is a helper that determine if we are binding a WeakPtr<> to a
// method.  It is unsed internally by Bind() to select the correct
// InvokeHelper that will no-op itself in the event the WeakPtr<> for
// the target object is invalidated.
//
// P1 should be the type of the object that will be received of the method.
template <bool IsMethod, typename P1>
struct IsWeakMethod : public false_type {};

template <typename T>
struct IsWeakMethod<true, WeakPtr<T> > : public true_type {};

template <typename T>
struct IsWeakMethod<true, ConstRefWrapper<WeakPtr<T> > > : public true_type {};

}  // namespace internal

template <typename T>
static inline internal::UnretainedWrapper<T> Unretained(T* o) {
  return internal::UnretainedWrapper<T>(o);
}

template <typename T>
static inline internal::ConstRefWrapper<T> ConstRef(const T& o) {
  return internal::ConstRefWrapper<T>(o);
}

template <typename T>
static inline internal::OwnedWrapper<T> Owned(T* o) {
  return internal::OwnedWrapper<T>(o);
}

template <typename R>
static inline Closure IgnoreReturn(Callback<R(void)> callback) {
  return Bind(&internal::VoidReturnAdapter<R>, callback);
}

template <typename T>
static inline internal::IgnoreResultHelper<T> IgnoreResult(T data) {
  return internal::IgnoreResultHelper<T>(data);
}

template <typename T>
static inline internal::IgnoreResultHelper<Callback<T> >
IgnoreResult(const Callback<T>& data) {
  return internal::IgnoreResultHelper<Callback<T> >(data);
}


}  // namespace base

#endif  // BASE_BIND_HELPERS_H_
