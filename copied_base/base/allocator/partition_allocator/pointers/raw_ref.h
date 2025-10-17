// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_REF_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_REF_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc_base/augmentations/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"

namespace base {

template <class T, RawPtrTraits Traits>
class raw_ref;

namespace internal {

template <class T>
struct is_raw_ref : std::false_type {};

template <class T, RawPtrTraits Traits>
struct is_raw_ref<::base::raw_ref<T, Traits>> : std::true_type {};

template <class T>
constexpr inline bool is_raw_ref_v = is_raw_ref<T>::value;

}  // namespace internal

// A smart pointer for a pointer which can not be null, and which provides
// Use-after-Free protection in the same ways as raw_ptr. This class acts like a
// combination of std::reference_wrapper and raw_ptr.
//
// See raw_ptr and //base/memory/raw_ptr.md for more details on the
// Use-after-Free protection.
//
// # Use after move
//
// The raw_ref type will abort if used after being moved.
//
// # Constness
//
// Use a `const raw_ref<T>` when the smart pointer should not be able to rebind
// to a new reference. Use a `const raw_ref<const T>` do the same for a const
// reference, which is like `const T&`.
//
// Unlike a native `T&` reference, a mutable `raw_ref<T>` can be changed
// independent of the underlying `T`, similar to `std::reference_wrapper`. That
// means the reference inside it can be moved and reassigned.
template <class T, RawPtrTraits Traits = RawPtrTraits::kEmpty>
class PA_TRIVIAL_ABI PA_GSL_POINTER raw_ref {
  // operator* is used with the expectation of GetForExtraction semantics:
  //
  // raw_ref<Foo> foo_raw_ref = something;
  // Foo& foo_ref = *foo_raw_ref;
  //
  // The implementation of operator* provides GetForDereference semantics, and
  // this results in spurious crashes in BRP-ASan builds, so we need to disable
  // hooks that provide BRP-ASan instrumentation for raw_ref.
  using Inner = raw_ptr<T, Traits | RawPtrTraits::kDisableHooks>;

  // Some underlying implementations do not clear on move, which produces an
  // inconsistent behaviour. We want consistent behaviour such that using a
  // raw_ref after move is caught and aborts, so do it when the underlying
  // implementation doesn't. Failure to clear would be indicated by the related
  // death tests not CHECKing appropriately.
  static constexpr bool kNeedClearAfterMove = !Inner::kZeroOnMove;

 public:
  using Impl = typename Inner::Impl;

  // Construct a raw_ref from a pointer, which must not be null.
  //
  // This function is safe to use with any pointer, as it will CHECK and
  // terminate the process if the pointer is null. Avoid dereferencing a pointer
  // to avoid this CHECK as you may be dereferencing null.
  PA_ALWAYS_INLINE constexpr static raw_ref from_ptr(T* ptr) noexcept {
    PA_RAW_PTR_CHECK(ptr);
    return raw_ref(*ptr);
  }

  // Construct a raw_ref from a reference.
  PA_ALWAYS_INLINE constexpr explicit raw_ref(T& p) noexcept
      : inner_(std::addressof(p)) {}

  // Assign a new reference to the raw_ref, replacing the existing reference.
  PA_ALWAYS_INLINE constexpr raw_ref& operator=(T& p) noexcept {
    inner_.operator=(&p);
    return *this;
  }

  // Disallow holding references to temporaries.
  raw_ref(const T&& p) = delete;
  raw_ref& operator=(const T&& p) = delete;

  PA_ALWAYS_INLINE constexpr raw_ref(const raw_ref& p) noexcept
      : inner_(p.inner_) {
    PA_RAW_PTR_CHECK(inner_);  // Catch use-after-move.
  }

  PA_ALWAYS_INLINE constexpr raw_ref(raw_ref&& p) noexcept
      : inner_(std::move(p.inner_)) {
    PA_RAW_PTR_CHECK(inner_);  // Catch use-after-move.
    if constexpr (kNeedClearAfterMove) {
      p.inner_ = nullptr;
    }
  }

  PA_ALWAYS_INLINE constexpr raw_ref& operator=(const raw_ref& p) noexcept {
    PA_RAW_PTR_CHECK(p.inner_);  // Catch use-after-move.
    inner_.operator=(p.inner_);
    return *this;
  }

  PA_ALWAYS_INLINE constexpr raw_ref& operator=(raw_ref&& p) noexcept {
    PA_RAW_PTR_CHECK(p.inner_);  // Catch use-after-move.
    inner_.operator=(std::move(p.inner_));
    if constexpr (kNeedClearAfterMove) {
      p.inner_ = nullptr;
    }
    return *this;
  }

