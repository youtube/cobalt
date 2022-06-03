# Watchdog

Watchdog is Cobalt's monitoring service used to detect non-responsive events
and trigger crashes. Similar to Android's ANRs.

Watchdog behaves as a singleton which provides global access to the one
instance of the class. The one Watchdog instance is created and deleted by
Application which not only manages Watchdog's lifecycle, but also updates
Watchdog's state.

Watchdog works by maintaining a dictionary of registered Clients with various
metadata including timestamps of when the Client last pinged Watchdog. Watchdog
then has a separate thread that monitors these registered Clients by
periodically checking if their last pinged timestamps exceed a time constraint.
The goal is to have Watchdog as minimal as possible while effectively detecting
non-responsive events. To that end, components to be monitored are themselves
responsible for registering, pinging, and unregistering themselves by calling
Watchdog functions.


## Quick Guide to Implementing Watchdog

### I. Periodic Behavior / Internal

To implement periodic behavior monitoring, simply call Register() before the
periodic behavior begins, Ping() during the periodic behavior, and Unregister()
after the periodic behavior. Register() parameters should be adjusted
accordingly with the time_interval often a few orders of magnitude greater than
expected.

If there is no simple place to call Register() before the period behavior
begins, it is possible to call Register() in place of calling Ping() and to
simply adjust the replace parameter accordingly.

Watchdog implementations assume that Application has already created the one
Watchdog instance, but implementations often expect the possibility of a
nullptr in which case no action is taken. This is because various unit and
black box tests do not start Cobalt with normal usage patterns.

### II. Liveness check / External

To implement liveness check behavior monitoring, a Javascript adapter layer
will need to be implemented that handles the periodic pinging and callbacks
required.

### III. Debugging

Watchdog also provides a function MaybeInjectDelay() that can be used to
artificially inject thread sleeps to create Watchdog violations. This is
especially useful for testing and should be placed near where Ping() calls are
made.

The delay works by reading the watchdog command line switch which takes in a
comma separated string with settings that correspond to
delay_name_,delay_wait_time_microseconds_,delay_sleep_time_microseconds_
(ex. "renderer,100000,1000"). The delay name is the name of the Watchdog client
to inject delay, the delay wait is the time in between delays (periodic), and
the delay sleep is the delay duration. They have default values of empty, 0,
and 0 respectively.
