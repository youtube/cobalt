---
layout: doc
title: "Starboard Module Reference: character.h"
---

Provides functions for interacting with characters.

## Functions

### SbCharacterIsAlphanumeric

**Description**

Indicates whether the given 8-bit character `c` (as an int) is alphanumeric
in the current locale.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCharacterIsAlphanumeric-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCharacterIsAlphanumeric-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCharacterIsAlphanumeric-declaration">
<pre>
SB_EXPORT bool SbCharacterIsAlphanumeric(int c);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCharacterIsAlphanumeric-stub">

```
#include "starboard/character.h"

bool SbCharacterIsAlphanumeric(int /*c*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>c</code></td>
    <td>The character to be evaluated.</td>
  </tr>
</table>

### SbCharacterIsDigit

**Description**

Indicates whether the given 8-bit character `c` (as an int) is a
decimal digit in the current locale.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCharacterIsDigit-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCharacterIsDigit-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCharacterIsDigit-declaration">
<pre>
SB_EXPORT bool SbCharacterIsDigit(int c);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCharacterIsDigit-stub">

```
#include "starboard/character.h"

bool SbCharacterIsDigit(int /*c*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>c</code></td>
    <td>The character to be evaluated.</td>
  </tr>
</table>

### SbCharacterIsHexDigit

**Description**

Indicates whether the given 8-bit character `c` (as an int) is a hexadecimal
in the current locale.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCharacterIsHexDigit-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCharacterIsHexDigit-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCharacterIsHexDigit-declaration">
<pre>
SB_EXPORT bool SbCharacterIsHexDigit(int c);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCharacterIsHexDigit-stub">

```
#include "starboard/character.h"

bool SbCharacterIsHexDigit(int /*c*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>c</code></td>
    <td>The character to be evaluated.</td>
  </tr>
</table>

### SbCharacterIsSpace

**Description**

Indicates whether the given 8-bit character `c` (as an int) is a space in
the current locale.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCharacterIsSpace-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCharacterIsSpace-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCharacterIsSpace-declaration">
<pre>
SB_EXPORT bool SbCharacterIsSpace(int c);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCharacterIsSpace-stub">

```
#include "starboard/character.h"

bool SbCharacterIsSpace(int /*c*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>c</code></td>
    <td>The character to be evaluated.</td>
  </tr>
</table>

### SbCharacterIsUpper

**Description**

Indicates whether the given 8-bit character `c` (as an int) is uppercase
in the current locale.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCharacterIsUpper-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCharacterIsUpper-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCharacterIsUpper-declaration">
<pre>
SB_EXPORT bool SbCharacterIsUpper(int c);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCharacterIsUpper-stub">

```
#include "starboard/character.h"

bool SbCharacterIsUpper(int /*c*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>c</code></td>
    <td>The character to be evaluated.</td>
  </tr>
</table>

### SbCharacterToLower

**Description**

Converts the given 8-bit character (as an int) to lowercase in the current
locale and returns an 8-bit character. If there is no lowercase version of
the character, or the character is already lowercase, the function just
returns the character as-is.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCharacterToLower-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCharacterToLower-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCharacterToLower-declaration">
<pre>
SB_EXPORT int SbCharacterToLower(int c);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCharacterToLower-stub">

```
#include "starboard/character.h"

int SbCharacterToLower(int c) {
  return c;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>c</code></td>
    <td>The character to be converted.</td>
  </tr>
</table>

### SbCharacterToUpper

**Description**

Converts the given 8-bit character (as an int) to uppercase in the current
locale and returns an 8-bit character. If there is no uppercase version of
the character, or the character is already uppercase, the function just
returns the character as-is.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbCharacterToUpper-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbCharacterToUpper-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbCharacterToUpper-declaration">
<pre>
SB_EXPORT int SbCharacterToUpper(int c);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbCharacterToUpper-stub">

```
#include "starboard/character.h"

int SbCharacterToUpper(int c) {
  return c;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>c</code></td>
    <td>The character to be converted.</td>
  </tr>
</table>

