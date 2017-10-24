---
layout: doc
title: "Starboard Module Reference: byte_swap.h"
---

Specifies functions for swapping byte order. These functions are used to
deal with endianness when performing I/O.

## Macros

<div id="macro-documentation-section">

<h3 id="sb_host_to_net_s16" class="small-h3">SB_HOST_TO_NET_S16</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_host_to_net_s32" class="small-h3">SB_HOST_TO_NET_S32</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_host_to_net_s64" class="small-h3">SB_HOST_TO_NET_S64</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_host_to_net_u16" class="small-h3">SB_HOST_TO_NET_U16</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_host_to_net_u32" class="small-h3">SB_HOST_TO_NET_U32</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_host_to_net_u64" class="small-h3">SB_HOST_TO_NET_U64</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_net_to_host_s16" class="small-h3">SB_NET_TO_HOST_S16</h3>

<h3 id="sb_net_to_host_s32" class="small-h3">SB_NET_TO_HOST_S32</h3>

<h3 id="sb_net_to_host_s64" class="small-h3">SB_NET_TO_HOST_S64</h3>

<h3 id="sb_net_to_host_u16" class="small-h3">SB_NET_TO_HOST_U16</h3>

<h3 id="sb_net_to_host_u32" class="small-h3">SB_NET_TO_HOST_U32</h3>

<h3 id="sb_net_to_host_u64" class="small-h3">SB_NET_TO_HOST_U64</h3>

</div>

## Functions

### SbByteSwapS16

**Description**

Unconditionally swaps the byte order in signed 16-bit `value`.

**Declaration**

```
SB_EXPORT int16_t SbByteSwapS16(int16_t value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int16_t</code><br>        <code>value</code></td>
    <td>The value for which the byte order will be swapped.</td>
  </tr>
</table>

### SbByteSwapS32

**Description**

Unconditionally swaps the byte order in signed 32-bit `value`.

**Declaration**

```
SB_EXPORT int32_t SbByteSwapS32(int32_t value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int32_t</code><br>        <code>value</code></td>
    <td>The value for which the byte order will be swapped.</td>
  </tr>
</table>

### SbByteSwapS64

**Description**

Unconditionally swaps the byte order in signed 64-bit `value`.

**Declaration**

```
SB_EXPORT int64_t SbByteSwapS64(int64_t value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int64_t</code><br>        <code>value</code></td>
    <td>The value for which the byte order will be swapped.</td>
  </tr>
</table>

### SbByteSwapU16

**Description**

Unconditionally swaps the byte order in unsigned 16-bit `value`.

**Declaration**

```
SB_EXPORT uint16_t SbByteSwapU16(uint16_t value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>uint16_t</code><br>        <code>value</code></td>
    <td>The value for which the byte order will be swapped.</td>
  </tr>
</table>

### SbByteSwapU32

**Description**

Unconditionally swaps the byte order in unsigned 32-bit `value`.

**Declaration**

```
SB_EXPORT uint32_t SbByteSwapU32(uint32_t value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>uint32_t</code><br>        <code>value</code></td>
    <td>The value for which the byte order will be swapped.</td>
  </tr>
</table>

### SbByteSwapU64

**Description**

Unconditionally swaps the byte order in unsigned 64-bit `value`.

**Declaration**

```
SB_EXPORT uint64_t SbByteSwapU64(uint64_t value);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>uint64_t</code><br>        <code>value</code></td>
    <td>The value for which the byte order will be swapped.</td>
  </tr>
</table>

