/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_IMPL_H_

#include <limits.h>
#include <string.h>

#include <atomic>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_fast_path.h"
#include "third_party/blink/renderer/platform/wtf/text/number_parsing_options.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

#if BUILDFLAG(IS_APPLE)
#include "base/mac/scoped_cftyperef.h"

typedef const struct __CFString* CFStringRef;
#endif

#ifdef __OBJC__
@class NSString;
#endif

namespace WTF {

enum TextCaseSensitivity {
  kTextCaseSensitive,
  kTextCaseASCIIInsensitive,

  // Unicode aware case insensitive matching. Non-ASCII characters might match
  // to ASCII characters. This flag is rarely used to implement web platform
  // features.
  kTextCaseUnicodeInsensitive
};

enum StripBehavior { kStripExtraWhiteSpace, kDoNotStripWhiteSpace };

typedef bool (*CharacterMatchFunctionPtr)(UChar);
typedef bool (*IsWhiteSpaceFunctionPtr)(UChar);
typedef HashMap<wtf_size_t, StringImpl*, AlreadyHashedTraits>
    StaticStringsTable;

// You can find documentation about this class in this doc:
// https://docs.google.com/document/d/1kOCUlJdh2WJMJGDf-WoEQhmnjKLaOYRbiHz5TiGJl14/edit?usp=sharing
class WTF_EXPORT StringImpl {
 private:
  // StringImpls are allocated out of the WTF buffer partition.
  void* operator new(size_t);
  void* operator new(size_t, void* ptr) { return ptr; }
  void operator delete(void*);

  // Used to construct static strings, which have a special ref_count_ that can
  // never hit zero. This means that the static string will never be destroyed.
  enum ConstructEmptyStringTag { kConstructEmptyString };
  explicit StringImpl(ConstructEmptyStringTag)
      : length_(0),
        hash_and_flags_(kAsciiPropertyCheckDone | kContainsOnlyAscii |
                        kIsLowerAscii | kIs8Bit | kIsStatic) {
    // Ensure that the hash is computed so that AtomicStringHash can call
    // existingHash() with impunity. The empty string is special because it
    // is never entered into AtomicString's HashKey, but still needs to
    // compare correctly.
    GetHash();
  }

  enum ConstructEmptyString16BitTag { kConstructEmptyString16Bit };
  explicit StringImpl(ConstructEmptyString16BitTag)
      : length_(0),
        hash_and_flags_(kAsciiPropertyCheckDone | kContainsOnlyAscii |
                        kIsLowerAscii | kIsStatic) {
    GetHash();
  }

  // FIXME: there has to be a less hacky way to do this.
  enum Force8Bit { kForce8BitConstructor };
  StringImpl(wtf_size_t length, Force8Bit)
      : length_(length), hash_and_flags_(LengthToAsciiFlags(length) | kIs8Bit) {
    DCHECK(length_);
  }

  StringImpl(wtf_size_t length)
      : length_(length), hash_and_flags_(LengthToAsciiFlags(length)) {
    DCHECK(length_);
  }

  enum StaticStringTag { kStaticString };
  StringImpl(wtf_size_t length, wtf_size_t hash, StaticStringTag)
      : length_(length),
        hash_and_flags_(hash << kHashShift | LengthToAsciiFlags(length) |
                        kIs8Bit | kIsStatic) {}

 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  static StringImpl* empty_;
  static StringImpl* empty16_bit_;

  StringImpl(const StringImpl&) = delete;
  StringImpl& operator=(const StringImpl&) = delete;
  ~StringImpl();

  static void InitStatics();

  static StringImpl* CreateStatic(const char* string,
                                  wtf_size_t length,
                                  wtf_size_t hash);
  static void ReserveStaticStringsCapacityForSize(wtf_size_t size);
  static void FreezeStaticStrings();
  static const StaticStringsTable& AllStaticStrings();
  static wtf_size_t HighestStaticStringLength() {
    return highest_static_string_length_;
  }

  static scoped_refptr<StringImpl> Create(const UChar*, wtf_size_t length);
  static scoped_refptr<StringImpl> Create(const LChar*, wtf_size_t length);
  static scoped_refptr<StringImpl> Create(
      const LChar*,
      wtf_size_t length,
      ASCIIStringAttributes ascii_attributes);
  static scoped_refptr<StringImpl> Create8BitIfPossible(const UChar*,
                                                        wtf_size_t length);
  template <wtf_size_t inlineCapacity>
  static scoped_refptr<StringImpl> Create8BitIfPossible(
      const Vector<UChar, inlineCapacity>& vector) {
    return Create8BitIfPossible(vector.data(), vector.size());
  }

