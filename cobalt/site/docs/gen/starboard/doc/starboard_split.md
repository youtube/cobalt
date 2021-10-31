---
layout: doc
title: "Separation of Starboard from Cobalt"
---
# Separation of Starboard from Cobalt

### Background

In creating Cobalt Evergreen, we needed to have as few differences in the code
as we could from one platform to another. Simply put, that means we want to get
rid of as many platform specific compile time macros as possible. Because of
the large amount of platform differences we had previously, there were macros
defined in a variety places, all of which require different cases to convert.
The below sections go into the differences regarding changes to these macros
from Cobalt 20 to 21 and from Starboard 11 to 12.

## Optional APIs

In previous versions, there were a few APIs that could optionally defined and
would only be enabled if certain macros were set. The platform would only have
to implement the API if it set the macros accordingly. Now, those differences
have been removed, and all platforms are required to implement these APIs. For
convenience, we have provided stub functions for all implementations, which a
platform can use without differences in Cobalt if they did not enable the API
beforehand.

## Migration from Configuration Macros to External Constants

Starboard configurations have moved from
[configuration.h](../configuration.h) and platform specific
`configuration_public.h` files to `configuration_constants.cc` files. They
have also been changes from macros to `extern const`s. This means that Cobalt
will be able to evaluate these variables at runtime instead of compile time.
The declarations for all of these variables are located in
[configuration_constants.h](../configuration_constants.h), but each platform
must define each variable individually, i.e. as
```
// configuration_constants.cc
#include "starboard/configuration_constants.h"

const int32_t kSbFileMaxName = 64;
```
There are two changes that are a result of this migration:

1. There is no longer any form of inheritance. This means that if there are any
configuration differences between two platforms, they must have separate
`configuration_constants.cc` files.
2. There are no longer default values for any variable. Because we cannot
inherit values, we would not be able to receive a default value for any one, so
each platform must have a definition for every configuration variable.

## Migration from GYP Variables to Cobalt Extensions

Cobalt configurations have moved from [cobalt_configuration.gypi](../../cobalt/build/cobalt_configuration.gypi) and platform specific `gyp_configuration.gypi` files to Cobalt extensions, primarily [CobaltExtensionConfigurationApi](../../cobalt/extension/configuration.h), but including the [CobaltExtensionGraphicsApi](../../cobalt/extension/graphics.h).

Some variables were already in the process of being deprecated, sometimes with a replacement. In those cases, that path was followed.

Implementing the Cobalt extension is completely optional, and when calling the functions corresponding to the old GYP variable, there will be a default value that the function will be able to fall back onto if the extension has not been implemented. That being said, if there is a single function the platform needs a custom implementation for, it must completely implement the CobaltExtensionConfigurationApi. For convenience, we have provided default functions to use to define the API if desired.

##### Notes

For migrating from macros, partners will have to treat their use differently! I.e. string literal vs const char* in their implementations. Add an overview of all of the cases of this migration in runtime code.
