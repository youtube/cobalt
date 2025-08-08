// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_H_

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include "base/allocator/partition_allocator/src/partition_alloc/flags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/cxx20_is_constant_evaluated.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_config.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_exclusion.h"
#include "base/allocator/partition_allocator/src/partition_alloc/raw_ptr_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_WIN)
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/win/win_handle_types.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/check.h"
// Live implementation of MiraclePtr being built.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#define PA_RAW_PTR_CHECK(condition) PA_BASE_CHECK(condition)
#else
// No-op implementation of MiraclePtr being built.
// Note that `PA_BASE_DCHECK()` evaporates from non-DCHECK builds,
// minimizing impact of generated code.
#define PA_RAW_PTR_CHECK(condition) PA_BASE_DCHECK(condition)
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#else   // BUILDFLAG(USE_PARTITION_ALLOC)
// Without PartitionAlloc, there's no `PA_BASE_D?CHECK()` implementation
// available.
#define PA_RAW_PTR_CHECK(condition)
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#include "base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_backup_ref_impl.h"
#elif BUILDFLAG(USE_ASAN_UNOWNED_PTR)
#include "base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_asan_unowned_impl.h"
#elif BUILDFLAG(USE_HOOKABLE_RAW_PTR)
#include "base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_hookable_impl.h"
#else
#include "base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_noop_impl.h"
#endif

namespace cc {
class Scheduler;
}
namespace base::internal {
class DelayTimerBase;
}
namespace content::responsiveness {
class Calculator;
}

namespace partition_alloc::internal {

// NOTE: All methods should be `PA_ALWAYS_INLINE`. raw_ptr is meant to be a
// lightweight replacement of a raw pointer, hence performance is critical.

// This is a bitfield representing the different flags that can be applied to a
// raw_ptr.
//
// Internal use only: Developers shouldn't use those values directly.
//
// Housekeeping rules: Try not to change trait values, so that numeric trait
// values stay constant across builds (could be useful e.g. when analyzing stack
// traces). A reasonable exception to this rule are `*ForTest` traits. As a
// matter of fact, we propose that new non-test traits are added before the
// `*ForTest` traits.
enum class RawPtrTraits : unsigned {
  kEmpty = 0,

  // Disables dangling pointer detection, but keeps other raw_ptr protections.
  //
  // Don't use directly, use DisableDanglingPtrDetection or DanglingUntriaged
  // instead.
  kMayDangle = (1 << 0),

  // Disables any hooks, when building with BUILDFLAG(USE_HOOKABLE_RAW_PTR).
  //
  // Internal use only.
  kDisableHooks = (1 << 2),

  // Pointer arithmetic is discouraged and disabled by default.
  //
  // Don't use directly, use AllowPtrArithmetic instead.
  kAllowPtrArithmetic = (1 << 3),

  // This pointer is evaluated by a separate, Ash-related experiment.
  //
  // Don't use directly, use ExperimentalAsh instead.
  kExperimentalAsh = (1 << 4),

  // Uninitialized pointers are discouraged and disabled by default.
  //
  // Don't use directly, use AllowUninitialized instead.
  kAllowUninitialized = (1 << 5),

  // *** ForTest traits below ***

  // Adds accounting, on top of the NoOp implementation, for test purposes.
  // raw_ptr/raw_ref with this trait perform extra bookkeeping, e.g. to track
  // the number of times the raw_ptr is wrapped, unwrapped, etc.
  //
  // Test only. Include raw_ptr_counting_impl_for_test.h in your test
  // files when using this trait.
  kUseCountingImplForTest = (1 << 10),

  // Helper trait that can be used to test raw_ptr's behaviour or conversions.
  //
  // Test only.
  kDummyForTest = (1 << 11),

  kAllMask = kMayDangle | kDisableHooks | kAllowPtrArithmetic |
             kExperimentalAsh | kAllowUninitialized | kUseCountingImplForTest |
             kDummyForTest,
};
// Template specialization to use |PA_DEFINE_OPERATORS_FOR_FLAGS| without
// |kMaxValue| declaration.
template <>
constexpr inline RawPtrTraits kAllFlags<RawPtrTraits> = RawPtrTraits::kAllMask;
PA_DEFINE_OPERATORS_FOR_FLAGS(RawPtrTraits);

}  // namespace partition_alloc::internal

