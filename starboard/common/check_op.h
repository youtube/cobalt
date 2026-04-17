// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_CHECK_OP_H_
#define STARBOARD_COMMON_CHECK_OP_H_

#include <cstddef>
#include <string>
#include <type_traits>

#include "starboard/common/check.h"
#include "starboard/common/log.h"

// Copied from base/check_op.h with modificatios, including
// - Add SB_ prefix to macros to avoid name conflict.
// - Replace deps on base/ with corresponding ones in starboard/

// This header defines the (DP)CHECK_EQ etc. macros.
//
// (DP)CHECK_EQ(x, y) is similar to (DP)CHECK(x == y) but will also log the
// values of x and y if the condition doesn't hold. This works for basic types
// and types with an operator<< or .ToString() method.
//
// The operands are evaluated exactly once, and even in build modes where e.g.
// DCHECK is disabled, the operands and their stringification methods are still
// referenced to avoid warnings about unused variables or functions.
//
// To support the stringification of the check operands, this header is
// *significantly* larger than base/check.h, so it should be avoided in common
// headers.
//
// This header also provides the (DP)CHECK macros (by including check.h), so if
// you use e.g. both CHECK_EQ and CHECK, including this header is enough. If you
// only use CHECK however, please include the smaller check.h instead.

namespace starboard {

// Uses expression SFINAE to detect whether using operator<< would work.
//
// Note that the above #include of <ostream> is necessary to guarantee
// consistent results here for basic types.
template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};
template <typename T>
struct SupportsOstreamOperator<T,
                               decltype(void(std::declval<std::ostream&>()
                                             << std::declval<T>()))>
    : std::true_type {};

template <typename T, typename = void>
struct SupportsToString : std::false_type {};
template <typename T>
struct SupportsToString<T, decltype(void(std::declval<T>().ToString()))>
    : std::true_type {};

// Functions for turning check operand values into strings.
// Caller takes ownership of the returned string.
char* CheckOpValueStr(int v);
char* CheckOpValueStr(unsigned v);
char* CheckOpValueStr(long v);
char* CheckOpValueStr(unsigned long v);
char* CheckOpValueStr(long long v);
char* CheckOpValueStr(unsigned long long v);
char* CheckOpValueStr(const void* v);
char* CheckOpValueStr(std::nullptr_t v);
char* CheckOpValueStr(double v);
char* CheckOpValueStr(const std::string& v);

// Convert a streamable value to string out-of-line to avoid <sstream>.
char* StreamValToStr(const void* v,
                     void (*stream_func)(std::ostream&, const void*));

#ifdef __has_builtin
#define SUPPORTS_BUILTIN_ADDRESSOF (__has_builtin(__builtin_addressof))
#else
#define SUPPORTS_BUILTIN_ADDRESSOF 0
#endif

template <typename T>
inline typename std::enable_if<
    SupportsOstreamOperator<const T&>::value &&
        !std::is_function<typename std::remove_pointer<T>::type>::value,
    char*>::type CheckOpValueStr(const T& v) {
  auto f = [](std::ostream& s, const void* p) {
    s << *reinterpret_cast<const T*>(p);
  };

  // operator& might be overloaded, so do the std::addressof dance.
  // __builtin_addressof is preferred since it also handles Obj-C ARC pointers.
  // Some casting is still needed, because T might be volatile.
#if SUPPORTS_BUILTIN_ADDRESSOF
  const void* vp = const_cast<const void*>(
      reinterpret_cast<const volatile void*>(__builtin_addressof(v)));
#else
  const void* vp = reinterpret_cast<const void*>(
      const_cast<const char*>(&reinterpret_cast<const volatile char&>(v)));
#endif
  return StreamValToStr(vp, f);
}

#undef SUPPORTS_BUILTIN_ADDRESSOF

// Overload for types that have no operator<< but do have .ToString() defined.
template <typename T>
inline typename std::enable_if<!SupportsOstreamOperator<const T&>::value &&
                                   SupportsToString<const T&>::value,
                               char*>::type
CheckOpValueStr(const T& v) {
  // .ToString() may not return a std::string, e.g. blink::WTF::String.
  return CheckOpValueStr(v.ToString());
}

// Provide an overload for functions and function pointers. Function pointers
// don't implicitly convert to void* but do implicitly convert to bool, so
// without this function pointers are always printed as 1 or 0. (MSVC isn't
// standards-conforming here and converts function pointers to regular
// pointers, so this is a no-op for MSVC.)
template <typename T>
inline typename std::enable_if<
    std::is_function<typename std::remove_pointer<T>::type>::value,
    char*>::type
CheckOpValueStr(const T& v) {
  return CheckOpValueStr(reinterpret_cast<const void*>(v));
}

// We need overloads for enums that don't support operator<<.
// (i.e. scoped enums where no operator<< overload was declared).
template <typename T>
inline typename std::enable_if<!SupportsOstreamOperator<const T&>::value &&
                                   std::is_enum<T>::value,
                               char*>::type
CheckOpValueStr(const T& v) {
  return CheckOpValueStr(
      static_cast<typename std::underlying_type<T>::type>(v));
}

// Captures the result of a CHECK_op and facilitates testing as a boolean.
class CheckOpResult {
 public:
  // An empty result signals success.
  constexpr CheckOpResult() {}

  constexpr explicit CheckOpResult(LogMessage* log_message)
      : log_message_(log_message) {}

