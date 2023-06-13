---
layout: doc
title: "Starboard Module Reference: string.h"
---

Defines functions for interacting with c-style strings.

## Functions ##

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

### SbStringDuplicate ###

Copies `source` into a buffer that is allocated by this function and that can be
freed with SbMemoryDeallocate. This function is meant to be a drop-in
replacement for `strdup`.

`source`: The string to be copied.

#### Declaration ####

```
char* SbStringDuplicate(const char *source)
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
