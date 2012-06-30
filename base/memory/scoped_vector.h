// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SCOPED_VECTOR_H_
#define BASE_MEMORY_SCOPED_VECTOR_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/move.h"
#include "base/stl_util.h"

// ScopedVector wraps a vector deleting the elements from its
// destructor.
template <class T>
class ScopedVector {
  MOVE_ONLY_TYPE_FOR_CPP_03(ScopedVector, RValue)

 public:
  typedef typename std::vector<T*>::allocator_type allocator_type;
  typedef typename std::vector<T*>::size_type size_type;
  typedef typename std::vector<T*>::difference_type difference_type;
  typedef typename std::vector<T*>::pointer pointer;
  typedef typename std::vector<T*>::const_pointer const_pointer;
  typedef typename std::vector<T*>::reference reference;
  typedef typename std::vector<T*>::const_reference const_reference;
  typedef typename std::vector<T*>::value_type value_type;
  typedef typename std::vector<T*>::iterator iterator;
  typedef typename std::vector<T*>::const_iterator const_iterator;
  typedef typename std::vector<T*>::reverse_iterator reverse_iterator;
  typedef typename std::vector<T*>::const_reverse_iterator
      const_reverse_iterator;

  ScopedVector() {}
  ~ScopedVector() { reset(); }
  ScopedVector(RValue& other) { swap(other); }

  ScopedVector& operator=(RValue& rhs) {
    swap(rhs);
    return *this;
  }

  T*& operator[](size_t i) { return v[i]; }
  const T* operator[](size_t i) const { return v[i]; }

  bool empty() const { return v.empty(); }
  size_t size() const { return v.size(); }

  reverse_iterator rbegin() { return v.rbegin(); }
  const_reverse_iterator rbegin() const { return v.rbegin(); }
  reverse_iterator rend() { return v.rend(); }
  const_reverse_iterator rend() const { return v.rend(); }

  iterator begin() { return v.begin(); }
  const_iterator begin() const { return v.begin(); }
  iterator end() { return v.end(); }
  const_iterator end() const { return v.end(); }

  const_reference front() const { return v.front(); }
  reference front() { return v.front(); }
  const_reference back() const { return v.back(); }
  reference back() { return v.back(); }

  void push_back(T* elem) { v.push_back(elem); }

  std::vector<T*>& get() { return v; }
  const std::vector<T*>& get() const { return v; }
  void swap(std::vector<T*>& other) { v.swap(other); }
  void swap(ScopedVector<T>& other) { v.swap(other.v); }
  void release(std::vector<T*>* out) {
    out->swap(v);
    v.clear();
  }

  void reset() { STLDeleteElements(&v); }
  void reserve(size_t capacity) { v.reserve(capacity); }
  void resize(size_t new_size) { v.resize(new_size); }

  template<typename InputIterator>
  void assign(InputIterator begin, InputIterator end) {
    v.assign(begin, end);
  }

  void clear() { v.clear(); }

  // Lets the ScopedVector take ownership of |x|.
  iterator insert(iterator position, T* x) {
    return v.insert(position, x);
  }

  // Lets the ScopedVector take ownership of elements in [first,last).
  template<typename InputIterator>
  void insert(iterator position, InputIterator first, InputIterator last) {
    v.insert(position, first, last);
  }

  iterator erase(iterator position) {
    delete *position;
    return v.erase(position);
  }

  iterator erase(iterator first, iterator last) {
    STLDeleteContainerPointers(first, last);
    return v.erase(first, last);
  }

  // Like |erase()|, but doesn't delete the element at |position|.
  iterator weak_erase(iterator position) {
    return v.erase(position);
  }

  // Like |erase()|, but doesn't delete the elements in [first, last).
  iterator weak_erase(iterator first, iterator last) {
    return v.erase(first, last);
  }

 private:
  std::vector<T*> v;
};

#endif  // BASE_MEMORY_SCOPED_VECTOR_H_
