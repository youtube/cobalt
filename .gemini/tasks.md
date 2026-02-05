# Tasks

## Caveats

 - [x] Note: All code changes should be made within //cobalt.
 - [x] Any change within Piper or Google3 should be left as a suggestion as a follow-up to the user. Leave the findings in a TODO.md at the root directory.
 - [x] For each task that requires a code change, make a new branch for it that has a reasonable name.
 - [x] For each change, make sure they are well-tested, using both cobalt\_unittests and cobalt\_browsertests.
 - [x] For each change, make sure they adhere to Chromium's C++, Java, and Python style guides.
 - [x] For each change, make sure they adhere to the best practices described in the docs under //docs.
 - [x] Do not modify any local existing branches or push to upstream GitHub. Keep the branches local and list GitHub actions in the TODO markdown file.

## Research

- [x] Research histograms related to memory for both //chrome and //android\_webview, including their shared layers like blink and content and components.
- [x] Research existing work surrounding histograms for Cobalt under //cobalt in origin/main, add-resident-memory-metrics, and prototype-memory-metrics-dev branches.

## Action

- [x] Component-Level Breakdowns: Even within a single process, Cobalt can attribute memory to major subsystems. This can be achieved by instrumenting allocators or using Chromium's Memory Infra primitives:
  - [x] Cobalt.Memory.JavaScript: Track memory used by the V8 heap.
  - [x] Cobalt.Memory.DOM: Memory consumed by DOM nodes and related structures.
  - [x] Cobalt.Memory.Layout: Memory for layout trees and rendering data.
  - [x] Cobalt.Memory.Graphics: Memory used by Skia, GPU resources, textures, etc.
  - [x] Cobalt.Memory.Media: Buffers and resources used by the media pipeline.
  - [x] Cobalt.Memory.Native: Other general native heap allocations.

- [x] Rebase onto origin/main

- [x] Histograms for Growth and Leaks: 
  - [x] Cobalt.Memory.PrivateMemoryFootprint.GrowthRate: Track how quickly memory usage increases over a session.
  - [x] Cobalt.Memory.ObjectCounts.[Component]: Histograms for the number of specific objects (e.g., DOM Nodes, event listeners).

- [ ] rebase onto origin/main

 - [ ] Leverage Existing Tools:
  - [ ] Integrate with Cobalt Telemetry: Ensure these memory metrics are regularly reported through go/cobalt-telemetry.
  - [ ] Expose Memory Infra Data: Cobalt already includes Chromium's base/trace_event and Memory Infra. Expose aggregated data from Memory Infra snapshots to the Cobalt Telemetry system. The tooling to capture and analyze these traces locally is described in go/chrobalt/chrobalt_memory_analysis_tools.

- [ ] rebase onto origin/main

- [ ] Integrate with Cobalt Telemetry: Ensure these memory metrics are regularly reported through go/cobalt-telemetry.
- [ ] Expose Memory Infra Data: Cobalt already includes Chromium's base/trace_event and Memory Infra. Expose aggregated data from Memory Infra snapshots to the Cobalt Telemetry system. The tooling to capture and analyze these traces locally is described in go/chrobalt/chrobalt_memory_analysis_tools.

## Documentation

- [ ] Make sure that any change has the necessary README/markdown documentation added as needed. For any documentation suggested for Google3/Piper, please leave it as a suggestion in the TODO markdown file.

