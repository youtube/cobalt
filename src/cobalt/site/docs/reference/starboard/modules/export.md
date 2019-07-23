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
EXPORT/IMPORT.
