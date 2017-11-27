// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BASE_POLYMORPHIC_EQUATABLE_H_
#define COBALT_BASE_POLYMORPHIC_EQUATABLE_H_

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/type_id.h"

namespace base {

// Derive from this class from the base class of a class hierarchy in order to
// provide your hierarchy with the functinoality to test for equivalence between
// different instances without knowing what the type of each instance is.
// You will also need to add the line
//   DEFINE_POLYMORPHIC_EQUATABLE_TYPE(MyDerivedClass)
// to the public section of each of your derived classes, to have them
// automatically implement the virtual methods declared in PolymorphicEquatable.
// You will additionally have to ensure that operator==() is defined, as it
// will be used to test for equality among objects of the same type.
class PolymorphicEquatable {
 public:
  virtual bool Equals(const PolymorphicEquatable& other) const = 0;
  virtual base::TypeId GetTypeId() const = 0;
};

// Used to provide type-safe equality checking even when the exact
// PolymorphicEquatable type is unknown.  For any class T that is intended to be
// a descendant of PolymorphicEquatable, it should call this macro in the public
// section of its class declaration.
#define DEFINE_POLYMORPHIC_EQUATABLE_TYPE(DerivedClassType)                  \
  bool Equals(const PolymorphicEquatable& other) const override {            \
    return base::GetTypeId<DerivedClassType>() == other.GetTypeId() &&       \
           *this ==                                                          \
               *base::polymorphic_downcast<const DerivedClassType*>(&other); \
  }                                                                          \
                                                                             \
  base::TypeId GetTypeId() const override {                                  \
    return base::GetTypeId<DerivedClassType>();                              \
  }

}  // namespace base

#endif  // COBALT_BASE_POLYMORPHIC_EQUATABLE_H_
