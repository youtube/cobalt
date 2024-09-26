// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GLIB_SCOPED_GOBJECT_H_
#define UI_BASE_GLIB_SCOPED_GOBJECT_H_

#include <glib-object.h>

#include "base/check.h"
#include "base/memory/raw_ptr.h"

// Similar to a std::shared_ptr for GObject types.
template <typename T>
class ScopedGObject {
 public:
  ScopedGObject() = default;

  ScopedGObject(const ScopedGObject<T>& other) : obj_(other.obj_) { Ref(); }

  ScopedGObject(ScopedGObject<T>&& other) : obj_(other.obj_) {
    other.obj_ = nullptr;
  }

  ~ScopedGObject() { Unref(); }

  ScopedGObject<T>& operator=(const ScopedGObject<T>& other) {
    Unref();
    obj_ = other.obj_;
    Ref();
    return *this;
  }

  ScopedGObject<T>& operator=(ScopedGObject<T>&& other) {
    Unref();
    obj_ = other.obj_;
    other.obj_ = nullptr;
    return *this;
  }

  T* get() { return obj_; }

  operator T*() { return obj_; }

 private:
  template <typename U>
  friend ScopedGObject<U> TakeGObject(U* obj);
  template <typename U>
  friend ScopedGObject<U> WrapGObject(U* obj);

  explicit ScopedGObject(T* obj) : obj_(obj) {}

  void RefSink() {
    // Remove the floating reference from |obj_| if it has one.
    if (obj_ && g_object_is_floating(obj_))
      g_object_ref_sink(obj_);
  }

  void Ref() {
    if (obj_) {
      DCHECK(!g_object_is_floating(obj_));
      g_object_ref(obj_);
    }
  }

  // This function is necessary so that gtk can overload it in
  // the case of T = GtkStyleContext.
  void Unref() {
    if (obj_)
      g_object_unref(obj_);
  }

  raw_ptr<T> obj_ = nullptr;
};

// Create a ScopedGObject and do not increase the GObject's reference count.
// This is usually used to reference a newly-created GObject, which are created
// with a reference count of 1 by default.
template <typename T>
ScopedGObject<T> TakeGObject(T* obj) {
  ScopedGObject<T> scoped(obj);
  scoped.RefSink();
  return scoped;
}

// Create a ScopedGObject and increase the GObject's reference count by 1.
// This is usually used to reference an existing GObject.
template <typename T>
ScopedGObject<T> WrapGObject(T* obj) {
  ScopedGObject<T> scoped(obj);
  scoped.Ref();
  return scoped;
}

#endif  // UI_BASE_GLIB_SCOPED_GOBJECT_H_
