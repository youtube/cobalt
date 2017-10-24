---
layout: doc
title: "Starboard Module Reference: export.h"
---

Provides macros for properly exporting or importing symbols from shared
libraries.

## Macros

<div id="macro-documentation-section">

<h3 id="sb_export" class="small-h3">SB_EXPORT</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_export_private" class="small-h3">SB_EXPORT_PRIVATE</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_import" class="small-h3">SB_IMPORT</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

</div>

## Functions

### means

**Description**

COMPONENT_BUILD is defined when generating shared libraries for each project,

**Declaration**

```
// rather than static libraries. This means we need to be careful about
// EXPORT/IMPORT.
// SB_IS_LIBRARY is defined when building Starboard as a shared library to be
// linked into a client app. In this case, we want to explicitly define
// EXPORT/IMPORT so that Starboard's symbols are visible to such clients.
#if defined(STARBOARD_IMPLEMENTATION)
// STARBOARD_IMPLEMENTATION is defined when building the Starboard library
// sources, and shouldn't be defined when building sources that are clients of
// Starboard.
#else  // defined(STARBOARD_IMPLEMENTATION)
#endif
#else  // defined(COMPONENT_BUILD) || SB_IS(LIBRARY)
#define SB_EXPORT
#define SB_EXPORT_PRIVATE
#define SB_IMPORT
#endif  // defined(COMPONENT_BUILD)
#endif  // STARBOARD_EXPORT_H_
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>we need to be careful</code><br>
        <code>about</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>//</code><br>
        <code>EXPORT/IMPORT.</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>// SB_IS_LIBRARY is defined when building Starboard as a shared library to</code><br>
        <code>be</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>// linked into a client app. In this</code><br>
        <code>case</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>we want to explicitly</code><br>
        <code>define</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>// EXPORT/IMPORT so that Starboard's symbols are visible to such</code><br>
        <code>clients.</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>#if</code><br>
        <code>defined(STARBOARD_IMPLEMENTATION</code></td>
    <td> </td>
  </tr>
</table>