namespace base {
using partition_alloc::internal::RawPtrTraits;

namespace raw_ptr_traits {

// IsSupportedType<T>::value answers whether raw_ptr<T> 1) compiles and 2) is
// always safe at runtime.  Templates that may end up using `raw_ptr<T>` should
// use IsSupportedType to ensure that raw_ptr is not used with unsupported
// types.  As an example, see how base::internal::StorageTraits uses
// IsSupportedType as a condition for using base::internal::UnretainedWrapper
// (which has a `ptr_` field that will become `raw_ptr<T>` after the Big
// Rewrite).
template <typename T, typename SFINAE = void>
struct IsSupportedType {
  static constexpr bool value = true;
};

// raw_ptr<T> is not compatible with function pointer types. Also, they don't
// even need the raw_ptr protection, because they don't point on heap.
template <typename T>
struct IsSupportedType<T, std::enable_if_t<std::is_function_v<T>>> {
  static constexpr bool value = false;
};

// This section excludes some types from raw_ptr<T> to avoid them from being
// used inside base::Unretained in performance sensitive places. These were
// identified from sampling profiler data. See crbug.com/1287151 for more info.
template <>
struct IsSupportedType<cc::Scheduler> {
  static constexpr bool value = false;
};
template <>
struct IsSupportedType<base::internal::DelayTimerBase> {
  static constexpr bool value = false;
};
template <>
struct IsSupportedType<content::responsiveness::Calculator> {
  static constexpr bool value = false;
};

#if __OBJC__
// raw_ptr<T> is not compatible with pointers to Objective-C classes for a
// multitude of reasons. They may fail to compile in many cases, and wouldn't
// work well with tagged pointers. Anyway, Objective-C objects have their own
// way of tracking lifespan, hence don't need the raw_ptr protection as much.
//
// Such pointers are detected by checking if they're convertible to |id| type.
template <typename T>
struct IsSupportedType<T, std::enable_if_t<std::is_convertible_v<T*, id>>> {
  static constexpr bool value = false;
};
#endif  // __OBJC__

#if BUILDFLAG(IS_WIN)
// raw_ptr<HWND__> is unsafe at runtime - if the handle happens to also
// represent a valid pointer into a PartitionAlloc-managed region then it can
// lead to manipulating random memory when treating it as BackupRefPtr
// ref-count.  See also https://crbug.com/1262017.
//
// TODO(https://crbug.com/1262017): Cover other handle types like HANDLE,
// HLOCAL, HINTERNET, or HDEVINFO.  Maybe we should avoid using raw_ptr<T> when
// T=void (as is the case in these handle types).  OTOH, explicit,
// non-template-based raw_ptr<void> should be allowed.  Maybe this can be solved
// by having 2 traits: IsPointeeAlwaysSafe (to be used in templates) and
// IsPointeeUsuallySafe (to be used in the static_assert in raw_ptr).  The
// upside of this approach is that it will safely handle base::Bind closing over
// HANDLE.  The downside of this approach is that base::Bind closing over a
// void* pointer will not get UaF protection.
#define PA_WINDOWS_HANDLE_TYPE(name)       \
  template <>                              \
  struct IsSupportedType<name##__, void> { \
    static constexpr bool value = false;   \
  };
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/win/win_handle_types_list.inc"
#undef PA_WINDOWS_HANDLE_TYPE
#endif

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
template <RawPtrTraits Traits>
using UnderlyingImplForTraits = internal::RawPtrBackupRefImpl<
    /*AllowDangling=*/ContainsFlags(Traits, RawPtrTraits::kMayDangle),
    /*ExperimentalAsh=*/ContainsFlags(Traits, RawPtrTraits::kExperimentalAsh)>;

#elif BUILDFLAG(USE_ASAN_UNOWNED_PTR)
template <RawPtrTraits Traits>
using UnderlyingImplForTraits = internal::RawPtrAsanUnownedImpl<
    ContainsFlags(Traits, RawPtrTraits::kAllowPtrArithmetic),
    ContainsFlags(Traits, RawPtrTraits::kMayDangle)>;

#elif BUILDFLAG(USE_HOOKABLE_RAW_PTR)
template <RawPtrTraits Traits>
using UnderlyingImplForTraits = internal::RawPtrHookableImpl<
    /*EnableHooks=*/!ContainsFlags(Traits, RawPtrTraits::kDisableHooks)>;

#else
template <RawPtrTraits Traits>
using UnderlyingImplForTraits = internal::RawPtrNoOpImpl;
#endif

constexpr bool IsPtrArithmeticAllowed(RawPtrTraits Traits) {
#if BUILDFLAG(ENABLE_POINTER_ARITHMETIC_TRAIT_CHECK)
  return ContainsFlags(Traits, RawPtrTraits::kAllowPtrArithmetic);
#else
  return true;
#endif
}

}  // namespace raw_ptr_traits

namespace test {

struct RawPtrCountingImplForTest;

}  // namespace test

namespace raw_ptr_traits {

// ImplForTraits is the struct that implements raw_ptr functions. Think of
// raw_ptr as a thin wrapper, that directs calls to ImplForTraits. ImplForTraits
// may be different from UnderlyingImplForTraits, because it may select a
// test impl instead.
template <RawPtrTraits Traits>
using ImplForTraits =
    std::conditional_t<ContainsFlags(Traits,
                                     RawPtrTraits::kUseCountingImplForTest),
                       test::RawPtrCountingImplForTest,
                       UnderlyingImplForTraits<Traits>>;

}  // namespace raw_ptr_traits

