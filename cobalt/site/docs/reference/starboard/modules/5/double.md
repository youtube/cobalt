---
layout: doc
title: "Starboard Module Reference: double.h"
---

Provides double-precision floating point helper functions.

## Functions ##

### SbDoubleAbsolute ###

Returns the absolute value of the given double-precision floating-point number
`d`, preserving `NaN` and infinity.

`d`: The number to be adjusted.

#### Declaration ####

```
double SbDoubleAbsolute(const double d)
```

### SbDoubleExponent ###

Returns `base` taken to the power of `exponent`.

`base`: The number to be adjusted. `exponent`: The power to which the `base`
number should be raised.

#### Declaration ####

```
double SbDoubleExponent(const double base, const double exponent)
```

### SbDoubleFloor ###

Floors double-precision floating-point number `d` to the nearest integer.

`d`: The number to be floored.

#### Declaration ####

```
double SbDoubleFloor(const double d)
```

### SbDoubleIsFinite ###

Determines whether double-precision floating-point number `d` represents a
finite number.

`d`: The number to be evaluated.

#### Declaration ####

```
bool SbDoubleIsFinite(const double d)
```

### SbDoubleIsNan ###

Determines whether double-precision floating-point number `d` represents "Not a
Number."

`d`: The number to be evaluated.

#### Declaration ####

```
bool SbDoubleIsNan(const double d)
```

