// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_SPAN_H_
#define STARBOARD_COMMON_SPAN_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"

// Copied from copied_base/base/containers/span.h, which is base::span as it
// was before Chromium allowed C++20, without its //base dependencies: its
// iterators are plain pointers and CHECK() is SB_CHECK().

namespace starboard {

// [views.constants]
constexpr size_t dynamic_extent = std::numeric_limits<size_t>::max();

template <typename T, size_t Extent = dynamic_extent>
class span;

namespace internal {

// Copied from copied_base/base/template_util.h.
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// Copied from copied_base/base/template_util.h.
template <typename Iter>
using iter_reference_t = decltype(*std::declval<Iter&>());

// Unlike base's IsContiguousIterator, only accepts pointers.
template <typename T>
using IsContiguousIterator = std::is_pointer<remove_cvref_t<T>>;

template <size_t I>
using size_constant = std::integral_constant<size_t, I>;

template <typename T>
struct ExtentImpl : size_constant<dynamic_extent> {};

template <typename T, size_t N>
struct ExtentImpl<T[N]> : size_constant<N> {};

template <typename T, size_t N>
struct ExtentImpl<std::array<T, N>> : size_constant<N> {};

template <typename T, size_t N>
struct ExtentImpl<span<T, N>> : size_constant<N> {};

template <typename T>
using Extent = ExtentImpl<remove_cvref_t<T>>;

template <typename T>
struct IsSpanImpl : std::false_type {};

template <typename T, size_t Extent>
struct IsSpanImpl<span<T, Extent>> : std::true_type {};

template <typename T>
using IsNotSpan = std::negation<IsSpanImpl<std::decay_t<T>>>;

template <typename T>
struct IsStdArrayImpl : std::false_type {};

template <typename T, size_t N>
struct IsStdArrayImpl<std::array<T, N>> : std::true_type {};

template <typename T>
using IsNotStdArray = std::negation<IsStdArrayImpl<std::decay_t<T>>>;

template <typename T>
using IsNotCArray = std::negation<std::is_array<std::remove_reference_t<T>>>;

template <typename From, typename To>
using IsLegalDataConversion = std::is_convertible<From (*)[], To (*)[]>;

template <typename Iter, typename T>
using IteratorHasConvertibleReferenceType =
    IsLegalDataConversion<std::remove_reference_t<iter_reference_t<Iter>>, T>;

template <typename Iter, typename T>
using EnableIfCompatibleContiguousIterator = std::enable_if_t<
    std::conjunction<IsContiguousIterator<Iter>,
                     IteratorHasConvertibleReferenceType<Iter, T>>::value>;

template <typename Container, typename T>
using ContainerHasConvertibleData = IsLegalDataConversion<
    std::remove_pointer_t<decltype(std::data(std::declval<Container>()))>,
    T>;

template <typename Container>
using ContainerHasIntegralSize =
    std::is_integral<decltype(std::size(std::declval<Container>()))>;

template <typename From, size_t FromExtent, typename To, size_t ToExtent>
using EnableIfLegalSpanConversion =
    std::enable_if_t<(ToExtent == dynamic_extent || ToExtent == FromExtent) &&
                     IsLegalDataConversion<From, To>::value>;

// SFINAE check if Array can be converted to a span<T>.
template <typename Array, typename T, size_t Extent>
using EnableIfSpanCompatibleArray =
    std::enable_if_t<(Extent == dynamic_extent ||
                      Extent == internal::Extent<Array>::value) &&
                     ContainerHasConvertibleData<Array, T>::value>;

// SFINAE check if Container can be converted to a span<T>.
template <typename Container, typename T>
using IsSpanCompatibleContainer =
    std::conjunction<IsNotSpan<Container>,
                     IsNotStdArray<Container>,
                     IsNotCArray<Container>,
                     ContainerHasConvertibleData<Container, T>,
                     ContainerHasIntegralSize<Container>>;

template <typename Container, typename T>
using EnableIfSpanCompatibleContainer =
    std::enable_if_t<IsSpanCompatibleContainer<Container, T>::value>;

template <typename Container, typename T, size_t Extent>
using EnableIfSpanCompatibleContainerAndSpanIsDynamic =
    std::enable_if_t<IsSpanCompatibleContainer<Container, T>::value &&
                     Extent == dynamic_extent>;

// A helper template for storing the size of a span. Spans with static extents
// don't require additional storage, since the extent itself is specified in the
// template parameter.
template <size_t Extent>
class ExtentStorage {
 public:
  constexpr explicit ExtentStorage(size_t size) noexcept {}
  constexpr size_t size() const noexcept { return Extent; }
};

// Specialization of ExtentStorage for dynamic extents, which do require
// explicit storage for the size.
template <>
struct ExtentStorage<dynamic_extent> {
  constexpr explicit ExtentStorage(size_t size) noexcept : size_(size) {}
  constexpr size_t size() const noexcept { return size_; }