// `raw_ptr<T>` is a non-owning smart pointer that has improved memory-safety
// over raw pointers. See the documentation for details:
// https://source.chromium.org/chromium/chromium/src/+/main:base/memory/raw_ptr.md
//
// raw_ptr<T> is marked as [[gsl::Pointer]] which allows the compiler to catch
// some bugs where the raw_ptr holds a dangling pointer to a temporary object.
// However the [[gsl::Pointer]] analysis expects that such types do not have a
// non-default move constructor/assignment. Thus, it's possible to get an error
// where the pointer is not actually dangling, and have to work around the
// compiler. We have not managed to construct such an example in Chromium yet.
template <typename T, RawPtrTraits Traits = RawPtrTraits::kEmpty>
class PA_TRIVIAL_ABI PA_GSL_POINTER raw_ptr {
 public:
  using Impl = typename raw_ptr_traits::ImplForTraits<Traits>;
  // Needed to make gtest Pointee matcher work with raw_ptr.
  using element_type = T;
  using DanglingType = raw_ptr<T, Traits | RawPtrTraits::kMayDangle>;

#if !BUILDFLAG(USE_PARTITION_ALLOC)
  // See comment at top about `PA_RAW_PTR_CHECK()`.
  static_assert(std::is_same_v<Impl, internal::RawPtrNoOpImpl>);
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC)

  static_assert(AreValidFlags(Traits), "Unknown raw_ptr trait(s)");
  static_assert(raw_ptr_traits::IsSupportedType<T>::value,
                "raw_ptr<T> doesn't work with this kind of pointee type T");

  static constexpr bool kZeroOnConstruct =
      Impl::kMustZeroOnConstruct ||
      (BUILDFLAG(RAW_PTR_ZERO_ON_CONSTRUCT) &&
       !ContainsFlags(Traits, RawPtrTraits::kAllowUninitialized));
  static constexpr bool kZeroOnMove =
      Impl::kMustZeroOnMove || BUILDFLAG(RAW_PTR_ZERO_ON_MOVE);
  static constexpr bool kZeroOnDestruct =
      Impl::kMustZeroOnDestruct || BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT);

// A non-trivial default ctor is required for complex implementations (e.g.
// BackupRefPtr), or even for NoOpImpl when zeroing is requested.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||                           \
    BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR) || \
    BUILDFLAG(RAW_PTR_ZERO_ON_CONSTRUCT)
  PA_ALWAYS_INLINE constexpr raw_ptr() noexcept {
    if constexpr (kZeroOnConstruct) {
      wrapped_ptr_ = nullptr;
    }
  }
#else
  // raw_ptr can be trivially default constructed (leaving |wrapped_ptr_|
  // uninitialized).
  PA_ALWAYS_INLINE constexpr raw_ptr() noexcept = default;
  static_assert(!kZeroOnConstruct);
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR) ||
        // BUILDFLAG(RAW_PTR_ZERO_ON_CONSTRUCT)

// A non-trivial copy ctor and assignment operator are required for complex
// implementations (e.g. BackupRefPtr). Unlike the blocks around, we don't need
// these for NoOpImpl even when zeroing is requested; better to keep them
// trivial.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR)
  PA_ALWAYS_INLINE constexpr raw_ptr(const raw_ptr& p) noexcept
      : wrapped_ptr_(Impl::Duplicate(p.wrapped_ptr_)) {}
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(const raw_ptr& p) noexcept {
    // Duplicate before releasing, in case the pointer is assigned to itself.
    //
    // Unlike the move version of this operator, don't add |this != &p| branch,
    // for performance reasons. Even though Duplicate() is not cheap, we
    // practically never assign a raw_ptr<T> to itself. We suspect that a
    // cumulative cost of a conditional branch, even if always correctly
    // predicted, would exceed that.
    T* new_ptr = Impl::Duplicate(p.wrapped_ptr_);
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = new_ptr;
    return *this;
  }
#else
  PA_ALWAYS_INLINE raw_ptr(const raw_ptr&) noexcept = default;
  PA_ALWAYS_INLINE raw_ptr& operator=(const raw_ptr&) noexcept = default;
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR)

// A non-trivial move ctor and assignment operator are required for complex
// implementations (e.g. BackupRefPtr), or even for NoOpImpl when zeroing is
// requested.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||                           \
    BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR) || \
    BUILDFLAG(RAW_PTR_ZERO_ON_MOVE)
  PA_ALWAYS_INLINE constexpr raw_ptr(raw_ptr&& p) noexcept {
    wrapped_ptr_ = p.wrapped_ptr_;
    if constexpr (kZeroOnMove) {
      p.wrapped_ptr_ = nullptr;
    }
  }
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(raw_ptr&& p) noexcept {
    // Unlike the the copy version of this operator, this branch is necessary
    // for correctness.
    if (PA_LIKELY(this != &p)) {
      Impl::ReleaseWrappedPtr(wrapped_ptr_);
      wrapped_ptr_ = p.wrapped_ptr_;
      if constexpr (kZeroOnMove) {
        p.wrapped_ptr_ = nullptr;
      }
    }
    return *this;
  }
#else
  PA_ALWAYS_INLINE raw_ptr(raw_ptr&&) noexcept = default;
  PA_ALWAYS_INLINE raw_ptr& operator=(raw_ptr&&) noexcept = default;
  static_assert(!kZeroOnMove);
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR) ||
        // BUILDFLAG(RAW_PTR_ZERO_ON_MOVE)

