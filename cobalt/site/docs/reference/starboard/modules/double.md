---
layout: doc
title: "Starboard Module Reference: double.h"
---

Provides double-precision floating point helper functions.

## Functions

### SbDoubleAbsolute

**Description**

Returns the absolute value of the given double-precision floating-point
number `d`, preserving `NaN` and infinity.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDoubleAbsolute-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDoubleAbsolute-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDoubleAbsolute-declaration">
<pre>
SB_EXPORT double SbDoubleAbsolute(const double d);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDoubleAbsolute-stub">

```
#include "starboard/double.h"

double SbDoubleAbsolute(double /*d*/) {
  return 0.0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const double</code><br>        <code>d</code></td>
    <td>The number to be adjusted.</td>
  </tr>
</table>

### SbDoubleExponent

**Description**

Returns `base` taken to the power of `exponent`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDoubleExponent-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDoubleExponent-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDoubleExponent-declaration">
<pre>
SB_EXPORT double SbDoubleExponent(const double base, const double exponent);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDoubleExponent-stub">

```
#include "starboard/double.h"

double SbDoubleExponent(const double /*base*/, const double /*exponent*/) {
  return 0.0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const double</code><br>        <code>base</code></td>
    <td>The number to be adjusted.</td>
  </tr>
  <tr>
    <td><code>const double</code><br>        <code>exponent</code></td>
    <td>The power to which the <code>base</code> number should be raised.</td>
  </tr>
</table>

### SbDoubleFloor

**Description**

Floors double-precision floating-point number `d` to the nearest integer.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDoubleFloor-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDoubleFloor-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDoubleFloor-declaration">
<pre>
SB_EXPORT double SbDoubleFloor(const double d);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDoubleFloor-stub">

```
#include "starboard/double.h"

double SbDoubleFloor(double /*d*/) {
  return 0.0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const double</code><br>        <code>d</code></td>
    <td>The number to be floored.</td>
  </tr>
</table>

### SbDoubleIsFinite

**Description**

Determines whether double-precision floating-point number `d` represents a
finite number.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDoubleIsFinite-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDoubleIsFinite-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDoubleIsFinite-declaration">
<pre>
SB_EXPORT bool SbDoubleIsFinite(const double d);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDoubleIsFinite-stub">

```
#include "starboard/double.h"

bool SbDoubleIsFinite(const double /*d*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const double</code><br>        <code>d</code></td>
    <td>The number to be evaluated.</td>
  </tr>
</table>

### SbDoubleIsNan

**Description**

Determines whether double-precision floating-point number `d` represents
"Not a Number."

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDoubleIsNan-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDoubleIsNan-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDoubleIsNan-declaration">
<pre>
SB_EXPORT bool SbDoubleIsNan(const double d);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDoubleIsNan-stub">

```
#include "starboard/double.h"

bool SbDoubleIsNan(const double /*d*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const double</code><br>        <code>d</code></td>
    <td>The number to be evaluated.</td>
  </tr>
</table>

