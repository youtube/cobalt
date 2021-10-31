// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_MOVABLE_H_
#define COBALT_RENDER_TREE_MOVABLE_H_

// Defines a helper structure and Pass() method that can be used to express
// that we are passing the structure with move semantics and not copy semantics.
// This would be trivial in C++11.  It should be passed directly into a
// constructor or object that will consume a moved object.
// It is different than what is found within base/move.h because this does
// not restrict one from also copying the object, and it also makes no attempt
// to automatically detect when the host object is being used as an rvalue,
// it must always be made explicit by calling Pass().
#define DECLARE_AS_MOVABLE(x)                      \
  class Moved {                                    \
   public:                                         \
    explicit Moved(x* object) : object_(object) {} \
    x* operator*() { return object_; }             \
    x* operator->() { return object_; }            \
                                                   \
   private:                                        \
    x* object_;                                    \
  };                                               \
                                                   \
  Moved Pass() { return Moved(this); }

#endif  // COBALT_RENDER_TREE_MOVABLE_H_
