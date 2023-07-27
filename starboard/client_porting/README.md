# Client Porting Helpers

This Starboard sub-project contains libraries and headers that can be used to
make porting client software on top of Starboard a little easier.


# "Poem" - Lightweight POSIX Emulation

Before Starboard, we had tried to build a POSIX emulation layer for every
platform we wanted to support. This meant replacing link symbols in some cases,
and a wielding global preprocessor macros in others. It was impossible to keep
inclusion of platform headers (e.g. `winsock.h`) from transitively leaking out
into common code.

`poem`, for POsix EMulation, is somewhat similar, but a surgical tool for
targeted porting, rather than a blunt instrument. Because we have Starboard as a
well-defined barrier, we can use the preprocessor to selectively redirect
standard functions like malloc to Starboard without making lots of changes to
the code.

The rule we use is that these headers must ONLY be included in implementation
files, not headers, so that the preprocessor replacements don't spread
uncontrollably.

For each `poem`, we try to name files in a consistent, formulaic manner.  With
this scheme, we can easily determine the correct poem header while porting.
This can also come handy if one were to write a script to assist in porting
third party libraries.  In theory, the script would just look for includes
in .cc files, where we have available poems and then wrap unprotected includes
in #if !defined(STARBOARD), and include the poems if STARBOARD is defined.


# "eztime" - Easy Time Functions

Starboard does not contain functions that do much in the way of time
conversion. This is because system-provided time manipulation is often buggy,
broken, and/or limited. `eztime` is a simplified Straight-C wrapper on top of
ICU and Starboard that makes it easy to replace calls to functions like mktime
and gmtime in older C libraries with more robust ICU calls.

EzTime can be used directly by including
`starboard/client_porting/eztime/eztime.h`, or you can include
`starboard/client_porting/poem/eztime_poem.h` in your implementation file to
automatically simulate POSIXy time functions.
