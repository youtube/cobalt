---
layout: doc
title: "Starboard Module Reference: types.h"
---

Provides a suite of standard types that should be universally available on
all platforms, specifically focused on explicitly-sized integer types and
booleans. This module also includes some related ubiquitous definitions like
limits of the explicitly-sized integer types, and standard pointer and int32
sentinel values.

## Macros

<div id="macro-documentation-section">

<h3 id="dbl_mant_dig" class="small-h3">DBL_MANT_DIG</h3>

<h3 id="int_max" class="small-h3">INT_MAX</h3>

<h3 id="int_min" class="small-h3">INT_MIN</h3>

<h3 id="long_max" class="small-h3">LONG_MAX</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="long_min" class="small-h3">LONG_MIN</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="null" class="small-h3">NULL</h3>

Simulate stddef.h for platforms that don't provide it.

<h3 id="uint_max" class="small-h3">UINT_MAX</h3>

Assume int is 32-bits until we find a platform for which that fails.

<h3 id="ulong_max" class="small-h3">ULONG_MAX</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

</div>

## Functions

### kSbInt16Max

**Declaration**

```
static const int16_t kSbInt16Max = ((int16_t)0x7FFF);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((int16_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbInt16Min

**Declaration**

```
static const int16_t kSbInt16Min = ((int16_t)0x8000);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((int16_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbInt32Max

**Declaration**

```
static const int32_t kSbInt32Max = ((int32_t)0x7FFFFFFF);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((int32_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbInt64Max

**Declaration**

```
static const int64_t kSbInt64Max = ((int64_t)SB_INT64_C(0x7FFFFFFFFFFFFFFF));
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((int64_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbInt64Min

**Declaration**

```
static const int64_t kSbInt64Min = ((int64_t)SB_INT64_C(0x8000000000000000));
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((int64_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbInt8Max

**Declaration**

```
static const int8_t kSbInt8Max = ((int8_t)0x7F);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((int8_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbInt8Min

**Declaration**

```
static const int8_t kSbInt8Min = ((int8_t)0x80);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((int8_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbUInt16Max

**Declaration**

```
static const uint16_t kSbUInt16Max = ((uint16_t)0xFFFF);
#define kSbInt32Min ((int32_t)0x80000000)
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((uint16_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbUInt32Max

**Declaration**

```
static const uint32_t kSbUInt32Max = ((uint32_t)0xFFFFFFFF);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((uint32_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbUInt64Max

**Declaration**

```
static const uint64_t kSbUInt64Max = ((uint64_t)SB_INT64_C(0xFFFFFFFFFFFFFFFF));
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
// A value that represents an int that is probably invalid.
#define kSbInvalidInt kSbInt32Min
#if !SB_HAS(FLOAT_H)
#endif
// --- Standard Include Emulation Audits ---------------------------------------
#if !defined(UINT_MAX) || !defined(INT_MIN) || !defined(INT_MAX) || \
    !defined(LONG_MIN) || !defined(LONG_MAX)
#error "limits.h or emulation did not provide a needed limit macro."
#endif
#if (UINT_MIN + 1 == UINT_MAX - 1) || (INT_MIN + 1 == INT_MAX - 1) || \
    (LONG_MIN + 1 == LONG_MAX - 1)
// This should always evaluate to false, but ensures that the limits macros can
// be used arithmetically in the preprocessor.
#endif
#if !defined(PRId32)
#error "inttypes.h should provide the portable formatting macros."
#endif
// --- Standard Type Audits ----------------------------------------------------
#if SB_IS(WCHAR_T_UTF16)
SB_COMPILE_ASSERT(sizeof(wchar_t) == 2,
                  SB_IS_WCHAR_T_UTF16_is_inconsistent_with_sizeof_wchar_t);
#endif
#if SB_IS(WCHAR_T_UTF32)
SB_COMPILE_ASSERT(sizeof(wchar_t) == 4,
                  SB_IS_WCHAR_T_UTF32_is_inconsistent_with_sizeof_wchar_t);
#endif
#if SB_IS(WCHAR_T_SIGNED)
SB_COMPILE_ASSERT((wchar_t)(-1) < 0,
                  SB_IS_WCHAR_T_SIGNED_is_defined_incorrectly);
#endif
#if SB_IS(WCHAR_T_UNSIGNED)
SB_COMPILE_ASSERT((wchar_t)(-1) > 0,
                  SB_IS_WCHAR_T_UNSIGNED_is_defined_incorrectly);
#endif
#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // STARBOARD_TYPES_H_
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((uint64_t</code></td>
    <td> </td>
  </tr>
</table>

### kSbUInt8Max

**Declaration**

```
static const uint8_t kSbUInt8Max = ((uint8_t)0xFF);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>=</code><br>
        <code>((uint8_t</code></td>
    <td> </td>
  </tr>
</table>

