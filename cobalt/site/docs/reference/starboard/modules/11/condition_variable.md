---
layout: doc
title: "Starboard Module Reference: condition_variable.h"
---

Defines an interface for condition variables.

## Enums ##

### SbConditionVariableResult ###

Enumeration of possible results from waiting on a condvar.

#### Values ####

*   `kSbConditionVariableSignaled`

    The wait completed because the condition variable was signaled.
*   `kSbConditionVariableTimedOut`

    The wait completed because it timed out, and was not signaled.
*   `kSbConditionVariableFailed`

    The wait failed, either because a parameter wasn't valid, or the condition
    variable has already been destroyed, or something similar.

## Functions ##

### SbConditionVariableBroadcast ###

Broadcasts to all current waiters of `condition` to stop waiting. This function
wakes all of the threads waiting on `condition` while SbConditionVariableSignal
wakes a single thread.

`condition`: The condition that should no longer be waited for.

#### Declaration ####

```
bool SbConditionVariableBroadcast(SbConditionVariable *condition)
```

### SbConditionVariableCreate ###

Creates a new condition variable to work with `opt_mutex`, which may be null,
placing the newly created condition variable in `out_condition`.

The return value indicates whether the condition variable could be created.

#### Declaration ####

```
bool SbConditionVariableCreate(SbConditionVariable *out_condition, SbMutex *opt_mutex)
```

### SbConditionVariableDestroy ###

Destroys the specified SbConditionVariable. The return value indicates whether
the destruction was successful. The behavior is undefined if other threads are
currently waiting on this condition variable.

`condition`: The SbConditionVariable to be destroyed. This invalidates the
condition variable.

#### Declaration ####

```
bool SbConditionVariableDestroy(SbConditionVariable *condition)
```

### SbConditionVariableIsSignaled ###

Returns whether the given result is a success.

#### Declaration ####

```
static bool SbConditionVariableIsSignaled(SbConditionVariableResult result)
```

### SbConditionVariableSignal ###

Signals the next waiter of `condition` to stop waiting. This function wakes a
single thread waiting on `condition` while SbConditionVariableBroadcast wakes
all threads waiting on it.

`condition`: The condition that the waiter should stop waiting for.

#### Declaration ####

```
bool SbConditionVariableSignal(SbConditionVariable *condition)
```

### SbConditionVariableWait ###

Waits for `condition`, releasing the held lock `mutex`, blocking indefinitely,
and returning the result. Behavior is undefined if `mutex` is not held.

#### Declaration ####

```
SbConditionVariableResult SbConditionVariableWait(SbConditionVariable *condition, SbMutex *mutex)
```

### SbConditionVariableWaitTimed ###

Waits for `condition`, releasing the held lock `mutex`, blocking up to
`timeout_duration`, and returning the acquisition result. Behavior is undefined
if `mutex` is not held.

`timeout_duration`: The maximum amount of time that function should wait for
`condition`. If the `timeout_duration` value is less than or equal to zero, the
function returns as quickly as possible with a kSbConditionVariableTimedOut
result.

#### Declaration ####

```
SbConditionVariableResult SbConditionVariableWaitTimed(SbConditionVariable *condition, SbMutex *mutex, SbTime timeout_duration)
```

