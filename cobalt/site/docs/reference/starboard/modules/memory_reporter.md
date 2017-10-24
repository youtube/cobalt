---
layout: doc
title: "Starboard Module Reference: memory_reporter.h"
---

Provides an interface for memory reporting.

## Functions

### SbMemorySetReporter

**Description**

Sets the MemoryReporter. Any previous memory reporter is unset. No lifetime
management is done internally on input pointer.<br>
Returns true if the memory reporter was set with no errors. If an error
was reported then check the log for why it failed.<br>
Note that other than a thread-barrier-write of the input pointer, there is
no thread safety guarantees with this function due to performance
considerations. It's recommended that this be called once during the
lifetime of the program, or not at all. Do not delete the supplied pointer,
ever.
Example (Good):
SbMemoryReporter* mem_reporter = new ...;
SbMemorySetReporter(&mem_reporter);
...
SbMemorySetReporter(NULL);  // allow value to leak.
Example (Bad):
SbMemoryReporter* mem_reporter = new ...;
SbMemorySetReporter(&mem_reporter);
...
SbMemorySetReporter(NULL);
delete mem_reporter;        // May crash.

**Declaration**

```
SB_EXPORT bool SbMemorySetReporter(struct SbMemoryReporter* tracker);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>struct SbMemoryReporter*</code><br>
        <code>tracker</code></td>
    <td> </td>
  </tr>
</table>

