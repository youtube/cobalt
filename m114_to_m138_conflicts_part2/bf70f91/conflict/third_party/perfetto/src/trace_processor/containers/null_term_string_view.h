/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_CONTAINERS_NULL_TERM_STRING_VIEW_H_
#define SRC_TRACE_PROCESSOR_CONTAINERS_NULL_TERM_STRING_VIEW_H_

<<<<<<< HEAD
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/public/compiler.h"

namespace perfetto::trace_processor {
=======
#include "perfetto/ext/base/string_view.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

// A string-like object that refers to a non-owned piece of memory which is
// null terminated.
class NullTermStringView : public base::StringView {
 public:
  // Creates an empty view.
<<<<<<< HEAD
  PERFETTO_ALWAYS_INLINE NullTermStringView() : StringView() {}
=======
  NullTermStringView() : StringView() {}
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Make the view copy constructible.
  NullTermStringView(const NullTermStringView&) = default;
  NullTermStringView& operator=(const NullTermStringView&) = default;

  // Creates a NullTermStringView from a null-terminated C string.
  // Deliberately not "explicit".
<<<<<<< HEAD
  PERFETTO_ALWAYS_INLINE NullTermStringView(const char* cstr)
      : StringView(cstr) {}
=======
  NullTermStringView(const char* cstr) : StringView(cstr) {}
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Creates a NullTermStringView from a null terminated C-string where the
  // size is known. This allows a call to strlen() to be avoided.
  // Note: This string MUST be null terminated i.e. data[size] == '\0' MUST hold
  // for this constructor to be valid.
<<<<<<< HEAD
  PERFETTO_ALWAYS_INLINE NullTermStringView(const char* data, size_t size)
      : StringView(data, size) {
=======
  NullTermStringView(const char* data, size_t size) : StringView(data, size) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    PERFETTO_DCHECK(data[size] == '\0');
  }

  // This instead has to be explicit, as creating a NullTermStringView out of a
  // std::string can be subtle.
<<<<<<< HEAD
  PERFETTO_ALWAYS_INLINE explicit NullTermStringView(const std::string& str)
      : StringView(str) {}
=======
  explicit NullTermStringView(const std::string& str) : StringView(str) {}
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Returns the null terminated C-string backing this string view. The same
  // pointer as |data()| is returned.
  const char* c_str() const { return data(); }
};

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_CONTAINERS_NULL_TERM_STRING_VIEW_H_
