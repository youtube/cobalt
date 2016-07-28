# Libevent Starboard Implementation

[Libevent](http://libevent.org) is a library that abstracts several methods of
multiplexing heterogeneous events into a single event loop. Event sources may
include:

 * Input/Output
 * Timers
 * Pipes

[Multiplexing methods](http://www.ulduzsoft.com/2014/01/select-poll-epoll-practical-difference-for-system-architects/) supported by libevent include:

 * epoll()
 * kqueue
 * POSIX select()
 * poll()
 * Windows select()
 * others...

This implementation only uses a subset of the interface described in libevent
1.4.x, and not any of the new libevent 2.x.x interface. To use this
implementation, you must provide a libevent 1.4.x implementation that works on
your platform, or just build and use the copy provided, if your version of
Starboard included one.
