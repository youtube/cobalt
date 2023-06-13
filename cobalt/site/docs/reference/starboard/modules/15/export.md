---
layout: doc
title: "Starboard Module Reference: export.h"
---

Provides macros for properly exporting or importing symbols from shared
libraries.

## Macros ##

### SB_EXPORT ###

Specification for a symbol that should be exported when building the DLL and
imported when building code that uses the DLL.

### SB_EXPORT_PRIVATE ###

Specification for a symbol that should be exported or imported for testing
purposes only.

### SB_IMPORT ###

Specification for a symbol that is expected to be defined externally to this
module.