 private:
  size_t size_;
};

// must_not_be_dynamic_extent prevents |dynamic_extent| from being returned in a
// constexpr context.
template <size_t kExtent>
constexpr size_t must_not_be_dynamic_extent() {
  static_assert(
      kExtent != dynamic_extent,
      "EXTENT should only be used for containers with a static extent.");
  return kExtent;
}

}  // namespace internal

// A span is a value type that represents an array of elements of type T. Since
// it only consists of a pointer to memory with an associated size, it is very
// light-weight. It is cheap to construct, copy, move and use spans, so that
// users are encouraged to use it as a pass-by-value parameter. A span does not
// own the underlying memory, so care must be taken to ensure that a span does
// not outlive the backing store.
//
// span is somewhat analogous to StringPiece, but with arbitrary element types,
// allowing mutation if T is non-const.
//
// span is implicitly convertible from C++ arrays, as well as most [1]
// container-like types that provide a data() and size() method (such as
// std::vector<T>). A mutable span<T> can also be implicitly converted to an
// immutable span<const T>.
//
// Consider using a span for functions that take a data pointer and size
// parameter: it allows the function to still act on an array-like type, while
// allowing the caller code to be a bit more concise.
//
// For read-only data access pass a span<const T>: the caller can supply either
// a span<const T> or a span<T>, while the callee will have a read-only view.
// For read-write access a mutable span<T> is required.
//
// Without span:
//   Read-Only:
//     // std::string HexEncode(const uint8_t* data, size_t size);
//     std::vector<uint8_t> data_buffer = GenerateData();
//     std::string r = HexEncode(data_buffer.data(), data_buffer.size());
//
//  Mutable:
//     // ssize_t SafeSNPrintf(char* buf, size_t N, const char* fmt, Args...);
//     char str_buffer[100];
//     SafeSNPrintf(str_buffer, sizeof(str_buffer), "Pi ~= %lf", 3.14);
//
// With span:
//   Read-Only:
//     // std::string HexEncode(base::span<const uint8_t> data);
//     std::vector<uint8_t> data_buffer = GenerateData();
//     std::string r = HexEncode(data_buffer);
//
//  Mutable:
//     // ssize_t SafeSNPrintf(base::span<char>, const char* fmt, Args...);
//     char str_buffer[100];
//     SafeSNPrintf(str_buffer, "Pi ~= %lf", 3.14);
//
// Spans with "const" and pointers
// -------------------------------
//
// Const and pointers can get confusing. Here are vectors of pointers and their
// corresponding spans:
//
//   const std::vector<int*>        =>  base::span<int* const>
//   std::vector<const int*>        =>  base::span<const int*>
//   const std::vector<const int*>  =>  base::span<const int* const>
//
// Differences from the C++20 draft
// --------------------------------
//
// http://eel.is/c++draft/views contains the latest C++20 draft of std::span.
// Chromium tries to follow the draft as close as possible. Differences between
// the draft and the implementation are documented in subsections below.
//
// Differences from [span.objectrep]:
// - as_bytes() and as_writable_bytes() return spans of uint8_t instead of
//   std::byte (std::byte is a C++17 feature)
//
// Differences from [span.cons]:
// - Constructing a static span (i.e. Extent != dynamic_extent) from a dynamic
//   sized container (e.g. std::vector) requires an explicit conversion (in the
//   C++20 draft this is simply UB)
//
// Furthermore, all constructors and methods are marked noexcept due to the lack
// of exceptions in Chromium.
//
// Due to the lack of class template argument deduction guides in C++14
// appropriate make_span() utility functions are provided.

// [span], class template span
template <typename T, size_t Extent>
class span : public internal::ExtentStorage<Extent> {
 private:
  using ExtentStorage = internal::ExtentStorage<Extent>;

 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = T*;
  // TODO(https://crbug.com/828324): Drop the const_iterator typedef once gMock
  // supports containers without this nested type.
  using const_iterator = iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  static constexpr size_t extent = Extent;

  // [span.cons], span constructors, copy, assignment, and destructor
  constexpr span() noexcept : ExtentStorage(0), data_(nullptr) {
    static_assert(Extent == dynamic_extent || Extent == 0, "Invalid Extent");
  }

