---
layout: doc
title: "Starboard Module Reference: thread_types.h"
---

Defines platform-specific threading and synchronization primitive types and
initializers. We hide, but pass through, the platform's primitives to avoid
doing a lot of work to replicate initialization-less synchronization primitives.