  ALWAYS_INLINE static scoped_refptr<StringImpl> Create(const char* s,
                                                        wtf_size_t length) {
    return Create(reinterpret_cast<const LChar*>(s), length);
  }
  static scoped_refptr<StringImpl> Create(const LChar*);
  ALWAYS_INLINE static scoped_refptr<StringImpl> Create(const char* s) {
    return Create(reinterpret_cast<const LChar*>(s));
  }

  static scoped_refptr<StringImpl> CreateUninitialized(wtf_size_t length,
                                                       LChar*& data);
  static scoped_refptr<StringImpl> CreateUninitialized(wtf_size_t length,
                                                       UChar*& data);

  wtf_size_t length() const { return length_; }
  bool Is8Bit() const {
    return hash_and_flags_.load(std::memory_order_relaxed) & kIs8Bit;
  }

  ALWAYS_INLINE const LChar* Characters8() const {
    DCHECK(Is8Bit());
    return reinterpret_cast<const LChar*>(this + 1);
  }
  ALWAYS_INLINE const UChar* Characters16() const {
    DCHECK(!Is8Bit());
    return reinterpret_cast<const UChar*>(this + 1);
  }
  ALWAYS_INLINE base::span<const LChar> Span8() const {
    DCHECK(Is8Bit());
    return {reinterpret_cast<const LChar*>(this + 1), length_};
  }
  ALWAYS_INLINE base::span<const UChar> Span16() const {
    DCHECK(!Is8Bit());
    return {reinterpret_cast<const UChar*>(this + 1), length_};
  }
  ALWAYS_INLINE const void* Bytes() const {
    return reinterpret_cast<const void*>(this + 1);
  }

  template <typename CharType>
  ALWAYS_INLINE const CharType* GetCharacters() const;

  size_t CharactersSizeInBytes() const {
    return length() * (Is8Bit() ? sizeof(LChar) : sizeof(UChar));
  }

  bool IsAtomic() const {
    return hash_and_flags_.load(std::memory_order_acquire) & kIsAtomic;
  }

  void SetIsAtomic() {
    hash_and_flags_.fetch_or(kIsAtomic, std::memory_order_release);
  }

  void UnsetIsAtomic() {
    hash_and_flags_.fetch_and(~kIsAtomic, std::memory_order_release);
  }

  bool IsStatic() const {
    return hash_and_flags_.load(std::memory_order_relaxed) & kIsStatic;
  }

  bool ContainsOnlyASCIIOrEmpty() const;

  bool IsLowerASCII() const;

  // The high bits of 'hash' are always empty, but we prefer to store our
  // flags in the low bits because it makes them slightly more efficient to
  // access.  So, we shift left and right when setting and getting our hash
  // code.
  void SetHash(wtf_size_t hash) const {
    // Multiple clients assume that StringHasher is the canonical string
    // hash function.
    DCHECK(hash == (Is8Bit() ? StringHasher::ComputeHashAndMaskTop8Bits(
                                   Characters8(), length_)
                             : StringHasher::ComputeHashAndMaskTop8Bits(
                                   Characters16(), length_)));
    DCHECK(hash);  // Verify that 0 is a valid sentinel hash value.
    SetHashRaw(hash);
  }

  bool HasHash() const { return GetHashRaw() != 0; }

  wtf_size_t ExistingHash() const {
    DCHECK(HasHash());
    return GetHashRaw();
  }

  wtf_size_t GetHash() const {
    if (wtf_size_t hash = GetHashRaw())
      return hash;
    return HashSlowCase();
  }

  ALWAYS_INLINE bool HasOneRef() const {
    return ref_count_.load(std::memory_order_acquire) == 1;
  }

  ALWAYS_INLINE void AddRef() const {
    if (!IsStatic()) {
      uint32_t previous_ref_count =
          ref_count_.fetch_add(1, std::memory_order_relaxed);
      CHECK_NE(previous_ref_count, std::numeric_limits<uint32_t>::max());
#if DCHECK_IS_ON()
      ref_count_change_count_++;
#endif
    }
  }

  // We explicitly remove the AddRef and Release operations from the tsan
  // bots because even though all data races in the C++ memory model sense
  // are undefined behavior, the use of atomics prevents a data race on
  // ref_count_ itself.

