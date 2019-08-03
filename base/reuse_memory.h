/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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

// This class uses a static free list to reuse memory block. Note, this
// class is not multi thread safe.

#ifndef BASE_REUSE_MEMORY_H_
#define BASE_REUSE_MEMORY_H_

namespace base {

// For base class if there is one.
#define DECLARE_REUSE_MEMORY_BASE_FREE_FUNCTION() \
  virtual bool AddToFreeList() { return false; }

// Declear free function
#define DECLARE_REUSE_MEMORY_FREE_FUNCTION() \
  virtual bool AddToFreeList() { AddObjectToFreeList(this); return true; }

// Utility macros
#define TryRecycleAdoptPtrAndReturn(type, ...) \
  void* data = AllocateFromFreeList(); \
    if (data) \
      return adoptPtr(new (data) type(__VA_ARGS__));

// Free a vector<OwnPtr>. Delete the objects which are not handled by
// AddToFreeList()
#define FreeOwnPtrVector(type, vector_var) \
  for (int _free_i = vector_var.size() - 1; _free_i >= 0; --_free_i) { \
        type* free_data = vector_var[_free_i].leakPtr(); \
        if (!free_data->AddToFreeList()) delete free_data; \
  }

// For reusable class. Note: this class should be the last one in base
// class list.
template<typename T>
class ReuseMemory {
 public:
   ReuseMemory() : next_(NULL) {}
   virtual ~ReuseMemory() {}

 protected:
  // Allocate object from free list
  static void* AllocateFromFreeList() {
    if (free_list_) {  // List is valid and not empty
      T* data = free_list_;
      free_list_ = free_list_->next_;
      return data;
    }
    return NULL;
  }
  // Add object to free list
  static void AddObjectToFreeList(T* obj) {
    obj->next_ = free_list_;
    free_list_ = obj;
  }

  // Release all objects in free list
  static void FreeAll() {
    while (free_list_) {
      T* data = free_list_;
      free_list_ = free_list_->next_;
      delete data;
    }
  }

 private:
  T* next_;
  static T* free_list_;
};

}  // namespace base

#endif  // BASE_REUSE_MEMORY_H_