  // Deliberately implicit in order to support implicit upcast.
  // Delegate cross-kind conversion to the inner raw_ptr, which decides when to
  // allow it.
  template <class U,
            RawPtrTraits PassedTraits,
            class = std::enable_if_t<std::is_convertible_v<U&, T&>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ref(const raw_ref<U, PassedTraits>& p) noexcept
      : inner_(p.inner_) {
    PA_RAW_PTR_CHECK(inner_);  // Catch use-after-move.
  }
  // Deliberately implicit in order to support implicit upcast.
  // Delegate cross-kind conversion to the inner raw_ptr, which decides when to
  // allow it.
  template <class U,
            RawPtrTraits PassedTraits,
            class = std::enable_if_t<std::is_convertible_v<U&, T&>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ref(raw_ref<U, PassedTraits>&& p) noexcept
      : inner_(std::move(p.inner_)) {
    PA_RAW_PTR_CHECK(inner_);  // Catch use-after-move.
    if constexpr (kNeedClearAfterMove) {
      p.inner_ = nullptr;
    }
  }

  // Upcast assignment
  // Delegate cross-kind conversion to the inner raw_ptr, which decides when to
  // allow it.
  template <class U,
            RawPtrTraits PassedTraits,
            class = std::enable_if_t<std::is_convertible_v<U&, T&>>>
  PA_ALWAYS_INLINE constexpr raw_ref& operator=(
      const raw_ref<U, PassedTraits>& p) noexcept {
    PA_RAW_PTR_CHECK(p.inner_);  // Catch use-after-move.
    inner_.operator=(p.inner_);
    return *this;
  }
  // Delegate cross-kind conversion to the inner raw_ptr, which decides when to
  // allow it.
  template <class U,
            RawPtrTraits PassedTraits,
            class = std::enable_if_t<std::is_convertible_v<U&, T&>>>
  PA_ALWAYS_INLINE constexpr raw_ref& operator=(
      raw_ref<U, PassedTraits>&& p) noexcept {
    PA_RAW_PTR_CHECK(p.inner_);  // Catch use-after-move.
    inner_.operator=(std::move(p.inner_));
    if constexpr (kNeedClearAfterMove) {
      p.inner_ = nullptr;
    }
    return *this;
  }

  PA_ALWAYS_INLINE constexpr T& operator*() const {
    PA_RAW_PTR_CHECK(inner_);  // Catch use-after-move.
    return inner_.operator*();
  }

  // This is an equivalent to operator*() that provides GetForExtraction rather
  // rather than GetForDereference semantics (see raw_ptr.h). This should be
  // used in place of operator*() when the memory referred to by the reference
  // is not immediately going to be accessed.
  PA_ALWAYS_INLINE constexpr T& get() const {
    PA_RAW_PTR_CHECK(inner_);  // Catch use-after-move.
    return *inner_.get();
  }

  PA_ALWAYS_INLINE constexpr T* operator->() const
      PA_ATTRIBUTE_RETURNS_NONNULL {
    PA_RAW_PTR_CHECK(inner_);  // Catch use-after-move.
    return inner_.operator->();
  }

  // This is used to verify callbacks are not invoked with dangling references.
  // If the `raw_ref` references a deleted object, it will trigger an error.
  // Depending on the PartitionAllocUnretainedDanglingPtr feature, this is
  // either a DumpWithoutCrashing, a crash, or ignored.
  PA_ALWAYS_INLINE void ReportIfDangling() const noexcept {
    inner_.ReportIfDangling();
  }

  PA_ALWAYS_INLINE friend constexpr void swap(raw_ref& lhs,
                                              raw_ref& rhs) noexcept {
    PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
    PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
    swap(lhs.inner_, rhs.inner_);
  }

