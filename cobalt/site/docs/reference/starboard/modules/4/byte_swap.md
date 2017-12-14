---
layout: doc
title: "Starboard Module Reference: byte_swap.h"
---

Specifies functions for swapping byte order. These functions are used to deal
with endianness when performing I/O.

## Functions ##

### SbByteSwapS16 ###

Unconditionally swaps the byte order in signed 16-bit `value`. `value`: The
value for which the byte order will be swapped.

#### Declaration ####

```
int16_t SbByteSwapS16(int16_t value)
```

### SbByteSwapS32 ###

Unconditionally swaps the byte order in signed 32-bit `value`. `value`: The
value for which the byte order will be swapped.

#### Declaration ####

```
int32_t SbByteSwapS32(int32_t value)
```

### SbByteSwapS64 ###

Unconditionally swaps the byte order in signed 64-bit `value`. `value`: The
value for which the byte order will be swapped.

#### Declaration ####

```
int64_t SbByteSwapS64(int64_t value)
```

### SbByteSwapU16 ###

Unconditionally swaps the byte order in unsigned 16-bit `value`. `value`: The
value for which the byte order will be swapped.

#### Declaration ####

```
uint16_t SbByteSwapU16(uint16_t value)
```

### SbByteSwapU32 ###

Unconditionally swaps the byte order in unsigned 32-bit `value`. `value`: The
value for which the byte order will be swapped.

#### Declaration ####

```
uint32_t SbByteSwapU32(uint32_t value)
```

### SbByteSwapU64 ###

Unconditionally swaps the byte order in unsigned 64-bit `value`. `value`: The
value for which the byte order will be swapped.

#### Declaration ####

```
uint64_t SbByteSwapU64(uint64_t value)
```

