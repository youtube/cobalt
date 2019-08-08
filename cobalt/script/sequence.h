// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_SEQUENCE_H_
#define COBALT_SCRIPT_SEQUENCE_H_

#include <algorithm>
#include <iosfwd>
#include <vector>

#include "base/memory/ref_counted.h"

namespace cobalt {
namespace script {

// Implementation of WebIDL sequence<T> types, which are copyable and
// assignable. This is a template type that behaves minimally like a vector, but
// specializes to handle the proper referencing based on the element type.
//
// https://heycam.github.io/webidl/#idl-sequence
template <typename T>
class Sequence {
 public:
  typedef std::vector<T> SequenceType;

  // --- Vector partial emulation ---
  typedef typename SequenceType::size_type size_type;
  typedef typename SequenceType::value_type value_type;
  typedef typename SequenceType::reference reference;
  typedef typename SequenceType::const_reference const_reference;
  typedef typename SequenceType::iterator iterator;
  typedef typename SequenceType::const_iterator const_iterator;
  void push_back(const_reference value) { sequence_.push_back(value); }
  size_type size() const { return sequence_.size(); }
  bool empty() const { return sequence_.empty(); }
  const_reference at(size_type index) const { return sequence_.at(index); }
  void swap(Sequence<T>& other) { sequence_.swap(other.sequence_); }
  iterator begin() { return sequence_.begin(); }
  const_iterator begin() const { return sequence_.begin(); }
  const_iterator cbegin() const { return sequence_.cbegin(); }
  iterator end() { return sequence_.end(); }
  const_iterator end() const { return sequence_.end(); }
  const_iterator cend() const { return sequence_.cend(); }
  void clear() { sequence_.clear(); }

 private:
  SequenceType sequence_;
};

// Needed to instantiate base::Optional< Sequence<T> >
template <typename T>
inline std::ostream& operator<<(std::ostream& stream,
                                const Sequence<T>& sequence) {
  stream << "[Sequence ";
  for (typename Sequence<T>::size_type i = 0, max = sequence.size(); i < max;
       ++i) {
    stream << (i > 0 ? ", " : "") << sequence.at(i);
  }
  stream << " ]";

  return stream;
}

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SEQUENCE_H_
