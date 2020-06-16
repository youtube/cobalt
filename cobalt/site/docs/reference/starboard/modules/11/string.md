---
layout: doc
title: "Starboard Module Reference: string.h"
---

Defines functions for interacting with c-style strings.

## Functions ##

### SbStringAToI ###

Parses a string into a base-10 integer. This is a shorthand replacement for
`atoi`.

`value`: The string to be converted.

#### Declaration ####

```
static int SbStringAToI(const char *value)
```

### SbStringAToL ###

Parses a string into a base-10, long integer. This is a shorthand replacement
for `atol`.

`value`: The string to be converted. NOLINTNEXTLINE(runtime/int)

#### Declaration ####

```
static long SbStringAToL(const char *value)
```

### SbStringCompare ###

Compares the first `count` characters of two 8-bit character strings. The return
value is:

*   `< 0` if `string1` is ASCII-betically lower than `string2`.

*   `0` if the two strings are equal.

*   `> 0` if `string1` is ASCII-betically higher than `string2`.

This function is meant to be a drop-in replacement for `strncmp`.

`string1`: The first 8-bit character string to compare. `string2`: The second
8-bit character string to compare. `count`: The number of characters to compare.

#### Declaration ####

```
int SbStringCompare(const char *string1, const char *string2, size_t count)
```

### SbStringCompareAll ###

Compares two entire 8-bit character strings. The return value is:

*   `< 0` if `string1` is ASCII-betically lower than `string2`.

*   `0` if the two strings are equal.

*   `> 0` if `string1` is ASCII-betically higher than `string2`.

This function is meant to be a drop-in replacement for `strcmp`.

`string1`: The first 8-bit character string to compare. `string2`: The second
8-bit character string to compare.

#### Declaration ####

```
int SbStringCompareAll(const char *string1, const char *string2)
```

### SbStringCompareNoCase ###

Compares two strings, ignoring differences in case. The return value is:

*   `< 0` if `string1` is ASCII-betically lower than `string2`.

*   `0` if the two strings are equal.

*   `> 0` if `string1` is ASCII-betically higher than `string2`.

This function is meant to be a drop-in replacement for `strcasecmp`.

`string1`: The first string to compare. `string2`: The second string to compare.

#### Declaration ####

```
int SbStringCompareNoCase(const char *string1, const char *string2)
```

### SbStringCompareNoCaseN ###

Compares the first `count` characters of two strings, ignoring differences in
case. The return value is:

*   `< 0` if `string1` is ASCII-betically lower than `string2`.

*   `0` if the two strings are equal.

*   `> 0` if `string1` is ASCII-betically higher than `string2`.

This function is meant to be a drop-in replacement for `strncasecmp`.

`string1`: The first string to compare. `string2`: The second string to compare.
`count`: The number of characters to compare.

#### Declaration ####

```
int SbStringCompareNoCaseN(const char *string1, const char *string2, size_t count)
```

### SbStringCompareWide ###

Compares the first `count` characters of two 16-bit character strings. The
return value is:

*   `< 0` if `string1` is ASCII-betically lower than `string2`.

*   `0` if the two strings are equal.

*   `> 0` if `string1` is ASCII-betically higher than `string2`.

This function is meant to be a drop-in replacement for `wcsncmp`.

`string1`: The first 16-bit character string to compare.weird `string2`: The
second 16-bit character string to compare. `count`: The number of characters to
compare.

#### Declaration ####

```
int SbStringCompareWide(const wchar_t *string1, const wchar_t *string2, size_t count)
```

### SbStringConcat ###

Appends `source` to `out_destination` as long as `out_destination` has enough
storage space to hold the concatenated string.

This function is meant to be a drop-in replacement for `strlcat`. Also note that
this function's signature is NOT compatible with `strncat`.

`out_destination`: The string to which the `source` string is appended.
`source`: The string to be appended to the destination string.
`destination_size`: The amount of storage space available for the concatenated
string.

#### Declaration ####

```
int SbStringConcat(char *out_destination, const char *source, int destination_size)
```

### SbStringConcatUnsafe ###

An inline wrapper for an unsafe SbStringConcat that assumes that the
`out_destination` provides enough storage space for the concatenated string.
Note that this function's signature is NOT compatible with `strcat`.

`out_destination`: The string to which the `source` string is appended.
`source`: The string to be appended to the destination string.

#### Declaration ####

```
static int SbStringConcatUnsafe(char *out_destination, const char *source)
```

### SbStringConcatWide ###

Identical to SbStringCat, but for wide characters.