  // Returns true if the check succeeded.
  constexpr explicit operator bool() const { return !log_message_; }

  LogMessage* log_message() { return log_message_; }

  // TODO(pbos): Annotate this ABSL_ATTRIBUTE_RETURNS_NONNULL after solving
  // compile failure.
  // Takes ownership of `v1_str` and `v2_str`, destroying them with free(). For
  // use with CheckOpValueStr() which allocates these strings using strdup().
  static LogMessage* CreateLogMessage(const char* file,
                                      int line,
                                      const char* expr_str,
                                      char* v1_str,
                                      char* v2_str);

 private:
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #constexpr-ctor-field-initializer
  LogMessage* const log_message_ = nullptr;
};

// Helper macro for binary operators.
// The 'switch' is used to prevent the 'else' from being ambiguous when the
// macro is used in an 'if' clause such as:
// if (a == 1)
//   CHECK_EQ(2, a);
#define SB_CHECK_OP_FUNCTION_IMPL(name, op, val1, val2)                        \
  switch (0)                                                                   \
  case 0:                                                                      \
  default:                                                                     \
    if (::starboard::CheckOpResult true_if_passed =                            \
            ::starboard::Check##name##Impl(__FILE__, __LINE__, (val1), (val2), \
                                           #val1 " " #op " " #val2))           \
      ;                                                                        \
    else                                                                       \
      ::starboard::CheckError(true_if_passed.log_message())

#define SB_CHECK_OP(name, op, val1, val2) \
  SB_CHECK_OP_FUNCTION_IMPL(name, op, val1, val2)

// The second overload avoids address-taking of static members for
// fundamental types.
#define DEFINE_SB_CHECK_OP_IMPL(name, op)                                 \
  template <typename T, typename U,                                       \
            std::enable_if_t<!std::is_fundamental<T>::value ||            \
                                 !std::is_fundamental<U>::value,          \
                             int> = 0>                                    \
  constexpr ::starboard::CheckOpResult Check##name##Impl(                 \
      const char* file, int line, const T& v1, const U& v2,               \
      const char* expr_str) {                                             \
    using ::starboard::CheckOpResult;                                     \
    if (v1 op v2)                                                         \
      return CheckOpResult();                                             \
    return CheckOpResult(::starboard::CheckOpResult::CreateLogMessage(    \
        file, line, expr_str, CheckOpValueStr(v1), CheckOpValueStr(v2))); \
  }                                                                       \
  template <typename T, typename U,                                       \
            std::enable_if_t<std::is_fundamental<T>::value &&             \
                                 std::is_fundamental<U>::value,           \
                             int> = 0>                                    \
  constexpr ::starboard::CheckOpResult Check##name##Impl(                 \
      const char* file, int line, T v1, U v2, const char* expr_str) {     \
    using ::starboard::CheckOpResult;                                     \
    if (v1 op v2)                                                         \
      return CheckOpResult();                                             \
    return CheckOpResult(::starboard::CheckOpResult::CreateLogMessage(    \
        file, line, expr_str, CheckOpValueStr(v1), CheckOpValueStr(v2))); \
  }

// clang-format off
DEFINE_SB_CHECK_OP_IMPL(EQ, ==)
DEFINE_SB_CHECK_OP_IMPL(NE, !=)
DEFINE_SB_CHECK_OP_IMPL(LE, <=)
DEFINE_SB_CHECK_OP_IMPL(LT, < )
DEFINE_SB_CHECK_OP_IMPL(GE, >=)
DEFINE_SB_CHECK_OP_IMPL(GT, > )
#undef DEFINE_SB_CHECK_OP_IMPL
#define SB_CHECK_EQ(val1, val2) SB_CHECK_OP(EQ, ==, val1, val2)
#define SB_CHECK_NE(val1, val2) SB_CHECK_OP(NE, !=, val1, val2)
#define SB_CHECK_LE(val1, val2) SB_CHECK_OP(LE, <=, val1, val2)
#define SB_CHECK_LT(val1, val2) SB_CHECK_OP(LT, < , val1, val2)
#define SB_CHECK_GE(val1, val2) SB_CHECK_OP(GE, >=, val1, val2)
#define SB_CHECK_GT(val1, val2) SB_CHECK_OP(GT, > , val1, val2)
// clang-format on

#if SB_DCHECK_ENABLED

#define SB_DCHECK_OP(name, op, val1, val2) \
  SB_CHECK_OP_FUNCTION_IMPL(name, op, val1, val2)

#else

#define SB_DCHECK_OP(name, op, val1, val2) SB_EAT_STREAM_PARAMETERS
#endif

// clang-format off
#define SB_DCHECK_EQ(val1, val2) SB_DCHECK_OP(EQ, ==, val1, val2)
#define SB_DCHECK_NE(val1, val2) SB_DCHECK_OP(NE, !=, val1, val2)
#define SB_DCHECK_LE(val1, val2) SB_DCHECK_OP(LE, <=, val1, val2)
#define SB_DCHECK_LT(val1, val2) SB_DCHECK_OP(LT, < , val1, val2)
#define SB_DCHECK_GE(val1, val2) SB_DCHECK_OP(GE, >=, val1, val2)
#define SB_DCHECK_GT(val1, val2) SB_DCHECK_OP(GT, > , val1, val2)
// clang-format on

}  // namespace starboard

#endif  // STARBOARD_COMMON_CHECK_OP_H_
