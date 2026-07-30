Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Testing

Starboard 18 aims to make the porting process as easy as possible. To that
end, it provides a compliance test suite, called NPLB
(No Platform Left Behind), that porters can use to gauge their progress.

## Current State

NPLB calls all APIs defined by Cobalt 27 and verifies most of them. APIs
typically implemented by a single system call are not exhaustively tested.

Starboard also implements many POSIX APIs that Cobalt 27 requires, some of which
replace earlier Starboard APIs, such as `sendto` replacing `SbSocketSendTo`.
These APIs are also tested by NPLB, except for the following functions known to
be missing tests:

  * `fstatat`
  * `openat`
  * `unlinkat`

Because NPLB tests must work on all Starboard implementations, they make
no assumptions about platform-specific details. Instead, they define a living
contract for the APIs across all platforms.

## Test Organization

NPLB tests can be found in the `starboard/nplb/` directory and are broken
out into files by Starboard module and function:

```
starboard/nplb/<module>_<function>_test.cc
```

A significant portion of compliance tests, especially those replacing older
Starboard APIs, are located in `starboard/nplb/posix_compliance/` and follow
a `posix_<function>_test.cc` naming convention.

Although each test may incidentally test other functions, they are designed
to be as self-contained as possible within a function and module. At the same
time, the tests also aim to avoid repeating checks that are covered elsewhere.

For example, the `sendto` and `recvfrom` functions, which replaced `SbSocketSendTo`
and `SbSocketReceiveFrom`, are tested together to ensure the API is
self-consistent on both sides of a connection. Consequently, only one set of
tests exists to cover these use cases, located in
`starboard/nplb/posix_compliance/posix_socket_recvfrom_test.cc`.