`out_destination`: The string to which the `source` string is appended.
`source`: The string to be appended to the destination string.
`destination_size`: The amount of storage space available for the concatenated
string.

#### Declaration ####

```
int SbStringConcatWide(wchar_t *out_destination, const wchar_t *source, int destination_size)
```

### SbStringCopy ###

Copies as much of a `source` string as possible and null-terminates it, given
that `destination_size` characters of storage are available. This function is
meant to be a drop-in replacement for `strlcpy`.

The return value specifies the length of `source`.

`out_destination`: The location to which the string is copied. `source`: The
string to be copied. `destination_size`: The amount of the source string to
copy.

#### Declaration ####

```
int SbStringCopy(char *out_destination, const char *source, int destination_size)
```

### SbStringCopyUnsafe ###

An inline wrapper for an unsafe SbStringCopy that assumes that the destination
provides enough storage space for the entire string. The return value is a
pointer to the destination string. This function is meant to be a drop-in
replacement for `strcpy`.

`out_destination`: The location to which the string is copied. `source`: The
string to be copied.

#### Declaration ####

```
static char* SbStringCopyUnsafe(char *out_destination, const char *source)
```

### SbStringCopyWide ###

Identical to SbStringCopy, but for wide characters.

`out_destination`: The location to which the string is copied. `source`: The
string to be copied. `destination_size`: The amount of the source string to
copy.

#### Declaration ####

```
int SbStringCopyWide(wchar_t *out_destination, const wchar_t *source, int destination_size)
```

### SbStringDuplicate ###

Copies `source` into a buffer that is allocated by this function and that can be
freed with SbMemoryDeallocate. This function is meant to be a drop-in
replacement for `strdup`.

`source`: The string to be copied.

#### Declaration ####

```
char* SbStringDuplicate(const char *source)
```

### SbStringFindCharacter ###

Finds the first occurrence of a `character` in `str`. The return value is a
pointer to the found character in the given string or `NULL` if the character is
not found. Note that this function's signature does NOT match that of the
`strchr` function.

`str`: The string to search for the character. `character`: The character to
find in the string.

#### Declaration ####

```
const char* SbStringFindCharacter(const char *str, char character)
```

### SbStringFindLastCharacter ###

Finds the last occurrence of a specified character in a string. The return value
is a pointer to the found character in the given string or `NULL` if the
character is not found. Note that this function's signature does NOT match that
of the `strrchr` function.

`str`: The string to search for the character. `character`: The character to
find in the string.

#### Declaration ####

```
const char* SbStringFindLastCharacter(const char *str, char character)
```

### SbStringFindString ###

Finds the first occurrence of `str2` in `str1`. The return value is a pointer to
the beginning of the found string or `NULL` if the string is not found. This
function is meant to be a drop-in replacement for `strstr`.

`str1`: The string in which to search for the presence of `str2`. `str2`: The
string to locate in `str1`.

#### Declaration ####

```
const char* SbStringFindString(const char *str1, const char *str2)
```

### SbStringFormat ###

Produces a string formatted with `format` and `arguments`, placing as much of
the result that will fit into `out_buffer`. The return value specifies the
number of characters that the format would produce if `buffer_size` were
infinite.

This function is meant to be a drop-in replacement for `vsnprintf`.

`out_buffer`: The location where the formatted string is stored. `buffer_size`:
The size of `out_buffer`. `format`: A string that specifies how the data should
be formatted. `arguments`: Variable arguments used in the string.

#### Declaration ####

```
int SbStringFormat(char *out_buffer, size_t buffer_size, const char *format, va_list arguments) SB_PRINTF_FORMAT(3
```

### SbStringFormatF ###

An inline wrapper of SbStringFormat that converts from ellipsis to va_args. This
function is meant to be a drop-in replacement for `snprintf`.

`out_buffer`: The location where the formatted string is stored. `buffer_size`:
The size of `out_buffer`. `format`: A string that specifies how the data should
be formatted. `...`: Arguments used in the string.

#### Declaration ####

```
int static int static int SbStringFormatF(char *out_buffer, size_t buffer_size, const char *format,...) SB_PRINTF_FORMAT(3
```

### SbStringFormatUnsafeF ###

An inline wrapper of SbStringFormat that is meant to be a drop-in replacement
for the unsafe but commonly used `sprintf`.

`out_buffer`: The location where the formatted string is stored. `format`: A
string that specifies how the data should be formatted. `...`: Arguments used in
the string.

#### Declaration ####

```
static int static int SbStringFormatUnsafeF(char *out_buffer, const char *format,...) SB_PRINTF_FORMAT(2
```

### SbStringFormatWide ###

