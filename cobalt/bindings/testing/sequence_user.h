// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_SEQUENCE_USER_H_
#define COBALT_BINDINGS_TESTING_SEQUENCE_USER_H_

#include <string>

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

class SequenceUser : public script::Wrappable {
 public:
  SequenceUser() {}

  script::Sequence<scoped_refptr<ArbitraryInterface> > GetInterfaceSequence() {
    return interfaces_;
  }

  script::Sequence<
      script::Sequence<script::Sequence<scoped_refptr<ArbitraryInterface> > > >
  GetInterfaceSequenceSequenceSequence() {
    return triple_sequence_;
  }

  script::Sequence<int32_t> GetLongSequence() { return longs_; }

  script::Sequence<std::string> GetStringSequence() { return strings_; }

  script::Sequence<script::Sequence<std::string> > GetStringSequenceSequence() {
    return sequences_;
  }

  script::UnionType2<std::string, script::Sequence<std::string> >
  GetUnionOfStringAndStringSequence() {
    return union_;
  }

  script::Sequence<
      script::UnionType2<std::string, scoped_refptr<ArbitraryInterface> > >
  GetUnionSequence() {
    return unions_;
  }

  void SetInterfaceSequence(
      const script::Sequence<scoped_refptr<ArbitraryInterface> >& sequence) {
    interfaces_ = sequence;
  }

  void SetInterfaceSequenceSequenceSequence(
      const script::Sequence<script::Sequence<
          script::Sequence<scoped_refptr<ArbitraryInterface> > > >& sequence) {
    triple_sequence_ = sequence;
  }

  void SetLongSequence(const script::Sequence<int32_t>& sequence) {
    longs_ = sequence;
  }

  void SetStringSequence(const script::Sequence<std::string>& sequence) {
    strings_ = sequence;
  }

  void SetStringSequenceSequence(
      const script::Sequence<script::Sequence<std::string> >& sequence) {
    sequences_ = sequence;
  }

  void SetUnionOfStringAndStringSequence(
      const script::UnionType2<std::string, script::Sequence<std::string> >&
          union_value) {
    union_ = union_value;
  }

  void SetUnionSequence(
      const script::Sequence<
          script::UnionType2<std::string, scoped_refptr<ArbitraryInterface> > >&
          sequence) {
    unions_ = sequence;
  }

  DEFINE_WRAPPABLE_TYPE(SequenceUser);

 private:
  script::Sequence<scoped_refptr<ArbitraryInterface> > interfaces_;
  script::Sequence<int32_t> longs_;
  script::Sequence<std::string> strings_;
  script::Sequence<script::Sequence<std::string> > sequences_;
  script::Sequence<
      script::Sequence<script::Sequence<scoped_refptr<ArbitraryInterface> > > >
      triple_sequence_;
  script::UnionType2<std::string, script::Sequence<std::string> > union_;
  script::Sequence<
      script::UnionType2<std::string, scoped_refptr<ArbitraryInterface> > >
      unions_;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_SEQUENCE_USER_H_