// A non-trivial default dtor is required for complex implementations (e.g.
// BackupRefPtr), or even for NoOpImpl when zeroing is requested.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||                           \
    BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR) || \
    BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT)
  PA_ALWAYS_INLINE PA_CONSTEXPR_DTOR ~raw_ptr() noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    // Work around external issues where raw_ptr is used after destruction.
    if constexpr (kZeroOnDestruct) {
      wrapped_ptr_ = nullptr;
    }
  }
#else
  PA_ALWAYS_INLINE ~raw_ptr() noexcept = default;
  static_assert(!kZeroOnDestruct);
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR) ||
        // BUILDFLAG(RAW_PTR_ZERO_ON_DESTRUCT)

  // Cross-kind copy constructor.
  // Move is not supported as different traits may use different ref-counts, so
  // let move operations degrade to copy, which handles it well.
  template <RawPtrTraits PassedTraits,
            typename Unused = std::enable_if_t<Traits != PassedTraits>>
  PA_ALWAYS_INLINE constexpr explicit raw_ptr(
      const raw_ptr<T, PassedTraits>& p) noexcept
      : wrapped_ptr_(Impl::WrapRawPtrForDuplication(
            raw_ptr_traits::ImplForTraits<PassedTraits>::
                UnsafelyUnwrapPtrForDuplication(p.wrapped_ptr_))) {
    // Limit cross-kind conversions only to cases where kMayDangle gets added,
    // because that's needed for Unretained(Ref)Wrapper. Use a static_assert,
    // instead of disabling via SFINAE, so that the compiler catches other
    // conversions. Otherwise implicit raw_ptr<T> -> T* -> raw_ptr<> route will
    // be taken.
    static_assert(Traits == (PassedTraits | RawPtrTraits::kMayDangle));
  }

  // Cross-kind assignment.
  // Move is not supported as different traits may use different ref-counts, so
  // let move operations degrade to copy, which handles it well.
  template <RawPtrTraits PassedTraits,
            typename Unused = std::enable_if_t<Traits != PassedTraits>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(
      const raw_ptr<T, PassedTraits>& p) noexcept {
    // Limit cross-kind assignments only to cases where `kMayDangle` gets added,
    // because that's needed for Unretained(Ref)Wrapper. Use a static_assert,
    // instead of disabling via SFINAE, so that the compiler catches other
    // conversions. Otherwise implicit raw_ptr<T> -> T* -> raw_ptr<> route will
    // be taken.
    static_assert(Traits == (PassedTraits | RawPtrTraits::kMayDangle));

    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtrForDuplication(
        raw_ptr_traits::ImplForTraits<
            PassedTraits>::UnsafelyUnwrapPtrForDuplication(p.wrapped_ptr_));
    return *this;
  }

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // Ignore kZeroOnConstruct, because here the caller explicitly wishes to
  // initialize with nullptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ptr(std::nullptr_t) noexcept
      : wrapped_ptr_(nullptr) {}

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ptr(T* p) noexcept
      : wrapped_ptr_(Impl::WrapRawPtr(p)) {}

  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible_v<U*, T*> &&
                !std::is_void_v<typename std::remove_cv<T>::type>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ptr(const raw_ptr<U, Traits>& ptr) noexcept
      : wrapped_ptr_(
            Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_))) {}
  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible_v<U*, T*> &&
                !std::is_void_v<typename std::remove_cv<T>::type>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ptr(raw_ptr<U, Traits>&& ptr) noexcept
      : wrapped_ptr_(Impl::template Upcast<T, U>(ptr.wrapped_ptr_)) {
    if constexpr (kZeroOnMove) {
      ptr.wrapped_ptr_ = nullptr;
    }
  }

  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(std::nullptr_t) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = nullptr;
    return *this;
  }
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(T* p) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtr(p);
    return *this;
  }

  // Upcast assignment
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible_v<U*, T*> &&
                !std::is_void_v<typename std::remove_cv<T>::type>>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(
      const raw_ptr<U, Traits>& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at raw_ptr address,
    // not its contained pointer value). The comparison is only needed when they
    // are the same type, otherwise they can't be the same raw_ptr object.
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if constexpr (std::is_same_v<raw_ptr, std::decay_t<decltype(ptr)>>) {
      PA_RAW_PTR_CHECK(this != &ptr);
    }
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ =
        Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_));
    return *this;
  }
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible_v<U*, T*> &&
                !std::is_void_v<typename std::remove_cv<T>::type>>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(
      raw_ptr<U, Traits>&& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at raw_ptr address,
    // not its contained pointer value). The comparison is only needed when they
    // are the same type, otherwise they can't be the same raw_ptr object.
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if constexpr (std::is_same_v<raw_ptr, std::decay_t<decltype(ptr)>>) {
      PA_RAW_PTR_CHECK(this != &ptr);
    }
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::template Upcast<T, U>(ptr.wrapped_ptr_);
    if constexpr (kZeroOnMove) {
      ptr.wrapped_ptr_ = nullptr;
    }
    return *this;
  }

  // Avoid using. The goal of raw_ptr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  PA_ALWAYS_INLINE constexpr T* get() const { return GetForExtraction(); }

  // You may use |raw_ptr<T>::AsEphemeralRawAddr()| to obtain |T**| or |T*&|
  // from |raw_ptr<T>|, as long as you follow these requirements:
  // - DO NOT carry T**/T*& obtained via AsEphemeralRawAddr() out of
  //   expression.
  // - DO NOT use raw_ptr or T**/T*& multiple times within an expression.
  //
  // https://chromium.googlesource.com/chromium/src/+/main/base/memory/raw_ptr.md#in_out-arguments-need-to-be-refactored
  class EphemeralRawAddr {
   public:
    EphemeralRawAddr(const EphemeralRawAddr&) = delete;
    EphemeralRawAddr& operator=(const EphemeralRawAddr&) = delete;
    void* operator new(size_t) = delete;
    void* operator new(size_t, void*) = delete;
    PA_ALWAYS_INLINE PA_CONSTEXPR_DTOR ~EphemeralRawAddr() { original = copy; }

    PA_ALWAYS_INLINE constexpr T** operator&() && { return &copy; }
    // NOLINTNEXTLINE(google-explicit-constructor)
    PA_ALWAYS_INLINE constexpr operator T*&() && { return copy; }

   private:
    friend class raw_ptr;
    PA_ALWAYS_INLINE constexpr explicit EphemeralRawAddr(raw_ptr& ptr)
        : copy(ptr.get()), original(ptr) {}
    T* copy;
    raw_ptr& original;  // Original pointer.
  };
  PA_ALWAYS_INLINE PA_CONSTEXPR_DTOR EphemeralRawAddr AsEphemeralRawAddr() & {
    return EphemeralRawAddr(*this);
  }

  PA_ALWAYS_INLINE constexpr explicit operator bool() const {
    return !!wrapped_ptr_;
  }

  template <typename U = T,
            typename Unused = std::enable_if_t<
                !std::is_void_v<typename std::remove_cv<U>::type>>>
  PA_ALWAYS_INLINE constexpr U& operator*() const {
    return *GetForDereference();
  }
  PA_ALWAYS_INLINE constexpr T* operator->() const {
    return GetForDereference();
  }

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr operator T*() const { return GetForExtraction(); }
  template <typename U>
  PA_ALWAYS_INLINE constexpr explicit operator U*() const {
    // This operator may be invoked from static_cast, meaning the types may not
    // be implicitly convertible, hence the need for static_cast here.
    return static_cast<U*>(GetForExtraction());
  }

  PA_ALWAYS_INLINE constexpr raw_ptr& operator++() {
    static_assert(
        raw_ptr_traits::IsPtrArithmeticAllowed(Traits),
        "cannot increment raw_ptr unless AllowPtrArithmetic trait is present.");
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, 1);
    return *this;
  }
  PA_ALWAYS_INLINE constexpr raw_ptr& operator--() {
    static_assert(
        raw_ptr_traits::IsPtrArithmeticAllowed(Traits),
        "cannot decrement raw_ptr unless AllowPtrArithmetic trait is present.");
    wrapped_ptr_ = Impl::Retreat(wrapped_ptr_, 1);
    return *this;
  }
  PA_ALWAYS_INLINE constexpr raw_ptr operator++(int /* post_increment */) {
    static_assert(
        raw_ptr_traits::IsPtrArithmeticAllowed(Traits),
        "cannot increment raw_ptr unless AllowPtrArithmetic trait is present.");
    raw_ptr result = *this;
    ++(*this);
    return result;
  }
  PA_ALWAYS_INLINE constexpr raw_ptr operator--(int /* post_decrement */) {
    static_assert(
        raw_ptr_traits::IsPtrArithmeticAllowed(Traits),
        "cannot decrement raw_ptr unless AllowPtrArithmetic trait is present.");
    raw_ptr result = *this;
    --(*this);
    return result;
  }
  template <
      typename Z,
      typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator+=(Z delta_elems) {
    static_assert(
        raw_ptr_traits::IsPtrArithmeticAllowed(Traits),
        "cannot increment raw_ptr unless AllowPtrArithmetic trait is present.");
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, delta_elems);
    return *this;
  }
  template <
      typename Z,
      typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator-=(Z delta_elems) {
    static_assert(
        raw_ptr_traits::IsPtrArithmeticAllowed(Traits),
        "cannot increment raw_ptr unless AllowPtrArithmetic trait is present.");
    wrapped_ptr_ = Impl::Retreat(wrapped_ptr_, delta_elems);
    return *this;
  }

  template <typename Z,
            typename U = T,
            RawPtrTraits CopyTraits = Traits,
            typename Unused = std::enable_if_t<
                !std::is_void_v<typename std::remove_cv<U>::type> &&
                partition_alloc::internal::is_offset_type<Z>>>
  U& operator[](Z delta_elems) const {
    static_assert(
        raw_ptr_traits::IsPtrArithmeticAllowed(Traits),
        "cannot index raw_ptr unless AllowPtrArithmetic trait is present.");
    return wrapped_ptr_[delta_elems];
  }

  // Do not disable operator+() and operator-().
  // They provide OOB checks, which prevent from assigning an arbitrary value to
  // raw_ptr, leading BRP to modifying arbitrary memory thinking it's ref-count.
  // Keep them enabled, which may be blocked later when attempting to apply the
  // += or -= operation, when disabled. In the absence of operators +/-, the
  // compiler is free to implicitly convert to the underlying T* representation
  // and perform ordinary pointer arithmetic, thus invalidating the purpose
  // behind disabling them.
  template <typename Z>
  PA_ALWAYS_INLINE friend constexpr raw_ptr operator+(const raw_ptr& p,
                                                      Z delta_elems) {
    raw_ptr result = p;
    return result += delta_elems;
  }
  template <typename Z>
  PA_ALWAYS_INLINE friend constexpr raw_ptr operator-(const raw_ptr& p,
                                                      Z delta_elems) {
    raw_ptr result = p;
    return result -= delta_elems;
  }

  PA_ALWAYS_INLINE friend constexpr ptrdiff_t operator-(const raw_ptr& p1,
                                                        const raw_ptr& p2) {
    return Impl::GetDeltaElems(p1.wrapped_ptr_, p2.wrapped_ptr_);
  }
  PA_ALWAYS_INLINE friend constexpr ptrdiff_t operator-(T* p1,
                                                        const raw_ptr& p2) {
    return Impl::GetDeltaElems(p1, p2.wrapped_ptr_);
  }
  PA_ALWAYS_INLINE friend constexpr ptrdiff_t operator-(const raw_ptr& p1,
                                                        T* p2) {
    return Impl::GetDeltaElems(p1.wrapped_ptr_, p2);
  }

  // Stop referencing the underlying pointer and free its memory. Compared to
  // raw delete calls, this avoids the raw_ptr to be temporarily dangling
  // during the free operation, which will lead to taking the slower path that
  // involves quarantine.
  PA_ALWAYS_INLINE constexpr void ClearAndDelete() noexcept {
    delete GetForExtractionAndReset();
  }
  PA_ALWAYS_INLINE constexpr void ClearAndDeleteArray() noexcept {
    delete[] GetForExtractionAndReset();
  }

  // Clear the underlying pointer and return another raw_ptr instance
  // that is allowed to dangle.
  // This can be useful in cases such as:
  // ```
  //  ptr.ExtractAsDangling()->SelfDestroy();
  // ```
  // ```
  //  c_style_api_do_something_and_destroy(ptr.ExtractAsDangling());
  // ```
  // NOTE, avoid using this method as it indicates an error-prone memory
  // ownership pattern. If possible, use smart pointers like std::unique_ptr<>
  // instead of raw_ptr<>.
  // If you have to use it, avoid saving the return value in a long-lived
  // variable (or worse, a field)! It's meant to be used as a temporary, to be
  // passed into a cleanup & freeing function, and destructed at the end of the
  // statement.
  PA_ALWAYS_INLINE constexpr DanglingType ExtractAsDangling() noexcept {
    DanglingType res(std::move(*this));
    // Not all implementation clear the source pointer on move. Furthermore,
    // even for implemtantions that do, cross-kind conversions (that add
    // kMayDangle) fall back to a copy, instead of move. So do it here just in
    // case. Should be cheap.
    operator=(nullptr);
    return res;
  }

  // Comparison operators between raw_ptr and raw_ptr<U>/U*/std::nullptr_t.
  // Strictly speaking, it is not necessary to provide these: the compiler can
  // use the conversion operator implicitly to allow comparisons to fall back to
  // comparisons between raw pointers. However, `operator T*`/`operator U*` may
  // perform safety checks with a higher runtime cost, so to avoid this, provide
  // explicit comparison operators for all combinations of parameters.

  // Comparisons between `raw_ptr`s. This unusual declaration and separate
  // definition below is because `GetForComparison()` is a private method. The
  // more conventional approach of defining a comparison operator between
  // `raw_ptr` and `raw_ptr<U>` in the friend declaration itself does not work,
  // because a comparison operator defined inline would not be allowed to call
  // `raw_ptr<U>`'s private `GetForComparison()` method.
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator==(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator!=(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator<(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator>(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator<=(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator>=(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);

  // Comparisons with U*. These operators also handle the case where the RHS is
  // T*.
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator==(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() == rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator!=(const raw_ptr& lhs, U* rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator==(U* lhs, const raw_ptr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator!=(U* lhs, const raw_ptr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator<(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() < rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator<=(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() <= rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator>(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() > rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator>=(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() >= rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator<(U* lhs, const raw_ptr& rhs) {
    return lhs < rhs.GetForComparison();
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator<=(U* lhs, const raw_ptr& rhs) {
    return lhs <= rhs.GetForComparison();
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator>(U* lhs, const raw_ptr& rhs) {
    return lhs > rhs.GetForComparison();
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator>=(U* lhs, const raw_ptr& rhs) {
    return lhs >= rhs.GetForComparison();
  }

  // Comparisons with `std::nullptr_t`.
  PA_ALWAYS_INLINE friend bool operator==(const raw_ptr& lhs, std::nullptr_t) {
    return !lhs;
  }
  PA_ALWAYS_INLINE friend bool operator!=(const raw_ptr& lhs, std::nullptr_t) {
    return !!lhs;  // Use !! otherwise the costly implicit cast will be used.
  }
  PA_ALWAYS_INLINE friend bool operator==(std::nullptr_t, const raw_ptr& rhs) {
    return !rhs;
  }
  PA_ALWAYS_INLINE friend bool operator!=(std::nullptr_t, const raw_ptr& rhs) {
    return !!rhs;  // Use !! otherwise the costly implicit cast will be used.
  }

  PA_ALWAYS_INLINE friend constexpr void swap(raw_ptr& lhs,
                                              raw_ptr& rhs) noexcept {
    Impl::IncrementSwapCountForTest();
    std::swap(lhs.wrapped_ptr_, rhs.wrapped_ptr_);
  }

  PA_ALWAYS_INLINE void ReportIfDangling() const noexcept {
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    Impl::ReportIfDangling(wrapped_ptr_);
#endif
  }

 private:
  // This getter is meant for situations where the pointer is meant to be
  // dereferenced. It is allowed to crash on nullptr (it may or may not),
  // because it knows that the caller will crash on nullptr.
  PA_ALWAYS_INLINE constexpr T* GetForDereference() const {
    return Impl::SafelyUnwrapPtrForDereference(wrapped_ptr_);
  }
  // This getter is meant for situations where the raw pointer is meant to be
  // extracted outside of this class, but not necessarily with an intention to
  // dereference. It mustn't crash on nullptr.
  PA_ALWAYS_INLINE constexpr T* GetForExtraction() const {
    return Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_);
  }
  // This getter is meant *only* for situations where the pointer is meant to be
  // compared (guaranteeing no dereference or extraction outside of this class).
  // Any verifications can and should be skipped for performance reasons.
  PA_ALWAYS_INLINE constexpr T* GetForComparison() const {
    return Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_);
  }

  PA_ALWAYS_INLINE constexpr T* GetForExtractionAndReset() {
    T* ptr = GetForExtraction();
    operator=(nullptr);
    return ptr;
  }

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #union, #global-scope, #constexpr-ctor-field-initializer
  RAW_PTR_EXCLUSION T* wrapped_ptr_;

  template <typename U, base::RawPtrTraits R>
  friend class raw_ptr;
};

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator==(const raw_ptr<U, Traits1>& lhs,
                                 const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() == rhs.GetForComparison();
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator!=(const raw_ptr<U, Traits1>& lhs,
                                 const raw_ptr<V, Traits2>& rhs) {
  return !(lhs == rhs);
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator<(const raw_ptr<U, Traits1>& lhs,
                                const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() < rhs.GetForComparison();
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator>(const raw_ptr<U, Traits1>& lhs,
                                const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() > rhs.GetForComparison();
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator<=(const raw_ptr<U, Traits1>& lhs,
                                 const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() <= rhs.GetForComparison();
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator>=(const raw_ptr<U, Traits1>& lhs,
                                 const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() >= rhs.GetForComparison();
}

template <typename T>
struct IsRawPtr : std::false_type {};

template <typename T, RawPtrTraits Traits>
struct IsRawPtr<raw_ptr<T, Traits>> : std::true_type {};

template <typename T>
inline constexpr bool IsRawPtrV = IsRawPtr<T>::value;

template <typename T>
inline constexpr bool IsRawPtrMayDangleV = false;

template <typename T, RawPtrTraits Traits>
inline constexpr bool IsRawPtrMayDangleV<raw_ptr<T, Traits>> =
    ContainsFlags(Traits, RawPtrTraits::kMayDangle);

// Template helpers for working with T* or raw_ptr<T>.
template <typename T>
struct IsPointer : std::false_type {};

template <typename T>
struct IsPointer<T*> : std::true_type {};

template <typename T, RawPtrTraits Traits>
struct IsPointer<raw_ptr<T, Traits>> : std::true_type {};

template <typename T>
inline constexpr bool IsPointerV = IsPointer<T>::value;

template <typename T>
struct RemovePointer {
  using type = T;
};

template <typename T>
struct RemovePointer<T*> {
  using type = T;
};

template <typename T, RawPtrTraits Traits>
struct RemovePointer<raw_ptr<T, Traits>> {
  using type = T;
};

template <typename T>
using RemovePointerT = typename RemovePointer<T>::type;

struct RawPtrGlobalSettings {
  static void EnableExperimentalAsh() {
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    internal::BackupRefPtrGlobalSettings::EnableExperimentalAsh();
#endif
  }
};

}  // namespace base

using base::raw_ptr;

// DisableDanglingPtrDetection option for raw_ptr annotates
// "intentional-and-safe" dangling pointers. It is meant to be used at the
// margin, only if there is no better way to re-architecture the code.
//
// Usage:
// raw_ptr<T, DisableDanglingPtrDetection> dangling_ptr;
//
// When using it, please provide a justification about what guarantees that it
// will never be dereferenced after becoming dangling.
constexpr auto DisableDanglingPtrDetection = base::RawPtrTraits::kMayDangle;

// See `docs/dangling_ptr.md`
// Annotates known dangling raw_ptr. Those haven't been triaged yet. All the
// occurrences are meant to be removed. See https://crbug.com/1291138.
constexpr auto DanglingUntriaged = base::RawPtrTraits::kMayDangle;

// Unlike DanglingUntriaged, this annotates raw_ptrs that are known to
// dangle only occasionally on the CQ.
//
// These were found from CQ runs and analysed in this dashboard:
// https://docs.google.com/spreadsheets/d/1k12PQOG4y1-UEV9xDfP1F8FSk4cVFywafEYHmzFubJ8/
//
// This is not meant to be added manually. You can ignore this flag.
constexpr auto FlakyDanglingUntriaged = base::RawPtrTraits::kMayDangle;

// Dangling raw_ptr that is more likely to cause UAF: its memory was freed in
// one task, and the raw_ptr was released in a different one.
//
// This is not meant to be added manually. You can ignore this flag.
constexpr auto AcrossTasksDanglingUntriaged = base::RawPtrTraits::kMayDangle;

// The use of pointer arithmetic with raw_ptr is strongly discouraged and
// disabled by default. Usually a container like span<> should be used
// instead of the raw_ptr.
constexpr auto AllowPtrArithmetic = base::RawPtrTraits::kAllowPtrArithmetic;

// Temporary flag for `raw_ptr` / `raw_ref`. This is used by finch experiments
// to differentiate pointers added recently for the ChromeOS ash rewrite.
//
// See launch plan:
// https://docs.google.com/document/d/105OVhNl-2lrfWElQSk5BXYv-nLynfxUrbC4l8cZ0CoU/edit
//
// This is not meant to be added manually. You can ignore this flag.
constexpr auto ExperimentalAsh = base::RawPtrTraits::kExperimentalAsh;

// The use of uninitialized pointers is strongly discouraged. raw_ptrs will
// be initialized to nullptr by default in all cases when building against
// Chromium. However, third-party projects built in a standalone manner may
// wish to opt out where possible. One way to do this is via buildflags,
// thus affecting all raw_ptrs, but a finer-grained mechanism is the use
// of the kAllowUninitialized trait.
//
// Note that opting out may not always be effective, given that algorithms
// like BackupRefPtr require nullptr initializaion for correctness and thus
// silently enforce it.
constexpr auto AllowUninitialized = base::RawPtrTraits::kAllowUninitialized;

// This flag is used to tag a subset of dangling pointers. Similarly to
// DanglingUntriaged, those pointers are known to be dangling. However, we also
// detected that those raw_ptr's were never released (either by calling
// raw_ptr's destructor or by resetting its value), which can ultimately put
// pressure on the BRP quarantine.
//
// This is not meant to be added manually. You can ignore this flag.
constexpr auto LeakedDanglingUntriaged = base::RawPtrTraits::kMayDangle;

// Temporary annotation for new pointers added during the renderer rewrite.
// TODO(crbug.com/1444624): Find pre-existing dangling pointers and remove
// this annotation.
//
// DO NOT ADD new occurrences of this.
constexpr auto ExperimentalRenderer = base::RawPtrTraits::kMayDangle;

// Public verson used in callbacks arguments when it is known that they might
// receive dangling pointers. In any other cases, please
// use one of:
// - raw_ptr<T, DanglingUntriaged>
// - raw_ptr<T, DisableDanglingPtrDetection>
template <typename T, base::RawPtrTraits Traits = base::RawPtrTraits::kEmpty>
using MayBeDangling = base::raw_ptr<T, Traits | base::RawPtrTraits::kMayDangle>;

namespace std {

// Override so set/map lookups do not create extra raw_ptr. This also allows
// dangling pointers to be used for lookup.
template <typename T, base::RawPtrTraits Traits>
struct less<raw_ptr<T, Traits>> {
  using Impl = typename raw_ptr<T, Traits>::Impl;
  using is_transparent = void;

  bool operator()(const raw_ptr<T, Traits>& lhs,
                  const raw_ptr<T, Traits>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(T* lhs, const raw_ptr<T, Traits>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(const raw_ptr<T, Traits>& lhs, T* rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }
};

// Define for cases where raw_ptr<T> holds a pointer to an array of type T.
// This is consistent with definition of std::iterator_traits<T*>.
// Algorithms like std::binary_search need that.
template <typename T, base::RawPtrTraits Traits>
struct iterator_traits<raw_ptr<T, Traits>> {
  using difference_type = ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::random_access_iterator_tag;
};

// Specialize std::pointer_traits. The latter is required to obtain the
// underlying raw pointer in the std::to_address(pointer) overload.
// Implementing the pointer_traits is the standard blessed way to customize
// `std::to_address(pointer)` in C++20 [3].
//
// [1] https://wg21.link/pointer.traits.optmem

template <typename T, ::base::RawPtrTraits Traits>
struct pointer_traits<::raw_ptr<T, Traits>> {
  using pointer = ::raw_ptr<T, Traits>;
  using element_type = T;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = ::raw_ptr<U, Traits>;

  static constexpr pointer pointer_to(element_type& r) noexcept {
    return pointer(&r);
  }

  static constexpr element_type* to_address(pointer p) noexcept {
    return p.get();
  }
};

}  // namespace std

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_H_