  template <typename It,
            typename = internal::EnableIfCompatibleContiguousIterator<It, T>>
  constexpr span(It first, size_t count) noexcept
      : ExtentStorage(count),
        // The use of to_address() here is to handle the case where the iterator
        // `first` is pointing to the container's `end()`. In that case we can
        // not use the address returned from the iterator, or dereference it
        // through the iterator's `operator*`, but we can store it. We must
        // assume in this case that `count` is 0, since the iterator does not
        // point to valid data. Future hardening of iterators may disallow
        // pulling the address from `end()`, as demonstrated by asserts() in
        // libstdc++: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93960.
        //
        // The span API dictates that the `data()` is accessible when size is 0,
        // since the pointer may be valid, so we cannot prevent storing and
        // giving out an invalid pointer here without breaking API compatibility
        // and our unit tests. Thus protecting against this can likely only be
        // successful from inside iterators themselves, where the context about
        // the pointer is known.
        //
        // We can not protect here generally against an invalid iterator/count
        // being passed in, since we have no context to determine if the
        // iterator or count are valid.
        data_(first) {
    SB_CHECK(Extent == dynamic_extent || Extent == count);
  }

  template <
      typename It,
      typename End,
      typename = internal::EnableIfCompatibleContiguousIterator<It, T>,
      typename = std::enable_if_t<!std::is_convertible<End, size_t>::value>>
  constexpr span(It begin, End end) noexcept
      // Subtracting two iterators gives a ptrdiff_t, but the result should be
      // non-negative: see CHECK below.
      : span(begin, static_cast<size_t>(end - begin)) {
    SB_CHECK_LE(begin, end);
  }

  template <
      size_t N,
      typename = internal::EnableIfSpanCompatibleArray<T (&)[N], T, Extent>>
  constexpr span(T (&array)[N]) noexcept : span(std::data(array), N) {}

  template <
      typename U,
      size_t N,
      typename =
          internal::EnableIfSpanCompatibleArray<std::array<U, N>&, T, Extent>>
  constexpr span(std::array<U, N>& array) noexcept
      : span(std::data(array), N) {}

  template <typename U,
            size_t N,
            typename = internal::
                EnableIfSpanCompatibleArray<const std::array<U, N>&, T, Extent>>
  constexpr span(const std::array<U, N>& array) noexcept
      : span(std::data(array), N) {}

  // Conversion from a container that has compatible std::data() and integral
  // std::size().
  template <
      typename Container,
      typename =
          internal::EnableIfSpanCompatibleContainerAndSpanIsDynamic<Container&,
                                                                    T,
                                                                    Extent>>
  constexpr span(Container& container) noexcept
      : span(std::data(container), std::size(container)) {}

  template <
      typename Container,
      typename = internal::EnableIfSpanCompatibleContainerAndSpanIsDynamic<
          const Container&,
          T,
          Extent>>
  constexpr span(const Container& container) noexcept
      : span(std::data(container), std::size(container)) {}

  constexpr span(const span& other) noexcept = default;

  // Conversions from spans of compatible types and extents: this allows a
  // span<T> to be seamlessly used as a span<const T>, but not the other way
  // around. If extent is not dynamic, OtherExtent has to be equal to Extent.
  template <
      typename U,
      size_t OtherExtent,
      typename =
          internal::EnableIfLegalSpanConversion<U, OtherExtent, T, Extent>>
  constexpr span(const span<U, OtherExtent>& other)
      : span(other.data(), other.size()) {}

  constexpr span& operator=(const span& other) noexcept = default;
  ~span() noexcept = default;

  // [span.sub], span subviews
  template <size_t Count>
  constexpr span<T, Count> first() const noexcept {
    static_assert(Count <= Extent, "Count must not exceed Extent");
    SB_CHECK(Extent != dynamic_extent || Count <= size());
    return {data(), Count};
  }

  template <size_t Count>
  constexpr span<T, Count> last() const noexcept {
    static_assert(Count <= Extent, "Count must not exceed Extent");
    SB_CHECK(Extent != dynamic_extent || Count <= size());
    return {data() + (size() - Count), Count};
  }

  template <size_t Offset, size_t Count = dynamic_extent>
  constexpr span<T,
                 (Count != dynamic_extent
                      ? Count
                      : (Extent != dynamic_extent ? Extent - Offset
                                                  : dynamic_extent))>
  subspan() const noexcept {
    static_assert(Offset <= Extent, "Offset must not exceed Extent");
    static_assert(Count == dynamic_extent || Count <= Extent - Offset,
                  "Count must not exceed Extent - Offset");
    SB_CHECK(Extent != dynamic_extent || Offset <= size());
    SB_CHECK(Extent != dynamic_extent || Count == dynamic_extent ||
             Count <= size() - Offset);
    return {data() + Offset, Count != dynamic_extent ? Count : size() - Offset};
  }

  constexpr span<T, dynamic_extent> first(size_t count) const noexcept {
    SB_CHECK_LE(count, size());
    return {data(), count};
  }

  constexpr span<T, dynamic_extent> last(size_t count) const noexcept {
    SB_CHECK_LE(count, size());
    return {data() + (size() - count), count};
  }