This function is identical to SbStringFormat, but is for wide characters. It is
meant to be a drop-in replacement for `vswprintf`.

`out_buffer`: The location where the formatted string is stored. `buffer_size`:
The size of `out_buffer`. `format`: A string that specifies how the data should
be formatted. `arguments`: Variable arguments used in the string.

#### Declaration ####

```
int SbStringFormatWide(wchar_t *out_buffer, size_t buffer_size, const wchar_t *format, va_list arguments)
```

### SbStringFormatWideF ###

An inline wrapper of SbStringFormatWide that converts from ellipsis to
`va_args`.

`out_buffer`: The location where the formatted string is stored. `buffer_size`:
The size of `out_buffer`. `format`: A string that specifies how the data should
be formatted. `...`: Arguments used in the string.

#### Declaration ####

```
static int SbStringFormatWideF(wchar_t *out_buffer, size_t buffer_size, const wchar_t *format,...)
```

### SbStringGetLength ###

Returns the length, in characters, of `str`.

`str`: A zero-terminated ASCII string.

#### Declaration ####

```
size_t SbStringGetLength(const char *str)
```

### SbStringGetLengthWide ###

Returns the length of a wide character string. (This function is the same as
SbStringGetLength, but for a string comprised of wide characters.) This function
assumes that there are no multi-element characters.

`str`: A zero-terminated ASCII string.

#### Declaration ####

```
size_t SbStringGetLengthWide(const wchar_t *str)
```

### SbStringParseDouble ###

Extracts a string that represents an integer from the beginning of `start` into
a double.

This function is meant to be a drop-in replacement for `strtod`, except that it
is explicitly declared to return a double.

`start`: The string that begins with the number to be converted. `out_end`: If
provided, the function places a pointer to the end of the consumed portion of
the string into `out_end`.

#### Declaration ####

```
double SbStringParseDouble(const char *start, char **out_end)
```

### SbStringParseSignedInteger ###

Extracts a string that represents an integer from the beginning of `start` into
a signed integer in the given `base`. This function is meant to be a drop-in
replacement for `strtol`.

`start`: The string that begins with the number to be converted. `out_end`: If
provided, the function places a pointer to the end of the consumed portion of
the string into `out_end`. `base`: The base into which the number will be
converted. The value must be between `2` and `36`, inclusive.
NOLINTNEXTLINE(runtime/int)

#### Declaration ####

```
long SbStringParseSignedInteger(const char *start, char **out_end, int base)
```

### SbStringParseUInt64 ###

Extracts a string that represents an integer from the beginning of `start` into
an unsigned 64-bit integer in the given `base`.

This function is meant to be a drop-in replacement for `strtoull`, except that
it is explicitly declared to return `uint64_t`.

`start`: The string that begins with the number to be converted. `out_end`: If
provided, the function places a pointer to the end of the consumed portion of
the string into `out_end`. `base`: The base into which the number will be
converted. The value must be between `2` and `36`, inclusive.

#### Declaration ####

```
uint64_t SbStringParseUInt64(const char *start, char **out_end, int base)
```

### SbStringParseUnsignedInteger ###

Extracts a string that represents an integer from the beginning of `start` into
an unsigned integer in the given `base`. This function is meant to be a drop-in
replacement for `strtoul`.

`start`: The string that begins with the number to be converted. `out_end`: If
provided, the function places a pointer to the end of the consumed portion of
the string into `out_end`. `base`: The base into which the number will be
converted. The value must be between `2` and `36`, inclusive.
NOLINTNEXTLINE(runtime/int)

#### Declaration ####

```
unsigned long SbStringParseUnsignedInteger(const char *start, char **out_end, int base)
```

### SbStringScan ###

Scans `buffer` for `pattern`, placing the extracted values in `arguments`. The
return value specifies the number of successfully matched items, which may be
`0`.

This function is meant to be a drop-in replacement for `vsscanf`.

`buffer`: The string to scan for the pattern. `pattern`: The string to search
for in `buffer`. `arguments`: Values matching `pattern` that were extracted from
`buffer`.

#### Declaration ####

```
int SbStringScan(const char *buffer, const char *pattern, va_list arguments)
```

### SbStringScanF ###

An inline wrapper of SbStringScan that converts from ellipsis to `va_args`. This
function is meant to be a drop-in replacement for `sscanf`. `buffer`: The string
to scan for the pattern. `pattern`: The string to search for in `buffer`. `...`:
Values matching `pattern` that were extracted from `buffer`.

#### Declaration ####

```
static int SbStringScanF(const char *buffer, const char *pattern,...)
```

