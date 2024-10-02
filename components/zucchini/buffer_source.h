// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_BUFFER_SOURCE_H_
#define COMPONENTS_ZUCCHINI_BUFFER_SOURCE_H_

#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <type_traits>

#include "base/check_op.h"
#include "components/zucchini/buffer_view.h"

namespace zucchini {

// BufferSource acts like an input stream with convenience methods to parse data
// from a contiguous sequence of raw data. The underlying ConstBufferView
// emulates a cursor to track current read position, and guards against buffer
// overrun. Where applicable, BufferSource should be passed by pointer to
// maintain cursor progress across reads.
class BufferSource : public ConstBufferView {
 public:
  // LEB128 info: http://dwarfstd.org/doc/dwarf-2.0.0.pdf , Section 7.6.
  enum : size_t { kMaxLeb128Size = 5 };

  static BufferSource FromRange(const_iterator first, const_iterator last) {
    return BufferSource(ConstBufferView::FromRange(first, last));
  }

  using ConstBufferView::ConstBufferView;
  BufferSource() = default;
  explicit BufferSource(ConstBufferView buffer);
  BufferSource(const BufferSource&) = default;
  BufferSource& operator=(BufferSource&&) = default;

  // Moves the cursor forward by |n| bytes, or to the end if data is exhausted.
  // Returns a reference to *this, to allow chaining, e.g.:
  //   if (!buffer_source.Skip(1024).GetValue<uint32_t>(&value)) {
  //      ... // Handle error.
  //   }
  // Notice that Skip() defers error handling to GetValue().
  BufferSource& Skip(size_type n);

  // Returns true if |value| matches data starting at the cursor when
  // reinterpreted as the integral type |T|.
  template <class T>
  bool CheckNextValue(const T& value) const {
    static_assert(std::is_integral<T>::value,
                  "Value type must be an integral type");
    DCHECK_NE(begin(), nullptr);
    if (Remaining() < sizeof(T))
      return false;
    return value == *reinterpret_cast<const T*>(begin());
  }

  // Returns true if the next bytes.size() bytes at the cursor match those in
  // |bytes|.
  bool CheckNextBytes(std::initializer_list<uint8_t> bytes) const;

  // Same as CheckNextBytes(), but moves the cursor by bytes.size() if read is
  // successfull.
  bool ConsumeBytes(std::initializer_list<uint8_t> bytes);

  // Tries to reinterpret data as type |T|, starting at the cursor and to write
  // the result into |value|, while moving the cursor forward by sizeof(T).
  // Returns true if sufficient data is available, and false otherwise.
  template <class T>
  bool GetValue(T* value) {
    static_assert(std::is_standard_layout<T>::value,
                  "Value type must be a standard layout type");

    DCHECK_NE(begin(), nullptr);
    if (Remaining() < sizeof(T))
      return false;
    *value = *reinterpret_cast<const T*>(begin());
    remove_prefix(sizeof(T));
    return true;
  }

  // Tries to reinterpret data as type |T| at the cursor and to return a
  // reinterpreted pointer of type |T| pointing into the underlying data, while
  // moving the cursor forward by sizeof(T). Returns nullptr if insufficient
  // data is available.
  template <class T>
  const T* GetPointer() {
    static_assert(std::is_standard_layout<T>::value,
                  "Value type must be a standard layout type");

    DCHECK_NE(begin(), nullptr);
    if (Remaining() < sizeof(T))
      return nullptr;
    const T* ptr = reinterpret_cast<const T*>(begin());
    remove_prefix(sizeof(T));
    return ptr;
  }

  // Tries to reinterpret data as an array of type |T| with |count| elements,
  // starting at the cursor, and to return a reinterpreted pointer of type |T|
  // pointing into the underlying data, while advancing the cursor beyond the
  // array. Returns nullptr if insufficient data is available.
  template <class T>
  const T* GetArray(size_t count) {
    static_assert(std::is_standard_layout<T>::value,
                  "Value type must be a standard layout type");

    if (Remaining() / sizeof(T) < count)
      return nullptr;
    const T* array = reinterpret_cast<const T*>(begin());
    remove_prefix(count * sizeof(T));
    return array;
  }

  // If sufficient data is available, assigns |buffer| to point to a region of
  // |size| bytes starting at the cursor, while advancing the cursor beyond the
  // region, and returns true. Otherwise returns false.
  bool GetRegion(size_type size, ConstBufferView* buffer);

  // Reads an Unsigned Little Endian Base 128 (uleb128) int at |first_|. If
  // successful, writes the result to |value|, advances |first_|, and returns
  // true. Otherwise returns false.
  bool GetUleb128(uint32_t* value);

  // Reads a Signed Little Endian Base 128 (sleb128) int at |first_|. If
  // successful, writes the result to |value|, advances |first_|, and returns
  // true. Otherwise returns false.
  bool GetSleb128(int32_t* value);

  // Reads uleb128 / sleb128 at |first_| but discards the result. If successful,
  // advances |first_| and returns true. Otherwise returns false.
  bool SkipLeb128();

  // Returns the number of bytes remaining from cursor until end.
  size_type Remaining() const { return size(); }
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_BUFFER_SOURCE_H_
