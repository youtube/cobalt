# Blink Scheduler

This directory contains the Blink Scheduler, which coordinates task execution
in renderer processes. The main subdirectories are:

- `public` -- contains the interfaces which scheduler exposes to the other parts
  of Blink (`FrameScheduler`, `PageScheduler`, `WorkerScheduler` and others).
  Other code in Blink should not depend on files outside of this directory.
- `common` -- contains `ThreadSchedulerImpl` which is the base class for all
  thread schedulers, as well as other functionality which is required for both
  main thread and worker threads.
- `main_thread` -- contains implementation of the main thread scheduler
  (`MainThreadSchedulerImpl`) and main thread scheduling policies.
- `worker` -- contains implementation of scheduling infrastructure for
  the non-main threads (compositor thread, worker threads).

The scheduler exposes an API to the content layer at
`public/platform/scheduler`.

# Further reading

[Overview of task scheduling in Blink](TaskSchedulingInBlink.md).
[Collection of scheduling-related documentation](links.md).