  constexpr span<T, dynamic_extent> subspan(
      size_t offset,
      size_t count = dynamic_extent) const noexcept {
    SB_CHECK_LE(offset, size());
    SB_CHECK(count == dynamic_extent || count <= size() - offset);
    return {data() + offset, count != dynamic_extent ? count : size() - offset};
  }

  // [span.obs], span observers
  constexpr size_t size() const noexcept { return ExtentStorage::size(); }
  constexpr size_t size_bytes() const noexcept { return size() * sizeof(T); }
  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  // [span.elem], span element access
  constexpr T& operator[](size_t idx) const noexcept {
    SB_CHECK_LT(idx, size());
    return *(data() + idx);
  }

  constexpr T& front() const noexcept {
    static_assert(Extent == dynamic_extent || Extent > 0,
                  "Extent must not be 0");
    SB_CHECK(Extent != dynamic_extent || !empty());
    return *data();
  }

  constexpr T& back() const noexcept {
    static_assert(Extent == dynamic_extent || Extent > 0,
                  "Extent must not be 0");
    SB_CHECK(Extent != dynamic_extent || !empty());
    return *(data() + size() - 1);
  }

  constexpr T* data() const noexcept { return data_; }

  // [span.iter], span iterator support
  constexpr iterator begin() const noexcept { return data_; }

  constexpr iterator end() const noexcept { return data_ + size(); }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

 private:
  T* data_;
};

// span<T, Extent>::extent can not be declared inline prior to C++17, hence this
// definition is required.
template <class T, size_t Extent>
constexpr size_t span<T, Extent>::extent;

// [span.objectrep], views of object representation
template <typename T, size_t X>
span<const uint8_t, (X == dynamic_extent ? dynamic_extent : sizeof(T) * X)>
as_bytes(span<T, X> s) noexcept {
  return {reinterpret_cast<const uint8_t*>(s.data()), s.size_bytes()};
}

template <typename T,
          size_t X,
          typename = std::enable_if_t<!std::is_const<T>::value>>
span<uint8_t, (X == dynamic_extent ? dynamic_extent : sizeof(T) * X)>
as_writable_bytes(span<T, X> s) noexcept {
  return {reinterpret_cast<uint8_t*>(s.data()), s.size_bytes()};
}

// Type-deducing helpers for constructing a span.
template <int&... ExplicitArgumentBarrier, typename It>
constexpr auto make_span(It it, size_t size) noexcept {
  using T = std::remove_reference_t<internal::iter_reference_t<It>>;
  return span<T>(it, size);
}

template <int&... ExplicitArgumentBarrier,
          typename It,
          typename End,
          typename = std::enable_if_t<!std::is_convertible_v<End, size_t>>>
constexpr auto make_span(It it, End end) noexcept {
  using T = std::remove_reference_t<internal::iter_reference_t<It>>;
  return span<T>(it, end);
}

// make_span utility function that deduces both the span's value_type and extent
// from the passed in argument.
//
// Usage: auto span = base::make_span(...);
template <int&... ExplicitArgumentBarrier, typename Container>
constexpr auto make_span(Container&& container) noexcept {
  using T =
      std::remove_pointer_t<decltype(std::data(std::declval<Container>()))>;
  using Extent = internal::Extent<Container>;
  return span<T, Extent::value>(std::forward<Container>(container));
}

// make_span utility functions that allow callers to explicit specify the span's
// extent, the value_type is deduced automatically. This is useful when passing
// a dynamically sized container to a method expecting static spans, when the
// container is known to have the correct size.
//
// Note: This will CHECK that N indeed matches size(container).
//
// Usage: auto static_span = base::make_span<N>(...);
template <size_t N, int&... ExplicitArgumentBarrier, typename It>
constexpr auto make_span(It it, size_t size) noexcept {
  using T = std::remove_reference_t<internal::iter_reference_t<It>>;
  return span<T, N>(it, size);
}

template <size_t N,
          int&... ExplicitArgumentBarrier,
          typename It,
          typename End,
          typename = std::enable_if_t<!std::is_convertible_v<End, size_t>>>
constexpr auto make_span(It it, End end) noexcept {
  using T = std::remove_reference_t<internal::iter_reference_t<It>>;
  return span<T, N>(it, end);
}

template <size_t N, int&... ExplicitArgumentBarrier, typename Container>
constexpr auto make_span(Container&& container) noexcept {
  using T =
      std::remove_pointer_t<decltype(std::data(std::declval<Container>()))>;
  return span<T, N>(std::data(container), std::size(container));
}

// Starboard's name for span.
template <typename T, size_t Extent = dynamic_extent>
using Span = span<T, Extent>;

// Helper function analogous to absl::MakeSpan or base::make_span.
template <class T>
constexpr Span<T> MakeSpan(T* data, size_t size) {
  return Span<T>(data, size);
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_SPAN_H_