  // Sharing the AtomicStringTable causes other races outside of ref_count_
  // that could lead to an early deletion of the StringImpl while other
  // threads are still holding references to it.
  // Possible races:
  // 1. Races where ref_count_ doesn't reach zero are not harmful.
  // 2. Races involving only release calls are not harmful. The
  //    atomicity of the operations guarantee that only the last subtraction to
  //    be executed will trigger the deletion of the StringImpl.
  // 3. A fetch_add on thread A is ordered after a fetch_sub on thread B that
  //    reaches 0. This can only happen on an AddRef() reached through the
  //    AtomicStringTable::Add* methods, otherwise there should be another
  //    reference on thread A, and the Release() on thread B could not have
  //    reached 0. This race is mitigated by the fact that the Atomic String
  //    Table Add and Removal operations (including the fetch_sub to 0) are
  //    done under a lock.

  ALWAYS_INLINE void Release() const {
    if (!IsStatic()) {
      // This can be a relaxed load as long as the subtraction is performed
      // with acq_rel order. Any modification to `ref_count_` reordered after
      // this load will be caught by the while loop or the fetch_sub inside
      // DestroyIfNeeded().
      uint32_t current_ref = ref_count_.load(std::memory_order_relaxed);
#if DCHECK_IS_ON()
      // In non-DCHECK builds, we can save a bit of time in micro-benchmarks by
      // not checking the arithmetic. We hope that checking in DCHECK builds is
      // enough to catch implementation bugs, and that implementation bugs are
      // the only way we'd experience underflow.
      DCHECK_NE(current_ref, 0u);
      ref_count_change_count_++;
#endif
      // This is a fancy fetch_sub() that allows the actual decrement to 0 to
      // be delegated to the DestroyIfNeeded() function. The result of this
      // compare_exchange_weak() will never be 0. Without this, there would be
      // a potential race by reaching 0 and calling AddRef and Release on
      // another thread before the deletion of the string in this thread,
      // triggering the removal and destruction of the string twice.
      do {
        if (current_ref == 1) {
          DestroyIfNeeded();
          return;
        }
      } while (!ref_count_.compare_exchange_weak(current_ref, current_ref - 1,
                                                 std::memory_order_acq_rel));
    }
  }

#if DCHECK_IS_ON()
  unsigned int RefCountChangeCountForTesting() const {
    return ref_count_change_count_;
  }
  void ResetRefCountChangeCountForTesting() { ref_count_change_count_ = 0; }
#endif

  ALWAYS_INLINE void Adopted() const {}

  // FIXME: Does this really belong in StringImpl?
  template <typename T>
  static void CopyChars(T* destination,
                        const T* source,
                        wtf_size_t num_characters) {
    memcpy(destination, source, num_characters * sizeof(T));
  }

  ALWAYS_INLINE static void CopyChars(UChar* destination,
                                      const LChar* source,
                                      wtf_size_t num_characters) {
    for (wtf_size_t i = 0; i < num_characters; ++i)
      destination[i] = source[i];
  }

  // It is no longer required to create isolated copies for thread-safety
  // purposes.
  scoped_refptr<StringImpl> IsolatedCopy() const;

  scoped_refptr<StringImpl> Substring(wtf_size_t pos,
                                      wtf_size_t len = UINT_MAX) const;

  UChar operator[](wtf_size_t i) const {
    SECURITY_DCHECK(i < length_);
    if (Is8Bit())
      return Characters8()[i];
    return Characters16()[i];
  }
  UChar32 CharacterStartingAt(wtf_size_t);

  bool ContainsOnlyWhitespaceOrEmpty();

  int ToInt(NumberParsingOptions, bool* ok) const;
  wtf_size_t ToUInt(NumberParsingOptions, bool* ok) const;
  int64_t ToInt64(NumberParsingOptions, bool* ok) const;
  uint64_t ToUInt64(NumberParsingOptions, bool* ok) const;

  wtf_size_t HexToUIntStrict(bool* ok);
  uint64_t HexToUInt64Strict(bool* ok);

  // FIXME: Like NumberParsingOptions::kStrict, these give false for "ok" when
  // there is trailing garbage.  Like NumberParsingOptions::kLoose, these return
  // the value when there is trailing garbage.  It would be better if these were
  // more consistent with the above functions instead.
  double ToDouble(bool* ok = nullptr);
  float ToFloat(bool* ok = nullptr);

  scoped_refptr<StringImpl> LowerASCII();
  scoped_refptr<StringImpl> UpperASCII();

