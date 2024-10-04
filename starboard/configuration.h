#ifndef STARBOARD_CONFIGURATION_H_
#define STARBOARD_CONFIGURATION_H_

#define SB_API_VERSION 16

#define SB_INT64_C(x) x##LL
#define SB_PRINTF_FORMAT(format_param, dots_param)

#define SB_C_FORCE_INLINE inline
#define SB_NORETURN
#define SB_UNREFERENCED_PARAMETER(x)

#define SB_HAS_QUIRK(x) (0)
#define SB_HAS(x) (0)

#define SB_SIZE_OF(DATATYPE) \
  ((defined SB_SIZE_OF_##DATATYPE) ? (SB_SIZE_OF_##DATATYPE) : (0))
#define SB_ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#define SB_ARRAY_SIZE_INT(x) static_cast<int>(SB_ARRAY_SIZE(x))
#define SB_ALIGNAS(byte_alignment) alignas(byte_alignment)
#define SB_ALIGNOF(type) alignof(type)

#define SB_WARN_UNUSED_RESULT 

#define SB_IS(x) (0)

// Will cause a compiler error with |msg| if |expr| is false. |msg| must be a
// valid identifier, and must be a unique type in the scope of the declaration.
#if defined(__cplusplus)
extern "C++" {
namespace starboard {
template <bool>
struct CompileAssert {};
}  // namespace starboard
}  // extern "C++"

#define SB_COMPILE_ASSERT(expr, msg)                              \
  typedef ::starboard::CompileAssert<(                            \
      bool(expr))>              /* NOLINT(readability/casting) */ \
      msg[bool(expr) ? 1 : -1]  // NOLINT(readability/casting)
#else
#define SB_COMPILE_ASSERT(expr, msg) \
  extern char _COMPILE_ASSERT_##msg[(expr) ? 1 : -1]
#endif

#define SB_FUNCTION __FUNCTION__
#define SB_PREFERRED_RGBA_BYTE_ORDER_RGBA 1

#endif  // STARBOARD_CONFIGURATION_H_
