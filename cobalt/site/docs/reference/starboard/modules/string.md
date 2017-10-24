---
layout: doc
title: "Starboard Module Reference: string.h"
---

Defines functions for interacting with c-style strings.

## Functions

### SbStringAToI

**Description**

Parses a string into a base-10 integer. This is a shorthand replacement for
`atoi`.

**Declaration**

```
static SB_C_INLINE int SbStringAToI(const char* value) {
  // NOLINTNEXTLINE(readability/casting)
  return (int)SbStringParseSignedInteger(value, NULL, 10);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>value</code></td>
    <td>The string to be converted.</td>
  </tr>
</table>

### SbStringAToL

**Description**

Parses a string into a base-10, long integer. This is a shorthand
replacement for `atol`.

**Declaration**

```
static SB_C_INLINE long SbStringAToL(const char* value) {
  // NOLINTNEXTLINE(readability/casting)
  return SbStringParseSignedInteger(value, NULL, 10);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>value</code></td>
    <td>The string to be converted.
NOLINTNEXTLINE(runtime/int)</td>
  </tr>
</table>

### SbStringCompare

**Description**

Compares the first `count` characters of two 8-bit character strings.
The return value is:
<ul><li>`< 0` if `string1` is ASCII-betically lower than `string2`.
</li><li>`0` if the two strings are equal.
</li><li>`> 0` if `string1` is ASCII-betically higher than `string2`.</li></ul>
This function is meant to be a drop-in replacement for `strncmp`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringCompare-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringCompare-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringCompare-declaration">
<pre>
SB_EXPORT int SbStringCompare(const char* string1,
                              const char* string2,
                              size_t count);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringCompare-stub">

```
#include "starboard/string.h"

int SbStringCompare(const char* /*string1*/,
                    const char* /*string2*/,
                    size_t /*count*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>string1</code></td>
    <td>The first 8-bit character string to compare.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>string2</code></td>
    <td>The second 8-bit character string to compare.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>count</code></td>
    <td>The number of characters to compare.</td>
  </tr>
</table>

### SbStringCompareAll

**Description**

Compares two entire 8-bit character strings. The return value is:
<ul><li>`< 0` if `string1` is ASCII-betically lower than `string2`.
</li><li>`0` if the two strings are equal.
</li><li>`> 0` if `string1` is ASCII-betically higher than `string2`.</li></ul>
This function is meant to be a drop-in replacement for `strcmp`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringCompareAll-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringCompareAll-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringCompareAll-declaration">
<pre>
SB_EXPORT int SbStringCompareAll(const char* string1, const char* string2);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringCompareAll-stub">

```
#include "starboard/string.h"

int SbStringCompareAll(const char* /*string1*/, const char* /*string2*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>string1</code></td>
    <td>The first 8-bit character string to compare.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>string2</code></td>
    <td>The second 8-bit character string to compare.</td>
  </tr>
</table>

### SbStringCompareNoCase

**Description**

Compares two strings, ignoring differences in case. The return value is:
<ul><li>`< 0` if `string1` is ASCII-betically lower than `string2`.
</li><li>`0` if the two strings are equal.
</li><li>`> 0` if `string1` is ASCII-betically higher than `string2`.</li></ul>
This function is meant to be a drop-in replacement for `strcasecmp`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringCompareNoCase-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringCompareNoCase-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringCompareNoCase-declaration">
<pre>
SB_EXPORT int SbStringCompareNoCase(const char* string1, const char* string2);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringCompareNoCase-stub">

```
#include "starboard/string.h"

int SbStringCompareNoCase(const char* /*string1*/, const char* /*string2*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>string1</code></td>
    <td>The first string to compare.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>string2</code></td>
    <td>The second string to compare.</td>
  </tr>
</table>

### SbStringCompareNoCaseN

**Description**

Compares the first `count` characters of two strings, ignoring differences
in case. The return value is:
<ul><li>`< 0` if `string1` is ASCII-betically lower than `string2`.
</li><li>`0` if the two strings are equal.
</li><li>`> 0` if `string1` is ASCII-betically higher than `string2`.</li></ul>
This function is meant to be a drop-in replacement for `strncasecmp`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringCompareNoCaseN-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringCompareNoCaseN-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringCompareNoCaseN-declaration">
<pre>
SB_EXPORT int SbStringCompareNoCaseN(const char* string1,
                                     const char* string2,
                                     size_t count);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringCompareNoCaseN-stub">

```
#include "starboard/string.h"

int SbStringCompareNoCaseN(const char* /*string1*/,
                           const char* /*string2*/,
                           size_t /*count*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>string1</code></td>
    <td>The first string to compare.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>string2</code></td>
    <td>The second string to compare.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>count</code></td>
    <td>The number of characters to compare.</td>
  </tr>
</table>

### SbStringCompareWide

**Description**

Compares the first `count` characters of two 16-bit character strings.
The return value is:
<ul><li>`< 0` if `string1` is ASCII-betically lower than `string2`.
</li><li>`0` if the two strings are equal.
</li><li>`> 0` if `string1` is ASCII-betically higher than `string2`.</li></ul>
This function is meant to be a drop-in replacement for `wcsncmp`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringCompareWide-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringCompareWide-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringCompareWide-declaration">
<pre>
SB_EXPORT int SbStringCompareWide(const wchar_t* string1,
                                  const wchar_t* string2,
                                  size_t count);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringCompareWide-stub">

```
#include "starboard/string.h"

int SbStringCompareWide(const wchar_t* /*string1*/,
                        const wchar_t* /*string2*/,
                        size_t /*count*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const wchar_t*</code><br>        <code>string1</code></td>
    <td>The first 16-bit character string to compare.weird</td>
  </tr>
  <tr>
    <td><code>const wchar_t*</code><br>        <code>string2</code></td>
    <td>The second 16-bit character string to compare.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>count</code></td>
    <td>The number of characters to compare.</td>
  </tr>
</table>

### SbStringConcat

**Description**

Appends `source` to `out_destination` as long as `out_destination` has
enough storage space to hold the concatenated string.<br>
This function is meant to be a drop-in replacement for `strlcat`. Also note
that this function's signature is NOT compatible with `strncat`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringConcat-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringConcat-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringConcat-declaration">
<pre>
SB_EXPORT int SbStringConcat(char* out_destination,
                             const char* source,
                             int destination_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringConcat-stub">

```
#include "starboard/string.h"

int SbStringConcat(char* /*out_destination*/,
                   const char* /*source*/,
                   int /*destination_size*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>char*</code><br>        <code>out_destination</code></td>
    <td>The string to which the <code>source</code> string is appended.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>source</code></td>
    <td>The string to be appended to the destination string.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>destination_size</code></td>
    <td>The amount of storage space available for the
concatenated string.</td>
  </tr>
</table>

### SbStringConcatUnsafe

**Description**

An inline wrapper for an unsafe <code><a href="#sbstringconcat">SbStringConcat</a></code> that assumes that the
`out_destination` provides enough storage space for the concatenated string.
Note that this function's signature is NOT compatible with `strcat`.

**Declaration**

```
static SB_C_INLINE int SbStringConcatUnsafe(char* out_destination,
                                            const char* source) {
  return SbStringConcat(out_destination, source, INT_MAX);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>char*</code><br>        <code>out_destination</code></td>
    <td>The string to which the <code>source</code> string is appended.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>source</code></td>
    <td>The string to be appended to the destination string.</td>
  </tr>
</table>

### SbStringConcatWide

**Description**

Identical to SbStringCat, but for wide characters.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringConcatWide-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringConcatWide-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringConcatWide-declaration">
<pre>
SB_EXPORT int SbStringConcatWide(wchar_t* out_destination,
                                 const wchar_t* source,
                                 int destination_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringConcatWide-stub">

```
#include "starboard/string.h"

int SbStringConcatWide(wchar_t* /*out_destination*/,
                       const wchar_t* /*source*/,
                       int /*destination_size*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>wchar_t*</code><br>        <code>out_destination</code></td>
    <td>The string to which the <code>source</code> string is appended.</td>
  </tr>
  <tr>
    <td><code>const wchar_t*</code><br>        <code>source</code></td>
    <td>The string to be appended to the destination string.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>destination_size</code></td>
    <td>The amount of storage space available for the
concatenated string.</td>
  </tr>
</table>

### SbStringCopy

**Description**

Copies as much of a `source` string as possible and null-terminates it,
given that `destination_size` characters of storage are available. This
function is meant to be a drop-in replacement for `strlcpy`.<br>
The return value specifies the length of `source`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringCopy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringCopy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringCopy-declaration">
<pre>
SB_EXPORT int SbStringCopy(char* out_destination,
                           const char* source,
                           int destination_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringCopy-stub">

```
#include "starboard/string.h"

int SbStringCopy(char* /*out_destination*/,
                 const char* /*source*/,
                 int /*destination_size*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>char*</code><br>        <code>out_destination</code></td>
    <td>The location to which the string is copied.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>source</code></td>
    <td>The string to be copied.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>destination_size</code></td>
    <td>The amount of the source string to copy.</td>
  </tr>
</table>

### SbStringCopyUnsafe

**Description**

An inline wrapper for an unsafe <code><a href="#sbstringcopy">SbStringCopy</a></code> that assumes that the
destination provides enough storage space for the entire string. The return
value is a pointer to the destination string. This function is meant to be
a drop-in replacement for `strcpy`.

**Declaration**

```
static SB_C_INLINE char* SbStringCopyUnsafe(char* out_destination,
                                            const char* source) {
  SbStringCopy(out_destination, source, INT_MAX);
  return out_destination;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>char*</code><br>        <code>out_destination</code></td>
    <td>The location to which the string is copied.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>source</code></td>
    <td>The string to be copied.</td>
  </tr>
</table>

### SbStringCopyWide

**Description**

Identical to <code><a href="#sbstringcopy">SbStringCopy</a></code>, but for wide characters.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringCopyWide-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringCopyWide-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringCopyWide-declaration">
<pre>
SB_EXPORT int SbStringCopyWide(wchar_t* out_destination,
                               const wchar_t* source,
                               int destination_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringCopyWide-stub">

```
#include "starboard/string.h"

int SbStringCopyWide(wchar_t* /*out_destination*/,
                     const wchar_t* /*source*/,
                     int /*destination_size*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>wchar_t*</code><br>        <code>out_destination</code></td>
    <td>The location to which the string is copied.</td>
  </tr>
  <tr>
    <td><code>const wchar_t*</code><br>        <code>source</code></td>
    <td>The string to be copied.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>destination_size</code></td>
    <td>The amount of the source string to copy.</td>
  </tr>
</table>

### SbStringDuplicate

**Description**

Copies `source` into a buffer that is allocated by this function and that
can be freed with SbMemoryDeallocate. This function is meant to be a drop-in
replacement for `strdup`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringDuplicate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringDuplicate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringDuplicate-declaration">
<pre>
SB_EXPORT char* SbStringDuplicate(const char* source);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringDuplicate-stub">

```
#include "starboard/string.h"

char* SbStringDuplicate(const char* /*source*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>source</code></td>
    <td>The string to be copied.</td>
  </tr>
</table>

### SbStringFindCharacter

**Description**

Finds the first occurrence of a `character` in `str`. The return value is a
pointer to the found character in the given string or `NULL` if the
character is not found. Note that this function's signature does NOT match
that of the `strchr` function.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringFindCharacter-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringFindCharacter-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringFindCharacter-declaration">
<pre>
SB_EXPORT const char* SbStringFindCharacter(const char* str, char character);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringFindCharacter-stub">

```
#include "starboard/string.h"

const char* SbStringFindCharacter(const char* /*str*/, char /*character*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>str</code></td>
    <td>The string to search for the character.</td>
  </tr>
  <tr>
    <td><code>char</code><br>        <code>character</code></td>
    <td>The character to find in the string.</td>
  </tr>
</table>

### SbStringFindLastCharacter

**Description**

Finds the last occurrence of a specified character in a string.
The return value is a pointer to the found character in the given string or
`NULL` if the character is not found. Note that this function's signature
does NOT match that of the `strrchr` function.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringFindLastCharacter-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringFindLastCharacter-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringFindLastCharacter-declaration">
<pre>
SB_EXPORT const char* SbStringFindLastCharacter(const char* str,
                                                char character);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringFindLastCharacter-stub">

```
#include "starboard/string.h"

const char* SbStringFindLastCharacter(const char* /*str*/, char /*character*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>str</code></td>
    <td>The string to search for the character.</td>
  </tr>
  <tr>
    <td><code>char</code><br>        <code>character</code></td>
    <td>The character to find in the string.</td>
  </tr>
</table>

### SbStringFindString

**Description**

Finds the first occurrence of `str2` in `str1`. The return value is a
pointer to the beginning of the found string or `NULL` if the string is
not found. This function is meant to be a drop-in replacement for `strstr`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringFindString-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringFindString-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringFindString-declaration">
<pre>
SB_EXPORT const char* SbStringFindString(const char* str1, const char* str2);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringFindString-stub">

```
#include "starboard/string.h"

const char* SbStringFindString(const char* /*str1*/, const char* /*str2*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>str1</code></td>
    <td>The string in which to search for the presence of <code>str2</code>.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>str2</code></td>
    <td>The string to locate in <code>str1</code>.</td>
  </tr>
</table>

### SbStringFormat

**Description**

Produces a string formatted with `format` and `arguments`, placing as much
of the result that will fit into `out_buffer`. The return value specifies
the number of characters that the format would produce if `buffer_size` were
infinite.<br>
This function is meant to be a drop-in replacement for `vsnprintf`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringFormat-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringFormat-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringFormat-declaration">
<pre>
SB_EXPORT int SbStringFormat(char* out_buffer,
                             size_t buffer_size,
                             const char* format,
                             va_list arguments) SB_PRINTF_FORMAT(3, 0);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringFormat-stub">

```
#include "starboard/string.h"

int SbStringFormat(char* /*out_buffer*/,
                   size_t /*buffer_size*/,
                   const char* /*format*/,
                   va_list /*arguments*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>char*</code><br>        <code>out_buffer</code></td>
    <td>The location where the formatted string is stored.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>buffer_size</code></td>
    <td>The size of <code>out_buffer</code>.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>format</code></td>
    <td>A string that specifies how the data should be formatted.</td>
  </tr>
  <tr>
    <td><code>va_list</code><br>        <code>arguments</code></td>
    <td>Variable arguments used in the string.</td>
  </tr>
</table>

### SbStringFormatF

**Declaration**

```
static SB_C_INLINE int SbStringFormatF(char* out_buffer,
                                       size_t buffer_size,
                                       const char* format,
                                       ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = SbStringFormat(out_buffer, buffer_size, format, arguments);
  va_end(arguments);
  return result;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>char*</code><br>
        <code>out_buffer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>size_t</code><br>
        <code>buffer_size</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const char*</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code></code><br>
        <code>...</code></td>
    <td> </td>
  </tr>
</table>

### SbStringFormatUnsafeF

**Declaration**

```
static SB_C_INLINE int SbStringFormatUnsafeF(char* out_buffer,
                                             const char* format,
                                             ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = SbStringFormat(out_buffer, UINT_MAX, format, arguments);
  va_end(arguments);
  return result;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>char*</code><br>
        <code>out_buffer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const char*</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code></code><br>
        <code>...</code></td>
    <td> </td>
  </tr>
</table>

### SbStringFormatWide

**Description**

This function is identical to <code><a href="#sbstringformat">SbStringFormat</a></code>, but is for wide characters.
It is meant to be a drop-in replacement for `vswprintf`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringFormatWide-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringFormatWide-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringFormatWide-declaration">
<pre>
SB_EXPORT int SbStringFormatWide(wchar_t* out_buffer,
                                 size_t buffer_size,
                                 const wchar_t* format,
                                 va_list arguments);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringFormatWide-stub">

```
#include "starboard/string.h"

int SbStringFormatWide(wchar_t* /*out_buffer*/,
                       size_t /*buffer_size*/,
                       const wchar_t* /*format*/,
                       va_list /*arguments*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>wchar_t*</code><br>        <code>out_buffer</code></td>
    <td>The location where the formatted string is stored.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>buffer_size</code></td>
    <td>The size of <code>out_buffer</code>.</td>
  </tr>
  <tr>
    <td><code>const wchar_t*</code><br>        <code>format</code></td>
    <td>A string that specifies how the data should be formatted.</td>
  </tr>
  <tr>
    <td><code>va_list</code><br>        <code>arguments</code></td>
    <td>Variable arguments used in the string.</td>
  </tr>
</table>

### SbStringFormatWideF

**Description**

An inline wrapper of <code><a href="#sbstringformat">SbStringFormat</a></code>Wide that converts from ellipsis to
`va_args`.

**Declaration**

```
static SB_C_INLINE int SbStringFormatWideF(wchar_t* out_buffer,
                                           size_t buffer_size,
                                           const wchar_t* format,
                                           ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = SbStringFormatWide(out_buffer, buffer_size, format, arguments);
  va_end(arguments);
  return result;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>wchar_t*</code><br>        <code>out_buffer</code></td>
    <td>The location where the formatted string is stored.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>buffer_size</code></td>
    <td>The size of <code>out_buffer</code>.</td>
  </tr>
  <tr>
    <td><code>const wchar_t*</code><br>        <code>format</code></td>
    <td>A string that specifies how the data should be formatted.</td>
  </tr>
  <tr>
    <td><code></code><br>        <code>...</code></td>
    <td>Arguments used in the string.</td>
  </tr>
</table>

### SbStringGetLength

**Description**

Returns the length, in characters, of `str`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringGetLength-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringGetLength-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringGetLength-declaration">
<pre>
SB_EXPORT size_t SbStringGetLength(const char* str);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringGetLength-stub">

```
#include "starboard/string.h"

size_t SbStringGetLength(const char* /*str*/) {
  return static_cast<size_t>(0);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>str</code></td>
    <td>A zero-terminated ASCII string.</td>
  </tr>
</table>

### SbStringGetLengthWide

**Description**

Returns the length of a wide character string. (This function is the same
as <code><a href="#sbstringgetlength">SbStringGetLength</a></code>, but for a string comprised of wide characters.) This
function assumes that there are no multi-element characters.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringGetLengthWide-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringGetLengthWide-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringGetLengthWide-declaration">
<pre>
SB_EXPORT size_t SbStringGetLengthWide(const wchar_t* str);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringGetLengthWide-stub">

```
#include "starboard/string.h"

size_t SbStringGetLengthWide(const wchar_t* /*str*/) {
  return static_cast<size_t>(0);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const wchar_t*</code><br>        <code>str</code></td>
    <td>A zero-terminated ASCII string.</td>
  </tr>
</table>

### SbStringParseDouble

**Description**

Extracts a string that represents an integer from the beginning of `start`
into a double.<br>
This function is meant to be a drop-in replacement for `strtod`, except
that it is explicitly declared to return a double.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringParseDouble-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringParseDouble-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringParseDouble-declaration">
<pre>
SB_EXPORT double SbStringParseDouble(const char* start, char** out_end);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringParseDouble-stub">

```
#include "starboard/string.h"

double SbStringParseDouble(const char* start, char** out_end) {
  if (out_end != NULL)
    *out_end = NULL;
  return 0.0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>start</code></td>
    <td>The string that begins with the number to be converted.</td>
  </tr>
  <tr>
    <td><code>char**</code><br>        <code>out_end</code></td>
    <td>If provided, the function places a pointer to the end of the
consumed portion of the string into <code>out_end</code>.</td>
  </tr>
</table>

### SbStringParseSignedInteger

**Description**

Extracts a string that represents an integer from the beginning of `start`
into a signed integer in the given `base`. This function is meant to be a
drop-in replacement for `strtol`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringParseSignedInteger-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringParseSignedInteger-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringParseSignedInteger-declaration">
<pre>
SB_EXPORT long SbStringParseSignedInteger(const char* start,
                                          char** out_end,
                                          int base);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringParseSignedInteger-stub">

```
#include "starboard/string.h"

// NOLINTNEXTLINE(runtime/int)
long SbStringParseSignedInteger(const char* /*start*/,
                                char** /*out_end*/,
                                int /*base*/) {
  return 0L;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>start</code></td>
    <td>The string that begins with the number to be converted.</td>
  </tr>
  <tr>
    <td><code>char**</code><br>        <code>out_end</code></td>
    <td>If provided, the function places a pointer to the end of the
consumed portion of the string into <code>out_end</code>.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>base</code></td>
    <td>The base into which the number will be converted. The value must
be between <code>2</code> and <code>36</code>, inclusive.
NOLINTNEXTLINE(runtime/int)</td>
  </tr>
</table>

### SbStringParseUInt64

**Description**

Extracts a string that represents an integer from the beginning of `start`
into an unsigned 64-bit integer in the given `base`.<br>
This function is meant to be a drop-in replacement for `strtoull`, except
that it is explicitly declared to return `uint64_t`.

**Declaration**

```
SB_EXPORT uint64_t SbStringParseUInt64(const char* start,
                                       char** out_end,
                                       int base);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>start</code></td>
    <td>The string that begins with the number to be converted.</td>
  </tr>
  <tr>
    <td><code>char**</code><br>        <code>out_end</code></td>
    <td>If provided, the function places a pointer to the end of the
consumed portion of the string into <code>out_end</code>.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>base</code></td>
    <td>The base into which the number will be converted. The value must
be between <code>2</code> and <code>36</code>, inclusive.</td>
  </tr>
</table>

### SbStringParseUnsignedInteger

**Description**

Extracts a string that represents an integer from the beginning of `start`
into an unsigned integer in the given `base`.
This function is meant to be a drop-in replacement for `strtoul`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringParseUnsignedInteger-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringParseUnsignedInteger-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringParseUnsignedInteger-declaration">
<pre>
SB_EXPORT unsigned long SbStringParseUnsignedInteger(const char* start,
                                                     char** out_end,
                                                     int base);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringParseUnsignedInteger-stub">

```
#include "starboard/string.h"

// NOLINTNEXTLINE(runtime/int)
unsigned long SbStringParseUnsignedInteger(const char* /*start*/,
                                           char** /*out_end*/,
                                           int /*base*/) {
  return 0UL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>start</code></td>
    <td>The string that begins with the number to be converted.</td>
  </tr>
  <tr>
    <td><code>char**</code><br>        <code>out_end</code></td>
    <td>If provided, the function places a pointer to the end of the
consumed portion of the string into <code>out_end</code>.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>base</code></td>
    <td>The base into which the number will be converted. The value must
be between <code>2</code> and <code>36</code>, inclusive.
NOLINTNEXTLINE(runtime/int)</td>
  </tr>
</table>

### SbStringScan

**Description**

Scans `buffer` for `pattern`, placing the extracted values in `arguments`.
The return value specifies the number of successfully matched items, which
may be `0`.<br>
This function is meant to be a drop-in replacement for `vsscanf`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStringScan-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStringScan-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStringScan-declaration">
<pre>
SB_EXPORT int SbStringScan(const char* buffer,
                           const char* pattern,
                           va_list arguments);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStringScan-stub">

```
#include "starboard/string.h"

int SbStringScan(const char* /*buffer*/,
                 const char* /*pattern*/,
                 va_list /*arguments*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>buffer</code></td>
    <td>The string to scan for the pattern.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>pattern</code></td>
    <td>The string to search for in <code>buffer</code>.</td>
  </tr>
  <tr>
    <td><code>va_list</code><br>        <code>arguments</code></td>
    <td>Values matching <code>pattern</code> that were extracted from <code>buffer</code>.</td>
  </tr>
</table>

### SbStringScanF

**Description**

An inline wrapper of <code><a href="#sbstringscan">SbStringScan</a></code> that converts from ellipsis to `va_args`.
This function is meant to be a drop-in replacement for `sscanf`.

**Declaration**

```
static SB_C_INLINE int SbStringScanF(const char* buffer,
                                     const char* pattern,
                                     ...) {
  va_list arguments;
  va_start(arguments, pattern);
  int result = SbStringScan(buffer, pattern, arguments);
  va_end(arguments);
  return result;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>buffer</code></td>
    <td>The string to scan for the pattern.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>pattern</code></td>
    <td>The string to search for in <code>buffer</code>.</td>
  </tr>
  <tr>
    <td><code></code><br>        <code>...</code></td>
    <td>Values matching <code>pattern</code> that were extracted from <code>buffer</code>.</td>
  </tr>
</table>