  scoped_refptr<StringImpl> Fill(UChar);
  // FIXME: Do we need fill(char) or can we just do the right thing if UChar is
  // ASCII?
  scoped_refptr<StringImpl> FoldCase();

  scoped_refptr<StringImpl> Truncate(wtf_size_t length);

  unsigned LengthWithStrippedWhiteSpace() const;

  scoped_refptr<StringImpl> StripWhiteSpace();
  scoped_refptr<StringImpl> StripWhiteSpace(IsWhiteSpaceFunctionPtr);
  scoped_refptr<StringImpl> SimplifyWhiteSpace(
      StripBehavior = kStripExtraWhiteSpace);
  scoped_refptr<StringImpl> SimplifyWhiteSpace(
      IsWhiteSpaceFunctionPtr,
      StripBehavior = kStripExtraWhiteSpace);

  scoped_refptr<StringImpl> RemoveCharacters(CharacterMatchFunctionPtr);
  template <typename CharType>
  ALWAYS_INLINE scoped_refptr<StringImpl> RemoveCharacters(
      const CharType* characters,
      CharacterMatchFunctionPtr);

  // Remove characters between [start, start+lengthToRemove). The range is
  // clamped to the size of the string. Does nothing if start >= length().
  scoped_refptr<StringImpl> Remove(wtf_size_t start,
                                   wtf_size_t length_to_remove = 1);

  // Find characters.
  wtf_size_t Find(LChar character, wtf_size_t start = 0);
  wtf_size_t Find(char character, wtf_size_t start = 0);
  wtf_size_t Find(UChar character, wtf_size_t start = 0);
  wtf_size_t Find(CharacterMatchFunctionPtr, wtf_size_t index = 0);
  wtf_size_t Find(base::RepeatingCallback<bool(UChar)> match_callback,
                  wtf_size_t index = 0) const;

  // Find substrings.
  wtf_size_t Find(const StringView&, wtf_size_t index = 0);
  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.
  wtf_size_t FindIgnoringCase(const StringView&, wtf_size_t index = 0);
  wtf_size_t FindIgnoringASCIICase(const StringView&, wtf_size_t index = 0);

  wtf_size_t ReverseFind(UChar, wtf_size_t index = UINT_MAX);
  wtf_size_t ReverseFind(const StringView&, wtf_size_t index = UINT_MAX);

  bool StartsWith(UChar) const;
  bool StartsWith(const StringView&) const;
  bool StartsWithIgnoringCase(const StringView&) const;
  bool StartsWithIgnoringASCIICase(const StringView&) const;

  bool EndsWith(UChar) const;
  bool EndsWith(const StringView&) const;
  bool EndsWithIgnoringCase(const StringView&) const;
  bool EndsWithIgnoringASCIICase(const StringView&) const;

  // Replace parts of the string.
  scoped_refptr<StringImpl> Replace(UChar pattern, UChar replacement);
  scoped_refptr<StringImpl> Replace(UChar pattern,
                                    const StringView& replacement);
  scoped_refptr<StringImpl> Replace(const StringView& pattern,
                                    const StringView& replacement);
  scoped_refptr<StringImpl> Replace(wtf_size_t index,
                                    wtf_size_t length_to_replace,
                                    const StringView& replacement);

  scoped_refptr<StringImpl> UpconvertedString();

  // Copy characters from string starting at |start| up until |maxLength| or
  // the end of the string is reached. Returns the actual number of characters
  // copied.
  wtf_size_t CopyTo(UChar* buffer,
                    wtf_size_t start,
                    wtf_size_t max_length) const;

  // Append characters from this string into a buffer. Expects the buffer to
  // have the methods:
  //    append(const UChar*, wtf_size_t length);
  //    append(const LChar*, wtf_size_t length);
  // StringBuilder and Vector conform to this protocol.
  template <typename BufferType>
  void AppendTo(BufferType&,
                wtf_size_t start = 0,
                wtf_size_t length = UINT_MAX) const;

  // Prepend characters from this string into a buffer. Expects the buffer to
  // have the methods:
  //    prepend(const UChar*, wtf_size_t length);
  //    prepend(const LChar*, wtf_size_t length);
  // Vector conforms to this protocol.
  template <typename BufferType>
  void PrependTo(BufferType&,
                 wtf_size_t start = 0,
                 wtf_size_t length = UINT_MAX) const;

#if BUILDFLAG(IS_APPLE)
  base::ScopedCFTypeRef<CFStringRef> CreateCFString();
#endif
#ifdef __OBJC__
  operator NSString*();
#endif

