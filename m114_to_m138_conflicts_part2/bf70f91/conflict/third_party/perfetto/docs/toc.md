<<<<<<< HEAD
* [Introduction](README.md)

* [Overview](#)
  * [Tracing 101](tracing-101.md)
  * [FAQ](faq.md)
=======
* [Overview](README.md)

* [Tracing 101](tracing-101.md)
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

* [Quickstart](#)
  * [Record traces on Android](quickstart/android-tracing.md)
  * [Record traces on Linux](quickstart/linux-tracing.md)
  * [Record Chrome traces](quickstart/chrome-tracing.md)
  * [SQL analysis and metrics](quickstart/trace-analysis.md)
  * [Trace conversion](quickstart/traceconv.md)
  * [Heap profiling](quickstart/heap-profiling.md)
  * [Callstack sampling on Android](quickstart/callstack-sampling.md)

<<<<<<< HEAD
* [Case studies](#)
  * [Tracing Android boot](case-studies/android-boot-tracing.md)
=======
* [FAQ](faq.md)

* [Case studies](#)
  * [Android boot tracing](case-studies/android-boot-tracing.md)
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  * [Debugging memory usage](case-studies/memory.md)

* [Data sources](#)
  * [Memory](#)
    * [Counters and events](data-sources/memory-counters.md)
    * [Heap profiler](data-sources/native-heap-profiler.md)
    * [Java heap dumps](data-sources/java-heap-profiler.md)
  * [CPU](#)
    * [Scheduling events](data-sources/cpu-scheduling.md)
    * [System calls](data-sources/syscalls.md)
    * [Frequency scaling](data-sources/cpu-freq.md)
  * [Power](#)
    * [Battery counters and rails](data-sources/battery-counters.md)
  * [Android system](#)
    * [Android game intervention list](data-sources/android-game-intervention-list.md)
    * [Atrace instrumentation](data-sources/atrace.md)
    * [Android log (logcat)](data-sources/android-log.md)
    * [Android Janks](data-sources/frametimeline.md)
<<<<<<< HEAD
  * [Linux system](#)
    * [Tracing across reboot](data-sources/previous-boot-trace.md)
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

* [App Instrumentation](#)
  * [Tracing SDK](instrumentation/tracing-sdk.md)
  * [Track events](instrumentation/track-events.md)
  * [Interceptors](instrumentation/interceptors.md)

* [Trace analysis](#)
<<<<<<< HEAD
  * [Trace Processor](#)
    * [C++ library](analysis/trace-processor.md)
    * [Python library](analysis/trace-processor-python.md)
    * [Trace-based metrics](analysis/metrics.md)
  * [PerfettoSQL](#)
    * [Standard Library](analysis/stdlib-docs.autogen)
    * [Syntax](analysis/perfetto-sql-syntax.md)
    * [Style Guide](analysis/style-guide.md)
    * [Prelude tables](analysis/sql-tables.autogen)
    * [Built-ins](analysis/builtin.md)
    * [Backwards Compatibility](analysis/perfetto-sql-backcompat.md)
  * [Analysis at scale](#)
    * [Batch Trace Processor](analysis/batch-trace-processor.md)
    * [Bigtrace](deployment/deploying-bigtrace-on-a-single-machine.md)
    * [Bigtrace on Kubernetes](deployment/deploying-bigtrace-on-kubernetes.md)
=======
  * [Trace Processor (SQL)](analysis/trace-processor.md)
  * [Batch Trace Processor](analysis/batch-trace-processor.md)
  * [Standard library](analysis/stdlib-docs.autogen)
  * [Built-in Functions](analysis/builtin.md)
  * [Trace-based metrics](analysis/metrics.md)
  * [Common queries](analysis/common-queries.md)
  * [SQL tables](analysis/sql-tables.autogen)
  * [Stats table](analysis/sql-stats.autogen)
  * [Pivot tables](analysis/pivot-tables.md)
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

* [Trace visualization](#)
  * [Perfetto UI](visualization/perfetto-ui.md)
  * [Visualising large traces](visualization/large-traces.md)
  * [Deep linking to Perfetto UI](visualization/deep-linking-to-perfetto-ui.md)
  * [Perfetto UI release process](visualization/perfetto-ui-release-process.md)
<<<<<<< HEAD
  * [Pivot tables](analysis/pivot-tables.md)
  * [Debug tracks](analysis/debug-tracks.md)
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

* [Core concepts](#)
  * [Trace configuration](concepts/config.md)
  * [Buffers and dataflow](concepts/buffers.md)
  * [Service model](concepts/service-model.md)
  * [Clock synchronization](concepts/clock-sync.md)
  * [Detached mode](concepts/detached-mode.md)
<<<<<<< HEAD
  * [Concurrent tracing sessions](concepts/concurrent-tracing-sessions.md)
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

* [Reference](#)
  * [Trace Config proto](reference/trace-config-proto.autogen)
  * [Trace Packet proto](reference/trace-packet-proto.autogen)
  * [perfetto cmdline](reference/perfetto-cli.md)
  * [heap_profile cmdline](reference/heap_profile-cli.md)
  * [Synthetic TrackEvent](reference/synthetic-track-event.md)
<<<<<<< HEAD
  * [Android Version Notes](reference/android-version-notes.md)
  * [Stats table](analysis/sql-stats.autogen)
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

* [Contributing](#)
    * [Getting started](contributing/getting-started.md)
    * [Build instructions](contributing/build-instructions.md)
    * [Running tests](contributing/testing.md)
<<<<<<< HEAD
    * [UI plugins](contributing/ui-plugins.md)
    * [UI development hints](contributing/ui-development.md)
    * [Embedding Perfetto](contributing/embedding.md)
    * [Releasing the SDK](contributing/sdk-releasing.md)
    * [Chrome branches](contributing/chrome-branches.md)
    * [Common tasks](contributing/common-tasks.md)
=======
    * [Common tasks](contributing/common-tasks.md)
    * [Embedding Perfetto](contributing/embedding.md)
    * [Releasing the SDK](contributing/sdk-releasing.md)
    * [Chrome branches](contributing/chrome-branches.md)
    * [UI development](contributing/ui-development.md)
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    * [Press](contributing/perfetto-in-the-press.md)

* [Design documents](#)
    * [API and ABI surface](design-docs/api-and-abi.md)
    * [Batch Trace Processor](design-docs/batch-trace-processor.md)
    * [Heapprofd design](design-docs/heapprofd-design.md)
    * [Heapprofd wire protocol](design-docs/heapprofd-wire-protocol.md)
    * [Heapprofd sampling](design-docs/heapprofd-sampling.md)
    * [Life of a tracing session](design-docs/life-of-a-tracing-session.md)
    * [Perfetto CI](design-docs/continuous-integration.md)
    * [ProtoZero](design-docs/protozero.md)
    * [Security model](design-docs/security-model.md)
    * [Statsd Checkpoint Atoms](design-docs/checkpoint-atoms.md)

