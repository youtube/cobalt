Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Testing

Starboard 18 attempts to make the porting process as easy as possible. To that
end, Starboard 18 provides a compliance test suite, called NPLB
(No Platform Left Behind), that porters can use to gauge their progress.

## Current State

All of the APIs that Cobalt 27 defines are at least called by NPLB, and most of
those APIs are verified in one way or another. The APIs that are most likely
to just be implemented by a single system call, such as the Starboard functions
defined in [string.h](../reference/starboard/modules/string.md) and [memory.h](../reference/starboard/modules/memory.md)
are not exhaustively tested.

NPLB tests must work on all Starboard implementations, so they may make no
assumptions about platform-specific details. Rather, they attempt to define
a living contract for the APIs on all platforms.

## Test Organization

NPLB tests can be found in the `starboard/nplb/` directory and are broken
out into files by Starboard module and function:

```
starboard/nplb/<module>_<function>_test.cc
```

A significant portion of compliance tests, especially those replacing older
Starboard APIs, are located in `starboard/nplb/posix_compliance/` and follow
a `posix_<function>_test.cc` naming convention.

Although each test may incidentally test other functions, there is an attempt
to keep things as self-contained as possible within a function and module.
At the same time, the tests also aim to avoid testing behavior that is
effectively tested as part of another test.

For example, `sendto` and `recvfrom` (which replaced `SbSocketSendTo` and
`SbSocketReceiveFrom`) are tested together, ensuring that the API is
self-consistent on both sides of a connection. Therefore, only one set of tests
exist to cover those use cases, in
`starboard/nplb/posix_compliance/posix_socket_recvfrom_test.cc`.