  static const UChar kLatin1CaseFoldTable[256];

 private:
  friend class AtomicStringTable;
  enum Flags {
    // These two fields are never modified for the lifetime of the StringImpl.
    // It is therefore safe to read them with a relaxed operation.
    kIs8Bit = 1 << 0,
    kIsStatic = 1 << 1,

    // This is the only flag that can be both set and unset. It is safe to do
    // so because all accesses are mediated by the same atomic string table and
    // so protected by a mutex. Thus these accesses can also be relaxed.
    kIsAtomic = 1 << 2,

    // These bits are set atomically together. They are initially all
    // zero, and like the hash computation below, become non-zero only as part
    // of a single atomic bitwise or. Thus concurrent loads will always observe
    // either a state where the ASCII property check has not been completed and
    // all bits are zero, or a state where the state is fully populated.
    //
    // The reason kIsLowerAscii is cached but upper ascii is not is that
    // DOM attributes APIs require a lowercasing check making it fairly hot.
    kAsciiPropertyCheckDone = 1 << 3,
    kContainsOnlyAscii = 1 << 4,
    kIsLowerAscii = 1 << 5,

    // The last 24 bits (past kHashShift) are reserved for the hash.
    // These bits are all zero if the hash is uncomputed, and the hash is
    // atomically stored with bitwise or.
    //
    // Therefore a relaxed read can be used, and will either observe an
    // uncomputed hash (if the fetch_or is not yet visible on this thread)
    // or the correct hash (if it is). It is possible for a thread to compute
    // the hash for a second time if there is a race. This is safe, since
    // storing the same bits again with a bitwise or is idempotent.
  };

  // Hash value is 24 bits.
  constexpr static int kHashShift = (sizeof(unsigned) * 8) - 24;

  static inline constexpr uint32_t LengthToAsciiFlags(int length) {
    return length
               ? 0
               : kAsciiPropertyCheckDone | kContainsOnlyAscii | kIsLowerAscii;
  }

  static inline uint32_t ASCIIStringAttributesToFlags(
      ASCIIStringAttributes ascii_attributes) {
    uint32_t flags = kAsciiPropertyCheckDone;
    if (ascii_attributes.contains_only_ascii)
      flags |= kContainsOnlyAscii;
    if (ascii_attributes.is_lower_ascii)
      flags |= kIsLowerAscii;
    return flags;
  }

  void SetHashRaw(unsigned hash_val) const {
    // Setting the hash is idempotent so fetch_or() is sufficient. DCHECK()
    // as a sanity check.
    unsigned previous_value = hash_and_flags_.fetch_or(
        hash_val << kHashShift, std::memory_order_relaxed);
    DCHECK(((previous_value >> kHashShift) == 0) ||
           ((previous_value >> kHashShift) == hash_val));
  }

  unsigned GetHashRaw() const {
    return hash_and_flags_.load(std::memory_order_relaxed) >> kHashShift;
  }

  template <typename CharType>
  static size_t AllocationSize(wtf_size_t length) {
    static_assert(
        sizeof(CharType) > 1,
        "Don't use this template with 1-byte chars; use a template "
        "specialization to save time and code-size by avoiding a CheckMul.");
    return base::CheckAdd(sizeof(StringImpl),
                          base::CheckMul(length, sizeof(CharType)))
        .ValueOrDie();
  }

  scoped_refptr<StringImpl> Replace(UChar pattern,
                                    const LChar* replacement,
                                    wtf_size_t replacement_length);
  scoped_refptr<StringImpl> Replace(UChar pattern,
                                    const UChar* replacement,
                                    wtf_size_t replacement_length);

  template <class UCharPredicate>
  unsigned LengthWithStrippedMatchedCharacters(UCharPredicate) const;
  template <class UCharPredicate>
  scoped_refptr<StringImpl> StripMatchedCharacters(UCharPredicate);
  template <typename CharType, class UCharPredicate>
  scoped_refptr<StringImpl> SimplifyMatchedCharactersToSpace(UCharPredicate,
                                                             StripBehavior);
  NOINLINE wtf_size_t HashSlowCase() const;

  void DestroyIfNeeded() const;

  // Calculates the kContainsOnlyAscii and kIsLowerAscii flags. Returns
  // a bitfield with those 2 values.
  unsigned ComputeASCIIFlags() const;

#if DCHECK_IS_ON()
  std::string AsciiForDebugging() const;
#endif

