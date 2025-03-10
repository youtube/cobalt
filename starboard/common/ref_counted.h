// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STARBOARD_COMMON_REF_COUNTED_H_
#define STARBOARD_COMMON_REF_COUNTED_H_

#include <algorithm>
#include <atomic>
#include <utility>

#include "starboard/common/log.h"

namespace starboard {
namespace subtle {

class RefCountedBase {
 public:
  bool HasOneRef() const;

 protected:
  RefCountedBase();
  ~RefCountedBase();

  void AddRef() const;

  // Returns true if the object should self-delete.
  bool Release() const;

 private:
  mutable int ref_count_;
#ifndef NDEBUG
  mutable bool in_dtor_;
#endif
};

class RefCountedThreadSafeBase {
 public:
  bool HasOneRef() const;

 protected:
  RefCountedThreadSafeBase();
  ~RefCountedThreadSafeBase();

  void AddRef() const;

  // Returns true if the object should self-delete.
  bool Release() const;

 private:
  mutable std::atomic<int32_t> ref_count_;
#ifndef NDEBUG
  mutable bool in_dtor_;
#endif
};

}  // namespace subtle

//
// A base class for reference counted classes.  Otherwise, known as a cheap
// knock-off of WebKit's RefCounted<T> class.  To use this guy just extend your
// class from it like so:
//
//   class MyFoo : public starboard::RefCounted<MyFoo> {
//    ...
//    private:
//     friend class starboard::RefCounted<MyFoo>;
//     ~MyFoo();
//   };
//
// You should always make your destructor private, to avoid any code deleting
// the object accidentally while there are references to it.
template <class T>
class RefCounted : public subtle::RefCountedBase {
 public:
  RefCounted() {}

  void AddRef() const { subtle::RefCountedBase::AddRef(); }

  void Release() const {
    if (subtle::RefCountedBase::Release()) {
      delete static_cast<const T*>(this);
    }
  }

 protected:
  ~RefCounted() {}
};

// Forward declaration.
template <class T, typename Traits>
class RefCountedThreadSafe;

// Default traits for RefCountedThreadSafe<T>.  Deletes the object when its ref
// count reaches 0.  Overload to delete it on a different thread etc.
template <typename T>
struct DefaultRefCountedThreadSafeTraits {
  static void Destruct(const T* x) {
    // Delete through RefCountedThreadSafe to make child classes only need to be
    // friend with RefCountedThreadSafe instead of this struct, which is an
    // implementation detail.
    RefCountedThreadSafe<T, DefaultRefCountedThreadSafeTraits>::DeleteInternal(
        x);
  }
};

//
// A thread-safe variant of RefCounted<T>
//
//   class MyFoo : public starboard::RefCountedThreadSafe<MyFoo> {
//    ...
//   };
//
// If you're using the default trait, then you should add compile time
// asserts that no one else is deleting your object.  i.e.
//    private:
//     friend class starboard::RefCountedThreadSafe<MyFoo>;
//     ~MyFoo();
template <class T, typename Traits = DefaultRefCountedThreadSafeTraits<T>>
class RefCountedThreadSafe : public subtle::RefCountedThreadSafeBase {
 public:
  RefCountedThreadSafe() {}

  void AddRef() const { subtle::RefCountedThreadSafeBase::AddRef(); }

  void Release() const {
    if (subtle::RefCountedThreadSafeBase::Release()) {
      Traits::Destruct(static_cast<const T*>(this));
    }
  }

 protected:
  ~RefCountedThreadSafe() {}

 private:
  friend struct DefaultRefCountedThreadSafeTraits<T>;
  static void DeleteInternal(const T* x) { delete x; }
};

//
// A thread-safe wrapper for some piece of data so we can place other
// things in scoped_refptrs<>.
//
template <typename T>
class RefCountedData
    : public starboard::RefCountedThreadSafe<starboard::RefCountedData<T>> {
 public:
  RefCountedData() : data() {}
  RefCountedData(const T& in_value) : data(in_value) {}        // NOLINT
  RefCountedData(T&& in_value) : data(std::move(in_value)) {}  // NOLINT

  T data;

 private:
  friend class starboard::RefCountedThreadSafe<starboard::RefCountedData<T>>;
  ~RefCountedData() {}
};

//
// A smart pointer class for reference counted objects.  Use this class instead
// of calling AddRef and Release manually on a reference counted object to
// avoid common memory leaks caused by forgetting to Release an object
// reference.  Sample usage:
//
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//   };
//
//   void some_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // |foo| is released when this function returns
//   }
//
//   void some_other_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     ...
//     foo = NULL;  // explicitly releases |foo|
//     ...
//     if (foo)
//       foo->Method(param);
//   }
//
// The above examples show how scoped_refptr<T> acts like a pointer to T.
// Given two scoped_refptr<T> classes, it is also possible to exchange
// references between the two objects, like so:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references NULL.
//   }
//
// To make both |a| and |b| in the above example reference the same MyFoo
// object, simply use the assignment operator:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//   }
//
template <class T>
class scoped_refptr {
 public:
  typedef T element_type;

  scoped_refptr() : ptr_(NULL) {}

  scoped_refptr(T* p) : ptr_(p) {  // NOLINT
    if (ptr_) {
      ptr_->AddRef();
    }
  }

  scoped_refptr(const scoped_refptr<T>& r) : ptr_(r.ptr_) {
    if (ptr_) {
      ptr_->AddRef();
    }
  }

  // Move constructor. This is required in addition to the move conversion
  // constructor below.
  scoped_refptr(scoped_refptr<T>&& r) noexcept : ptr_(r.ptr_) {
    r.ptr_ = nullptr;
  }

  // Move conversion constructor.
  template <typename U,
            typename = typename std::enable_if<
                std::is_convertible<U*, T*>::value>::type>
  scoped_refptr(scoped_refptr<U>&& r) noexcept : ptr_(r.get()) {
    r.ptr_ = nullptr;
  }

  template <typename U>
  scoped_refptr(const scoped_refptr<U>& r) : ptr_(r.get()) {
    if (ptr_) {
      ptr_->AddRef();
    }
  }

  ~scoped_refptr() {
    if (ptr_) {
      ptr_->Release();
    }
  }

  T* get() const { return ptr_; }
  operator T*() const { return ptr_; }
  T* operator->() const {
    SB_DCHECK(ptr_ != NULL);
    return ptr_;
  }

  T& operator*() const {
    SB_DCHECK(ptr_ != NULL);
    return *ptr_;
  }

  scoped_refptr<T>& operator=(T* p) {
    // AddRef first so that self assignment should work
    if (p) {
      p->AddRef();
    }
    T* old_ptr = ptr_;
    ptr_ = p;
    if (old_ptr) {
      old_ptr->Release();
    }
    return *this;
  }

  scoped_refptr<T>& operator=(const scoped_refptr<T>& r) {
    return *this = r.ptr_;
  }

  template <typename U>
  scoped_refptr<T>& operator=(const scoped_refptr<U>& r) {
    return *this = r.get();
  }

  void swap(T** pp) {
    T* p = ptr_;
    ptr_ = *pp;
    *pp = p;
  }

  void swap(scoped_refptr<T>& r) { swap(&r.ptr_); }

 protected:
  T* ptr_;

 private:
  // Friend required for move constructors that set r.ptr_ to null.
  template <typename U>
  friend class scoped_refptr;
};

// Handy utility for creating a scoped_refptr<T> out of a T* explicitly without
// having to retype all the template arguments
template <typename T>
scoped_refptr<T> make_scoped_refptr(T* t) {
  return scoped_refptr<T>(t);
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_REF_COUNTED_H_
