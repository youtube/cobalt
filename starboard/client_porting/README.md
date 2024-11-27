# Client Porting Helpers

This Starboard sub-project contains libraries and headers that can be used to
make porting client software on top of Starboard a little easier.


# "eztime" - Easy Time Functions

Starboard does not contain functions that do much in the way of time
conversion. This is because system-provided time manipulation is often buggy,
broken, and/or limited. `eztime` is a simplified Straight-C wrapper on top of
ICU and Starboard that makes it easy to replace calls to functions like mktime
and gmtime in older C libraries with more robust ICU calls.

EzTime can be used directly by including
`starboard/client_porting/eztime/eztime.h`.
