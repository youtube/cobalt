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

#ifndef COBALT_SCRIPT_SEQUENCE_H_
#define COBALT_SCRIPT_SEQUENCE_H_

#include <algorithm>
#include <iosfwd>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"

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
  Sequence() {}
  Sequence(const Sequence& other) { *this = other; }

  void clear() { sequence_.erase(sequence_.begin(), sequence_.end()); }

  Sequence& operator=(const Sequence& other) {
    clear();
    size_type max = other.size();
    for (size_type i = 0; i < max; ++i) {
      push_back(other.at(i));
    }
    return *this;
  }

  typedef std::vector<T> SequenceType;

  // --- Vector partial emulation ---
  typedef typename SequenceType::size_type size_type;
  typedef typename SequenceType::reference reference;
  typedef typename SequenceType::const_reference const_reference;
  void push_back(const_reference value) { sequence_.push_back(value); }
  size_type size() const { return sequence_.size(); }
  bool empty() const { return sequence_.empty(); }
  const_reference at(size_type index) const { return sequence_.at(index); }
  void swap(Sequence<T>& other) { sequence_.swap(other.sequence_); }

 private:
  SequenceType sequence_;
};

// Needed to instantiate base::optional< Sequence<T> >
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
