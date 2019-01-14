---
layout: doc
title: "Starboard Module Reference: export.h"
---

Provides macros for properly exporting or importing symbols from shared
libraries.

## Macros ##

### SB_EXPORT ###

COMPONENT_BUILD is defined when generating shared libraries for each project,
rather than static libraries. This means we need to be careful about
EXPORT/IMPORT. SB_IS_LIBRARY is defined when building Starboard as a shared
library to be linked into a client app. In this case, we want to explicitly
define EXPORT/IMPORT so that Starboard's symbols are visible to such clients.