  template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
  friend bool operator==(const raw_ref<U, Traits1>& lhs,
                         const raw_ref<V, Traits2>& rhs);
  template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
  friend bool operator!=(const raw_ref<U, Traits1>& lhs,
                         const raw_ref<V, Traits2>& rhs);
  template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
  friend bool operator<(const raw_ref<U, Traits1>& lhs,
                        const raw_ref<V, Traits2>& rhs);
  template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
  friend bool operator>(const raw_ref<U, Traits1>& lhs,
                        const raw_ref<V, Traits2>& rhs);
  template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
  friend bool operator<=(const raw_ref<U, Traits1>& lhs,
                         const raw_ref<V, Traits2>& rhs);
  template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
  friend bool operator>=(const raw_ref<U, Traits1>& lhs,
                         const raw_ref<V, Traits2>& rhs);

  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator==(const raw_ref& lhs, const U& rhs) {
    PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
    return lhs.inner_ == &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator!=(const raw_ref& lhs, const U& rhs) {
    PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
    return lhs.inner_ != &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator<(const raw_ref& lhs, const U& rhs) {
    PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
    return lhs.inner_ < &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator>(const raw_ref& lhs, const U& rhs) {
    PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
    return lhs.inner_ > &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator<=(const raw_ref& lhs, const U& rhs) {
    PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
    return lhs.inner_ <= &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator>=(const raw_ref& lhs, const U& rhs) {
    PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
    return lhs.inner_ >= &rhs;
  }

  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator==(const U& lhs, const raw_ref& rhs) {
    PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
    return &lhs == rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator!=(const U& lhs, const raw_ref& rhs) {
    PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
    return &lhs != rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator<(const U& lhs, const raw_ref& rhs) {
    PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
    return &lhs < rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator>(const U& lhs, const raw_ref& rhs) {
    PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
    return &lhs > rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator<=(const U& lhs, const raw_ref& rhs) {
    PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
    return &lhs <= rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  PA_ALWAYS_INLINE friend bool operator>=(const U& lhs, const raw_ref& rhs) {
    PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
    return &lhs >= rhs.inner_;
  }

 private:
  template <class U, RawPtrTraits R>
  friend class raw_ref;

  Inner inner_;
};

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator==(const raw_ref<U, Traits1>& lhs,
                                 const raw_ref<V, Traits2>& rhs) {
  PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
  PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
  return lhs.inner_ == rhs.inner_;
}
template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator!=(const raw_ref<U, Traits1>& lhs,
                                 const raw_ref<V, Traits2>& rhs) {
  PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
  PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
  return lhs.inner_ != rhs.inner_;
}
template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator<(const raw_ref<U, Traits1>& lhs,
                                const raw_ref<V, Traits2>& rhs) {
  PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
  PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
  return lhs.inner_ < rhs.inner_;
}
template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator>(const raw_ref<U, Traits1>& lhs,
                                const raw_ref<V, Traits2>& rhs) {
  PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
  PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
  return lhs.inner_ > rhs.inner_;
}
template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator<=(const raw_ref<U, Traits1>& lhs,
                                 const raw_ref<V, Traits2>& rhs) {
  PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
  PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
  return lhs.inner_ <= rhs.inner_;
}
template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator>=(const raw_ref<U, Traits1>& lhs,
                                 const raw_ref<V, Traits2>& rhs) {
  PA_RAW_PTR_CHECK(lhs.inner_);  // Catch use-after-move.
  PA_RAW_PTR_CHECK(rhs.inner_);  // Catch use-after-move.
  return lhs.inner_ >= rhs.inner_;
}

// CTAD deduction guide.
template <class T>
raw_ref(T&) -> raw_ref<T>;
template <class T>
raw_ref(const T&) -> raw_ref<const T>;

// Template helpers for working with raw_ref<T>.
template <typename T>
struct IsRawRef : std::false_type {};

template <typename T, RawPtrTraits Traits>
struct IsRawRef<raw_ref<T, Traits>> : std::true_type {};

template <typename T>
inline constexpr bool IsRawRefV = IsRawRef<T>::value;

template <typename T>
struct RemoveRawRef {
  using type = T;
};

template <typename T, RawPtrTraits Traits>
struct RemoveRawRef<raw_ref<T, Traits>> {
  using type = T;
};

template <typename T>
using RemoveRawRefT = typename RemoveRawRef<T>::type;

}  // namespace base

using base::raw_ref;

namespace std {

// Override so set/map lookups do not create extra raw_ref. This also
// allows C++ references to be used for lookup.
template <typename T, base::RawPtrTraits Traits>
struct less<raw_ref<T, Traits>> {
  using Impl = typename raw_ref<T, Traits>::Impl;
  using is_transparent = void;

  bool operator()(const raw_ref<T, Traits>& lhs,
                  const raw_ref<T, Traits>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(T& lhs, const raw_ref<T, Traits>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(const raw_ref<T, Traits>& lhs, T& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }
};

#if defined(_LIBCPP_VERSION)
// Specialize std::pointer_traits. The latter is required to obtain the
// underlying raw pointer in the std::to_address(pointer) overload.
// Implementing the pointer_traits is the standard blessed way to customize
// `std::to_address(pointer)` in C++20 [3].
//
// [1] https://wg21.link/pointer.traits.optmem

template <typename T, ::base::RawPtrTraits Traits>
struct pointer_traits<::raw_ref<T, Traits>> {
  using pointer = ::raw_ref<T, Traits>;
  using element_type = T;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = ::raw_ref<U, Traits>;

  static constexpr pointer pointer_to(element_type& r) noexcept {
    return pointer(r);
  }

  static constexpr element_type* to_address(pointer p) noexcept {
    // `raw_ref::get` is used instead of raw_ref::operator*`. It provides
    // GetForExtraction rather rather than GetForDereference semantics (see
    // raw_ptr.h). This should be used when we we don't know the memory will be
    // accessed.
    return &(p.get());
  }
};
#endif  // defined(_LIBCPP_VERSION)

}  // namespace std

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_REF_H_