  static wtf_size_t highest_static_string_length_;

#if DCHECK_IS_ON()
  void AssertHashIsCorrect() {
    DCHECK(HasHash());
    DCHECK_EQ(ExistingHash(), StringHasher::ComputeHashAndMaskTop8Bits(
                                  Characters8(), length()));
  }
#endif

#if DCHECK_IS_ON()
  mutable std::atomic<unsigned> ref_count_change_count_{0};
#endif
  // TODO (crbug.com/1083392): Use base::AtomicRefCount.
  mutable std::atomic_uint32_t ref_count_{1};
  const unsigned length_;
  mutable std::atomic<uint32_t> hash_and_flags_;
};

template <>
ALWAYS_INLINE const LChar* StringImpl::GetCharacters<LChar>() const {
  return Characters8();
}

template <>
ALWAYS_INLINE const UChar* StringImpl::GetCharacters<UChar>() const {
  return Characters16();
}

// The following template specialization can be moved to the class declaration
// once we officially switch to C++17 (we need C++ DR727 to be implemented).
template <>
ALWAYS_INLINE size_t StringImpl::AllocationSize<LChar>(wtf_size_t length) {
  static_assert(sizeof(LChar) == 1, "sizeof(LChar) should be 1.");
  return base::CheckAdd(sizeof(StringImpl), length).ValueOrDie();
}

WTF_EXPORT bool Equal(const StringImpl*, const StringImpl*);
WTF_EXPORT bool Equal(const StringImpl*, const LChar*);
inline bool Equal(const StringImpl* a, const char* b) {
  return Equal(a, reinterpret_cast<const LChar*>(b));
}
WTF_EXPORT bool Equal(const StringImpl*, const LChar*, wtf_size_t);
WTF_EXPORT bool Equal(const StringImpl*, const UChar*, wtf_size_t);
inline bool Equal(const StringImpl* a, const char* b, wtf_size_t length) {
  return Equal(a, reinterpret_cast<const LChar*>(b), length);
}
inline bool Equal(const LChar* a, StringImpl* b) {
  return Equal(b, a);
}
inline bool Equal(const char* a, StringImpl* b) {
  return Equal(b, reinterpret_cast<const LChar*>(a));
}
WTF_EXPORT bool EqualNonNull(const StringImpl* a, const StringImpl* b);

ALWAYS_INLINE bool StringImpl::ContainsOnlyASCIIOrEmpty() const {
  uint32_t flags = hash_and_flags_.load(std::memory_order_relaxed);
  if (flags & kAsciiPropertyCheckDone)
    return flags & kContainsOnlyAscii;
  return ComputeASCIIFlags() & kContainsOnlyAscii;
}

ALWAYS_INLINE bool StringImpl::IsLowerASCII() const {
  uint32_t flags = hash_and_flags_.load(std::memory_order_relaxed);
  if (flags & kAsciiPropertyCheckDone)
    return flags & kIsLowerAscii;
  return ComputeASCIIFlags() & kIsLowerAscii;
}

template <typename CharType>
ALWAYS_INLINE bool Equal(const CharType* a,
                         const CharType* b,
                         wtf_size_t length) {
  return !memcmp(a, b, length * sizeof(CharType));
}

ALWAYS_INLINE bool Equal(const LChar* a, const UChar* b, wtf_size_t length) {
  for (wtf_size_t i = 0; i < length; ++i) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

ALWAYS_INLINE bool Equal(const UChar* a, const LChar* b, wtf_size_t length) {
  return Equal(b, a, length);
}

// Unicode aware case insensitive string matching. Non-ASCII characters might
// match to ASCII characters. These functions are rarely used to implement web
// platform features.
// These functions are deprecated. Use EqualIgnoringASCIICase(), or introduce
// EqualIgnoringUnicodeCase(). See crbug.com/627682
WTF_EXPORT bool DeprecatedEqualIgnoringCase(const LChar*,
                                            const LChar*,
                                            wtf_size_t length);
WTF_EXPORT bool DeprecatedEqualIgnoringCase(const UChar*,
                                            const LChar*,
                                            wtf_size_t length);
inline bool DeprecatedEqualIgnoringCase(const LChar* a,
                                        const UChar* b,
                                        wtf_size_t length) {
  return DeprecatedEqualIgnoringCase(b, a, length);
}
WTF_EXPORT bool DeprecatedEqualIgnoringCase(const UChar*,
                                            const UChar*,
                                            wtf_size_t length);

WTF_EXPORT bool EqualIgnoringNullity(StringImpl*, StringImpl*);

template <typename CharacterTypeA, typename CharacterTypeB>
inline bool EqualIgnoringASCIICase(const CharacterTypeA* a,
                                   const CharacterTypeB* b,
                                   wtf_size_t length) {
  for (wtf_size_t i = 0; i < length; ++i) {
    if (ToASCIILower(a[i]) != ToASCIILower(b[i]))
      return false;
  }
  return true;
}

WTF_EXPORT int CodeUnitCompareIgnoringASCIICase(const StringImpl*,
                                                const StringImpl*);
WTF_EXPORT int CodeUnitCompareIgnoringASCIICase(const StringImpl*,
                                                const LChar*);

inline wtf_size_t Find(const LChar* characters,
                       wtf_size_t length,
                       LChar match_character,
                       wtf_size_t index = 0) {
  // Some clients rely on being able to pass index >= length.
  if (index >= length)
    return kNotFound;
  const LChar* found = static_cast<const LChar*>(
      memchr(characters + index, match_character, length - index));
  return found ? static_cast<wtf_size_t>(found - characters) : kNotFound;
}

inline wtf_size_t Find(const UChar* characters,
                       wtf_size_t length,
                       UChar match_character,
                       wtf_size_t index = 0) {
  while (index < length) {
    if (characters[index] == match_character)
      return index;
    ++index;
  }
  return kNotFound;
}

ALWAYS_INLINE wtf_size_t Find(const UChar* characters,
                              wtf_size_t length,
                              LChar match_character,
                              wtf_size_t index = 0) {
  return Find(characters, length, static_cast<UChar>(match_character), index);
}

inline wtf_size_t Find(const LChar* characters,
                       wtf_size_t length,
                       UChar match_character,
                       wtf_size_t index = 0) {
  if (match_character & ~0xFF)
    return kNotFound;
  return Find(characters, length, static_cast<LChar>(match_character), index);
}

template <typename CharacterType>
inline wtf_size_t Find(const CharacterType* characters,
                       wtf_size_t length,
                       char match_character,
                       wtf_size_t index = 0) {
  return Find(characters, length, static_cast<LChar>(match_character), index);
}

inline wtf_size_t Find(const LChar* characters,
                       wtf_size_t length,
                       CharacterMatchFunctionPtr match_function,
                       wtf_size_t index = 0) {
  while (index < length) {
    if (match_function(characters[index]))
      return index;
    ++index;
  }
  return kNotFound;
}

inline wtf_size_t Find(const UChar* characters,
                       wtf_size_t length,
                       CharacterMatchFunctionPtr match_function,
                       wtf_size_t index = 0) {
  while (index < length) {
    if (match_function(characters[index]))
      return index;
    ++index;
  }
  return kNotFound;
}

template <typename CharacterType>
inline wtf_size_t ReverseFind(const CharacterType* characters,
                              wtf_size_t length,
                              CharacterType match_character,
                              wtf_size_t index = UINT_MAX) {
  if (!length)
    return kNotFound;
  if (index >= length)
    index = length - 1;
  while (characters[index] != match_character) {
    if (!index--)
      return kNotFound;
  }
  return index;
}

ALWAYS_INLINE wtf_size_t ReverseFind(const UChar* characters,
                                     wtf_size_t length,
                                     LChar match_character,
                                     wtf_size_t index = UINT_MAX) {
  return ReverseFind(characters, length, static_cast<UChar>(match_character),
                     index);
}

inline wtf_size_t ReverseFind(const LChar* characters,
                              wtf_size_t length,
                              UChar match_character,
                              wtf_size_t index = UINT_MAX) {
  if (match_character & ~0xFF)
    return kNotFound;
  return ReverseFind(characters, length, static_cast<LChar>(match_character),
                     index);
}

inline wtf_size_t StringImpl::Find(LChar character, wtf_size_t start) {
  if (Is8Bit())
    return WTF::Find(Characters8(), length_, character, start);
  return WTF::Find(Characters16(), length_, character, start);
}

ALWAYS_INLINE wtf_size_t StringImpl::Find(char character, wtf_size_t start) {
  return Find(static_cast<LChar>(character), start);
}

inline wtf_size_t StringImpl::Find(UChar character, wtf_size_t start) {
  if (Is8Bit())
    return WTF::Find(Characters8(), length_, character, start);
  return WTF::Find(Characters16(), length_, character, start);
}

inline wtf_size_t LengthOfNullTerminatedString(const UChar* string) {
  size_t length = 0;
  while (string[length] != UChar(0))
    ++length;
  return base::checked_cast<wtf_size_t>(length);
}

template <wtf_size_t inlineCapacity>
bool EqualIgnoringNullity(const Vector<UChar, inlineCapacity>& a,
                          StringImpl* b) {
  if (!b)
    return !a.size();
  if (a.size() != b->length())
    return false;
  if (b->Is8Bit())
    return Equal(a.data(), b->Characters8(), b->length());
  return Equal(a.data(), b->Characters16(), b->length());
}

template <typename CharacterType1, typename CharacterType2>
static inline int CodeUnitCompare(wtf_size_t l1,
                                  wtf_size_t l2,
                                  const CharacterType1* c1,
                                  const CharacterType2* c2) {
  const wtf_size_t lmin = l1 < l2 ? l1 : l2;
  wtf_size_t pos = 0;
  while (pos < lmin && *c1 == *c2) {
    ++c1;
    ++c2;
    ++pos;
  }

  if (pos < lmin)
    return (c1[0] > c2[0]) ? 1 : -1;

  if (l1 == l2)
    return 0;

  return (l1 > l2) ? 1 : -1;
}

static inline int CodeUnitCompare8(const StringImpl* string1,
                                   const StringImpl* string2) {
  return CodeUnitCompare(string1->length(), string2->length(),
                         string1->Characters8(), string2->Characters8());
}

static inline int CodeUnitCompare16(const StringImpl* string1,
                                    const StringImpl* string2) {
  return CodeUnitCompare(string1->length(), string2->length(),
                         string1->Characters16(), string2->Characters16());
}

static inline int CodeUnitCompare8To16(const StringImpl* string1,
                                       const StringImpl* string2) {
  return CodeUnitCompare(string1->length(), string2->length(),
                         string1->Characters8(), string2->Characters16());
}

static inline int CodeUnitCompare(const StringImpl* string1,
                                  const StringImpl* string2) {
  if (!string1)
    return (string2 && string2->length()) ? -1 : 0;

  if (!string2)
    return string1->length() ? 1 : 0;

  bool string1_is_8bit = string1->Is8Bit();
  bool string2_is_8bit = string2->Is8Bit();
  if (string1_is_8bit) {
    if (string2_is_8bit)
      return CodeUnitCompare8(string1, string2);
    return CodeUnitCompare8To16(string1, string2);
  }
  if (string2_is_8bit)
    return -CodeUnitCompare8To16(string2, string1);
  return CodeUnitCompare16(string1, string2);
}

inline scoped_refptr<StringImpl> StringImpl::IsolatedCopy() const {
  if (Is8Bit())
    return Create(Characters8(), length_);
  return Create(Characters16(), length_);
}

template <typename BufferType>
inline void StringImpl::AppendTo(BufferType& result,
                                 wtf_size_t start,
                                 wtf_size_t length) const {
  wtf_size_t number_of_characters_to_copy = std::min(length, length_ - start);
  if (!number_of_characters_to_copy)
    return;
  if (Is8Bit())
    result.Append(Characters8() + start, number_of_characters_to_copy);
  else
    result.Append(Characters16() + start, number_of_characters_to_copy);
}

template <typename BufferType>
inline void StringImpl::PrependTo(BufferType& result,
                                  wtf_size_t start,
                                  wtf_size_t length) const {
  wtf_size_t number_of_characters_to_copy = std::min(length, length_ - start);
  if (!number_of_characters_to_copy)
    return;
  if (Is8Bit())
    result.Prepend(Characters8() + start, number_of_characters_to_copy);
  else
    result.Prepend(Characters16() + start, number_of_characters_to_copy);
}

template <typename T>
struct HashTraits;
// Defined in string_hash.h.
template <>
struct HashTraits<StringImpl*>;
template <>
struct HashTraits<scoped_refptr<StringImpl>>;

}  // namespace WTF

using WTF::StringImpl;
using WTF::kTextCaseASCIIInsensitive;
using WTF::kTextCaseUnicodeInsensitive;
using WTF::kTextCaseSensitive;
using WTF::TextCaseSensitivity;
using WTF::Equal;
using WTF::EqualNonNull;
using WTF::LengthOfNullTerminatedString;
using WTF::ReverseFind;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_IMPL_H_
