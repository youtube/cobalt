Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Cobalt Multi-Process Lifecycle Coordination Internals

This document provides a comprehensive, high-fidelity systems architecture guide describing how Cobalt coordinates process lifecycle transitions (such as Startup, Conceal, Freeze, Resume, and Stop) in a multi-process, asynchronous environment.

---

## 1. The Starboard Application Lifecycle

Cobalt's multi-process lifecycle is built directly on top of the **Starboard Application Lifecycle** specification defined in [`starboard/event.h`](file:///usr/local/google/home/jfoks/cobalt.main3/src/starboard/event.h). Starboard defines a strict linear state machine for applications to manage resource consumption, background capabilities, and clean shutdowns on diverse platforms:

```mermaid
graph TD
  %% Styling Definitions
  %%{init: {"flowchart": {"htmlLabels": false}} }%%
  classDef launcher fill:#CFD8DC,stroke:#37474F,stroke-width:1px;
  classDef started fill:#C8E6C9,stroke:#388E3C,stroke-width:2px;
  classDef blurred fill:#FFF9C4,stroke:#FBC02D,stroke-width:2px;
  classDef concealed fill:#E1BEE7,stroke:#7B1FA2,stroke-width:2px;
  classDef frozen fill:#B3E5FC,stroke:#0288D1,stroke-width:2px;
  classDef stopped fill:#FFCDD2,stroke:#D32F2F,stroke-width:2px;

  %% Nodes Definitions
  Launcher[INITIAL]

  subgraph Foreground["Foreground (Visible)"]
    direction TB
    Started[STARTED <br/> Focused]
    Blurred[BLURRED <br/> Unfocused]
  end

  subgraph Background["Background (Invisible)"]
    direction TB
    Concealed[CONCEALED <br/> Running]
    Frozen[FROZEN <br/> Suspended]
  end
  Stopped[STOPPED <br/> Terminated]

  %% Apply Styles
  class Launcher launcher;
  class Started started;
  class Blurred blurred;
  class Concealed concealed;
  class Frozen frozen;
  class Stopped stopped;
  style Foreground fill:#F9F9F9,stroke:#A0A0A0,stroke-width:1px,stroke-dasharray: 5;
  style Background fill:#F9F9F9,stroke:#A0A0A0,stroke-width:1px,stroke-dasharray: 5;

  %% Transition Edges (Acyclic Double-Headed Column to force clean vertical layout)
  Launcher -->|"Start"| Started
  Launcher ---->|"Preload"| Concealed

  Started <-->|"Focus / Blur"| Blurred
  Blurred <-->|"Conceal / Reveal"| Concealed
  Concealed <-->|"Freeze / Unfreeze"| Frozen
  Frozen -->|"Stop (Shutdown)"| Stopped
```

### Starboard State Specification:
*   **`STARTED`** (`kSbEventTypeStart`): The application is in the foreground, visible, and has focus. It is expected to perform all normal operations (video playback, UI animations) and is expected to receive user keyboard and remote control input.
*   **`BLURRED`** (`kSbEventTypeBlur`): The application is still visible but has lost focus (e.g., obscured by a system dialog or on its way to shutdown). Foreground activities like rendering and animations should be paused, and the application must not receive or process keyboard/remote control input.
*   **`CONCEALED`** (`kSbEventTypeConceal` / `kSbEventTypePreload`): The application is fully invisible but can still run background tasks (audio playback, network fetching, low-priority synchronization) with reduced CPU/memory allocations.
*   **`FROZEN`** (`kSbEventTypeFreeze`): The application is invisible and periodic background work is completely stopped. It **must release all GPU, graphics, and video resources**, and flush storage writes to disk. The application can be forcefully killed by the OS in this state at any time.
*   **`STOPPED`** (`kSbEventTypeStop`): The application is cleanly terminated, freeing all remaining resources.

All Starboard-compliant platforms guarantee that applications traverse these states linearly (e.g., an application must go through `BLURRED` and `CONCEALED` to reach `FROZEN` before stopping or being killed). Cobalt's architecture is designed to enforce this exact linear progression, synthesizing intermediate states when the OS dispatches events out-of-order or skips them.

---

## 2. High-Level Architecture

Cobalt separates lifecycle coordination into a strict **three-tier boundary model** to preserve architectural layering, avoid circular dependencies, and guarantee flakeless state progression:

1.  **Pure State Sequencing** (`AppEventDelegate`): A generic, 100% side-effect-free C++ state machine that translates OS events into a linear sequence of states, synthesizing intermediate transitions when necessary to prevent skipped states.
2.  **Synchronous Thread-Blocking Execution** (`AppEventRunnerImpl`): A platform-specific orchestrator that translates state directives into physical actions (Aura window visibility, Cookies and LocalStorage flushes). It **synchronously sleeps the main UI thread** inside a whitelisted nested `base::RunLoop` until the platform actions are fully completed.
3.  **Viewport Mojo Observation** (`CobaltLifecycleManager`): A browser-side viewport state manager that binds to renderer-side Mojo pipes (`CobaltLifecycleController`) to aggregate visible layout ACKs across multiple blink frames and broadcast completion events.


### Non-Destructive Platform Window and Views Widget Preservation

During background deactivation transitions (Conceal and Freeze), Cobalt implements a **non-destructive state preservation model**:
*   **The Design**: The platform `Window` object and the Chromium `views::Widget` hierarchy **are kept fully alive in memory** in the background, while only the physical EGL graphics surfaces and hardware compositor planes are destroyed and released back to the TV operating system.
*   **System Rationale (Why we preserve the Views Layout Tree)**:
    1.  **Near-Instant Wake-up**: Re-instantiating the entire Views widget hierarchy, re-allocating Aura render tree hosts, and re-executing styling and layout passes upon resume would simply be unnecessarily slow. Keeping the tree alive allows us to simply re-bind the graphics plane and paint the pre-existing layout instantly.
    2.  **Interactive State Preservation**: Keeps user scroll positions, active remote control focus, modal overlays, and half-filled form states perfectly preserved, preventing user disorientation upon wake-up.
    3.  **Renderer-Layout Synchronization**: In a Web-runtime browser like Cobalt, the Renderer context (DOM trees, active JavaScript states, active media playback structures, and Mojo IPC pipes) must be kept alive in the background during suspension. If the Renderer's DOM tree remains alive, but we destroy the underlying Views Layout representation, the DOM elements would become completely desynchronized from their visual layout blocks. Re-aligning a fresh Views tree against an existing DOM tree is highly complex, prone to memory leaks, and breaks event-routing pipelines. Keeping both layers alive in memory guarantees absolute synchronization across the browser process, enabling flawless resume transitions.

---

## 3. Architectural Coordination Flowchart

This block chart illustrates how the Browser and Renderer processes coordinate lifecycle transitions under our wait-state encapsulation model:

```mermaid
graph TD
  %% Styling Definitions
  %%{init: {"flowchart": {"htmlLabels": false}} }%%
  classDef os fill:#FFCCBC,stroke:#FF5722,stroke-width:2px;
  classDef chromium fill:#CFD8DC,stroke:#37474F,stroke-width:1px;
  classDef delegate fill:#C5CAE9,stroke:#3F51B5,stroke-width:2px;
  classDef runner fill:#D1C4E9,stroke:#673AB7,stroke-width:2px;
  classDef manager fill:#FFE082,stroke:#FFB300,stroke-width:2px;
  classDef renderer fill:#F8BBD0,stroke:#C2185B,stroke-width:1px;

  %% Nodes Definitions
  OS[OS Thread <br/> Starboard Event Loop <br/> OS System Events]
  Delegate[Browser UI Thread <br/> AppEventDelegate <br/> Sequencer - State Machine]
  Runner[Browser UI Thread <br/> AppEventRunnerImpl <br/> Orchestrator - Blocker]

  %% Chromium Browser Components
  Shell[Browser UI Thread <br/> content::Shell <br/> Aura Platform Window]
  Storage[Browser Storage Thread <br/> ContentBrowserClient <br/> Cookies and <br/> LocalStorage]

  Manager[Browser UI Thread <br/> CobaltLifecycleManager <br/> Mojo Observer Receiver]

  %% Renderer Core & Supplements
  BlinkCore[Renderer Process <br/> Blink Core <br/> Page and Window State]
  Controller[Renderer Process <br/> CobaltLifecycleController <br/> DOM Window Supplement]

  %% Apply Styles
  class OS os;
  class Shell,Storage chromium;
  class Delegate delegate;
  class Runner runner;
  class Manager manager;
  class BlinkCore,Controller renderer;

  %% Directed Coordination Edges
  OS -->|"SbEvent"| Delegate
  Delegate -->|"DoStart <br/> DoStop <br/> DoFocus <br/> (Async)"| Runner
  Delegate -->|"DoFreeze <br/> DoConceal <br/> DoReveal <br/> DoUnfreeze <br/> DoBlur <br/> (Sync)"| Runner

  %% Wait State Branches (Storage)
  Runner -.->|"Flush <br/> Cookies/Storage <br/> (If Freeze)"| Storage
  Storage -.->|"OnCookieFlushComplete"| Runner

  %% Direct Transition Trigger (Runner -> Shell -> Renderer Blink)
  Runner -->|"Trigger Viewport Transition"| Shell

  Shell ==>|"Mojo <br/> Widget::WasHidden() / WasShown()"| BlinkCore
  Shell ==>|"Mojo <br/> FrameWidget::SetActive(active)"| BlinkCore

  %% C++ State Observation inside Blink
  BlinkCore -.->|"C++ Observers (PageVisibility <br/> DOMWindow)"| Controller

  %% Wait State Branches (Mojo Acks - Sync Blocking transitions only)
  Runner -.->|"WaitForAck"| Manager

  %% Mojo Control Flow (Browser -> Renderer)
  Manager ==>|"Mojo: <br/> CobaltLifecycleController::SetObserver(Observer)"| Controller

  %% Mojo Observation Flow (Renderer -> Browser Manager)
  Controller ==>|"Mojo <br/> (Sync ACK): <br/> OnPageVisibilityChanged <br/> OnPageResumed <br/> OnPageBlurred"| Manager
  Controller ==>|"Mojo <br/> (Async - Passive): <br/> OnPageFocused"| Manager

  %% Return to main loop after aggregation (Only for Sync Blocking transitions)
  Manager -->|"CobaltLifecycleManagerObserver"| Runner
  Runner -->|"Synchronous Return"| Delegate
```

---

## 4. Transition Sequence Diagrams

### A. Conceal Transition
This sequence diagram illustrates how the browser blocks the UI thread synchronously while waiting for Mojo visibility layout ACKs from the Renderer Process:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)
  participant Shell as content::Shell <br/> (Chromium Shell)
  participant Manager as CobaltLifecycleManager <br/> (Observer)
  participant Blink as Blink Core <br/> (Renderer)
  participant Controller as CobaltLifecycle <br/> Controller

  OS->>Delegate: SbEvent (Conceal)
  Note over Delegate: Resolve intermediate step:<br/>kBlurred ➔ kConcealed
  Delegate->>Runner: DoConceal()
  Runner->>Blink: Mojo: blink::mojom::Widget::WasHidden() <br/> (via WebContents::WasHidden)
  Runner->>Shell: content::Shell::OnConceal()
  Note over Shell: Triggers platform Aura window hides<br/>& schedules layout Conceal
  Note over Runner: Initialize visual wait state:<br/>pending_ack_ = kConceal
  Note over Runner: WaitForAck(kConceal)<br/>(Blocks UI thread via nested RunLoop)

  Note over Blink: 1. Blink updates visibilityState = hidden<br/>2. Dispatches JS event: 'visibilitychange' (hidden)
  Blink-->>Controller: C++ PageVisibilityObserver::OnPageVisibilityChanged()
  Controller->>Manager: Mojo: OnPageVisibilityChanged(hidden)
  Note over Manager: All active frames completed Conceal!
  Manager->>Runner: OnAllFramesConcealed(web_contents)
  Note over Runner: Quit nested RunLoop!<br/>run_loop.Quit()
  Note over Runner: DoConceal() returns synchronously
  Runner-->>Delegate: DoConceal complete
  Note over Delegate: Update canonical state:<br/>SetApplicationState(kConcealed)
```

### B. Freeze Transition
This sequence diagram illustrates how the same unified runner blocks the UI thread synchronously while waiting for a local Cookies and LocalStorage flush:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)
  participant Shell as content::Shell <br/> (Chromium Shell)
  participant Content as ContentBrowserClient <br/> (Storage)

  OS->>Delegate: SbEvent (Freeze)
  Note over Delegate: Resolve intermediate step:<br/>kConcealed ➔ kFrozen
  Delegate->>Runner: DoFreeze(callback)
  Runner->>Shell: content::Shell::OnFreeze()
  Note over Shell: Triggers process-level freeze<br/>& stops background tasks
  Note over Runner: Initialize storage wait state:<br/>pending_ack_ = kCookieFlush
  Runner->>Content: FlushCookiesAndLocalStorage(OnCookieFlushComplete)
  Note over Runner: UI Thread blocks synchronously!<br/>run_loop.Run()

  Note over Content: Storage thread flushes<br/>Cookies and LocalStorage to disk
  Content-->>Runner: Async disk write complete!
  Note over Runner: OnCookieFlushComplete() executes:<br/>run_loop.Quit()
  Note over Runner: DoFreeze() returns synchronously
  Runner-->>Delegate: DoFreeze complete
  Note over Delegate: Update canonical state:<br/>SetApplicationState(kFrozen)
```

### C. Unfreeze Transition
This sequence diagram illustrates how the browser blocks the UI thread synchronously while waiting for Mojo frame resume ACKs from the Renderer Process:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)
  participant Shell as content::Shell <br/> (Chromium Shell)
  participant Manager as CobaltLifecycleManager <br/> (Observer)
  participant Blink as Blink Core <br/> (Renderer)
  participant Controller as CobaltLifecycle <br/> Controller

  OS->>Delegate: SbEvent (Unfreeze)
  Note over Delegate: "Resolve intermediate step:<br/>kFrozen ➔ kConcealed"
  Delegate->>Runner: DoUnfreeze()
  Runner->>Shell: content::Shell::OnUnfreeze()
  Shell->>Blink: C++ WebContents::SetPageFrozen(false)
  Note over Shell: "Resumes background tasks<br/>& prepares graphics viewports"
  Note over Runner: "Initialize resume wait state:<br/>pending_ack_ = kUnfreeze"
  Note over Runner: "WaitForAck(kUnfreeze)<br/>(Blocks UI thread via nested RunLoop)"

  Note over Blink: "Main Blink Frame resumes<br/>and handles events in background"
  Blink-->>Controller: C++ ContextLifecycleStateObserver
  Controller->>Manager: Mojo: PageResumed()
  Note over Manager: All active frames resumed!
  Manager->>Runner: OnAllFramesResumed(web_contents)
  Note over Runner: "Quit nested RunLoop!<br/>run_loop.Quit()"
  Note over Runner: DoUnfreeze() returns synchronously
  Runner-->>Delegate: DoUnfreeze complete
  Note over Delegate: "Update canonical state:<br/>SetApplicationState(kConcealed)"
```

### D. Reveal Transition
This sequence diagram illustrates how the browser blocks the UI thread synchronously while waiting for Mojo frame visible layout ACKs, and triggers the visual `WasShown()` viewport before returning:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)
  participant Shell as content::Shell <br/> (Chromium Shell)
  participant Manager as CobaltLifecycleManager <br/> (Observer)
  participant Blink as Blink Core <br/> (Renderer)
  participant Controller as CobaltLifecycle <br/> Controller

  OS->>Delegate: SbEvent (Reveal)
  Note over Delegate: Resolve intermediate step:<br/>kConcealed ➔ kBlurred
  Delegate->>Runner: DoReveal()
  Runner->>Blink: Mojo: blink::mojom::Widget::WasShown() <br/> (via WebContents::WasShown)
  Runner->>Shell: content::Shell::OnReveal()
  Note over Shell: Triggers platform Aura window shows<br/>& schedules layout Reveal
  Note over Runner: Initialize visual wait state:<br/>pending_ack_ = kReveal
  Note over Runner: WaitForAck(kReveal)<br/>(Blocks UI thread via nested RunLoop)

  Note over Blink: Main Blink Frame layouts<br/>& renders viewport
  Blink-->>Controller: C++ PageVisibilityObserver::OnPageVisibilityChanged()
  Controller->>Manager: Mojo: OnPageVisibilityChanged(visible)
  Note over Manager: All active frames completed Reveal!
  Manager->>Runner: OnAllFramesVisible(web_contents)
  Note over Runner: Quit nested RunLoop!<br/>run_loop.Quit()
  Note over Runner: DoReveal() returns synchronously
  Runner-->>Delegate: DoReveal complete
  Note over Delegate: Update canonical state:<br/>SetApplicationState(kBlurred)
```

### E. Startup Transition
This sequence diagram illustrates how Cobalt initializes the browser process and starts the main loop on initial startup, setting up Mojo observation channels when the main frame becomes ready:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)
  participant Blink as Blink Core <br/> (Renderer)
  participant Controller as CobaltLifecycle <br/> Controller
  participant Manager as CobaltLifecycleManager <br/> (Observer)

  OS->>Delegate: SbEvent (Start)
  Note over Delegate: Initialize State:<br/>kInitial ➔ kStarted
  Delegate->>Runner: OnStart(event)
  Note over Runner: 1. Set State: running=true, visible=true, focused=true, frozen=false<br/>2. Initialize System (AtExitManager)
  Runner->>Runner: DoStart(event)
  Note over Runner: 1. Binds platform event source (Ozone)<br/>2. Runs Browser Main Loop (content::RunContentProcess)

  Note over Blink: Main Frame loads Page & DOMWindow
  Blink->>Controller: constructs CobaltLifecycleController (Blink Supplement)
  Manager->>Controller: Mojo: CobaltLifecycleController::SetObserver(Observer)
  Controller->>Manager: Mojo: CobaltLifecycleObserver::OnFrameReady()
  Note over Manager: Registers Frame in Manager
```

### F. Focus and Blur Transitions
These sequence diagrams illustrate the asynchronous, non-blocking focus and blur transitions, showing the downward Mojo activation calls and passive Mojo observation return loops:

#### 1. Focus Transition
```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)
  participant Shell as content::Shell <br/> (Chromium Shell)
  participant Blink as Blink Core <br/> (Renderer)
  participant Controller as CobaltLifecycle <br/> Controller
  participant Manager as CobaltLifecycleManager <br/> (Observer)

  OS->>Delegate: SbEvent (Focus)
  Note over Delegate: kBlurred ➔ kStarted
  Delegate->>Runner: OnFocus() -> DoFocus()
  Runner->>Shell: content::Shell::OnFocus()
  Shell->>Blink: Mojo: blink::mojom::FrameWidget::SetActive(true)
  Note over Blink: 1. Gains keyboard focus<br/>2. Dispatches JS event: 'focus'
  Blink-->>Controller: C++ FocusedFrameChanged()
  Controller->>Manager: Mojo: PageFocused()
  Note over Manager: Updates frame focus state in tracker (passive)
  Note over Runner: DoFocus() returns immediately<br/>(Asynchronous / Non-blocking)

  Runner-->>OS: DispatchFocusEvent(true) <br/> (Asynchronous Task)
  OS->>Runner: ProcessFocusEvent(true) <br/> (via PlatformEventSource)
  Note over Runner: PlatformWindowStarboard::Activate() <br/> (If waiting_for_reveal_ack_ is true, ignores!)
  Runner->>OS: OnActivationChanged(true) <br/> (via PlatformWindowDelegate)
  Note over OS: Activates Aura compositor <br/> & enables physical remote input
```

#### 2. Blur Transition
This sequence diagram illustrates how the browser blocks the UI thread synchronously while waiting for Mojo blur ACKs from the Renderer Process, in parallel with an asynchronous storage flush:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)
  participant Shell as content::Shell <br/> (Chromium Shell)
  participant Manager as CobaltLifecycleManager <br/> (Observer)
  participant Blink as Blink Core <br/> (Renderer)
  participant Controller as CobaltLifecycle <br/> Controller
  participant Content as ContentBrowserClient <br/> (Storage)

  OS->>Delegate: SbEvent (Blur)
  Note over Delegate: Resolve intermediate step:<br/>kStarted ➔ kBlurred
  Delegate->>Runner: DoBlur()
  Runner->>Shell: content::Shell::OnBlur()
  Shell->>Blink: Mojo: blink::mojom::FrameWidget::SetActive(false)

  Note over Runner: Asynchronous Cookies and <br/> LocalStorage flush triggered in parallel:
  Runner->>Content: FlushCookiesAndLocalStorage(base::DoNothing())

  Runner-->>OS: DispatchFocusEvent(false) <br/> (Asynchronous Task)
  OS->>Runner: ProcessFocusEvent(false) <br/> (via PlatformEventSource)
  Runner->>OS: OnActivationChanged(false) <br/> (via PlatformWindowDelegate)
  Note over OS: Deactivates physical remote input

  Note over Runner: Initialize visual wait state:<br/>pending_ack_ = kBlur
  Note over Runner: WaitForAck(kBlur)<br/>(Blocks UI thread via nested RunLoop)

  Note over Blink: 1. Loses focus<br/>2. Dispatches JS event: 'blur'
  Blink-->>Controller: C++ FocusedFrameChanged()
  Controller->>Manager: Mojo: PageBlurred()
  Note over Manager: All active frames completed Blur!
  Manager->>Runner: OnAllFramesBlurred(web_contents)
  Note over Runner: Quit nested RunLoop!<br/>run_loop.Quit()
  Note over Runner: DoBlur() returns synchronously
  Runner-->>Delegate: DoBlur complete
  Note over Delegate: Update canonical state:<br/>SetApplicationState(kBlurred)
```

### G. Stop Transition
This sequence diagram illustrates how the browser process cleanly shuts down all renderer contexts, WebContents, and system-level resources upon receiving the Starboard stop event:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)
  participant Shell as content::Shell <br/> (Chromium Shell)

  OS->>Delegate: SbEvent (Stop)
  Note over Delegate: kFrozen ➔ kStopped
  Delegate->>Runner: OnStop() -> DoStop()
  Runner->>Shell: content::Shell::OnStop()
  Runner->>Shell: content::Shell::Shutdown()
  Note over Shell: Destroys all WebContents & closes windows
  Note over Runner: 1. Shuts down main delegate<br/>2. Shuts down ContentMainRunner<br/>3. Releases system resources

  Delegate->>Delegate: DoTeardown()
  Note over Delegate: 1. Sets state to kStopped<br/>2. Starboard event handler terminates
```

---

## 5. Multi-Step Transition Synthesis Examples

These high-level sequence diagrams illustrate how the pure sequencer (`AppEventDelegate`) and the modular orchestrator (`AppEventRunnerImpl`) coordinate to resolve complex, multi-state transitions.

By design, these diagrams **end at the `AppEventRunnerImpl` boundary**, focusing on how the sequencer synthesizes multiple intermediate transition steps linearly, and how the runner orchestrates blocking vs. non-blocking calls.

### A. Focus Event Received While Frozen
This diagram shows how a single OS `Focus` event received while the application is completely frozen triggers three sequential, linear activating steps: **Unfreeze ➔ Reveal ➔ Focus**.

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)

  OS->>Delegate: SbEvent (Focus)
  Note over Delegate: Target State = kStarted<br/>is_activating = true
  Delegate->>Delegate: TransitionToLifeCycleState(kStarted)

  %% Step 1
  Note over Delegate: Step 1: kFrozen ➔ kConcealed (Unfreeze)
  Delegate->>Runner: OnUnfreeze() -> DoUnfreeze()
  Note over Runner: Synchronous Wait:<br/>Blocks UI thread inside nested RunLoop<br/>waiting for Mojo OnPageResumed ACK
  Runner-->>Delegate: DoUnfreeze complete
  Note over Delegate: SetApplicationState(kConcealed)

  %% Step 2
  Note over Delegate: Step 2: kConcealed ➔ kBlurred (Reveal)
  Delegate->>Runner: OnReveal() -> DoReveal()
  Note over Runner: Synchronous Wait:<br/>Blocks UI thread inside nested RunLoop<br/>waiting for Mojo OnPageVisibilityChanged(visible) ACK
  Runner-->>Delegate: DoReveal complete
  Note over Delegate: SetApplicationState(kBlurred)

  %% Step 3
  Note over Delegate: Step 3: kBlurred ➔ kStarted (Focus)
  Delegate->>Runner: OnFocus() -> DoFocus()
  Note over Runner: Asynchronous/Immediate:<br/>Dispatches focus event to platform/Shell<br/>without blocking UI thread
  Runner-->>Delegate: DoFocus complete
  Note over Delegate: SetApplicationState(kStarted)

  Note over Delegate: Transition Complete!<br/>Reached target state kStarted
```

### B. Freeze Event Received While Started
This diagram shows how a single OS `Freeze` event received while the application is running with active focus triggers three sequential, linear deactivating steps: **Blur ➔ Conceal ➔ Freeze**.

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)

  OS->>Delegate: SbEvent (Freeze)
  Note over Delegate: Target State = kFrozen<br/>is_activating = false
  Delegate->>Delegate: TransitionToLifeCycleState(kFrozen)

  %% Step 1
  Note over Delegate: Step 1: kStarted ➔ kBlurred (Blur)
  Delegate->>Runner: OnBlur() -> DoBlur()
  Note over Runner: Synchronous Wait:<br/>Blocks UI thread inside nested RunLoop<br/>waiting for Mojo OnPageBlurred ACK
  Runner-->>Delegate: DoBlur complete
  Note over Delegate: SetApplicationState(kBlurred)

  %% Step 2
  Note over Delegate: Step 2: kBlurred ➔ kConcealed (Conceal)
  Delegate->>Runner: OnConceal() -> DoConceal()
  Note over Runner: Synchronous Wait:<br/>Blocks UI thread inside nested RunLoop<br/>waiting for Mojo OnPageVisibilityChanged(hidden) ACK
  Runner-->>Delegate: DoConceal complete
  Note over Delegate: SetApplicationState(kConcealed)

  %% Step 3
  Note over Delegate: Step 3: kConcealed ➔ kFrozen (Freeze)
  Delegate->>Runner: OnFreeze() -> DoFreeze()
  Note over Runner: Synchronous Wait:<br/>Blocks UI thread inside nested RunLoop<br/>waiting for Cookies and LocalStorage flush complete ACK
  Runner-->>Delegate: DoFreeze complete
  Note over Delegate: SetApplicationState(kFrozen)

  Note over Delegate: Transition Complete!<br/>Reached target state kFrozen
```

---

## 6. Re-Entrant Event Handling and Target Redirects

Because deactivation transitions (such as Blur, Conceal, and Freeze) synchronously block the UI Main Thread in a nested `base::RunLoop`, it is highly common for new system events to arrive re-entrantly *before* the current active transition step fully completes.

### Concurrency Safety Architecture:
*   **Lock Release During Waiting**: The `AppEventDelegate` locks its state machine mutex (`lock_`) *only* when updating state or calculating steps. During the actual transition execution (`DoBlur`, `DoConceal`), the mutex **is fully unlocked**.
*   **Re-Entrant Thread Safety**: When a new OS event arrives during a blocking wait, `HandleEvent` is executed re-entrantly on the same thread. Since the mutex is unlocked, it successfully acquires `lock_` without deadlocking!
*   **Dynamic Target Redirection**: If a transition is already active (`is_transitioning_ == true`), the sequencer simply **updates `target_state_`** to the new event's target and returns immediately without spawning a new loop.
*   **Linear Resolution**: Once the active blocking step receives its Mojo ACK and exits the nested loop, the sequencer updates the completed state, compares it with the *newly redirected target*, and seamlessly executes the next step in the new target's direction.

### A. Deactivation Redirect: Conceal Received During Active Blur
This diagram illustrates how an incoming `Conceal` event received during an active, blocking `Blur` transition dynamically redirects the target state, resulting in the seamless execution of `Conceal` immediately after `Blur` completes:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)

  Note over Delegate: Application is kStarted
  OS->>Delegate: SbEvent (Blur)
  Note over Delegate: target_state_ = kBlurred<br/>is_transitioning_ = true
  Delegate->>Runner: OnBlur() -> DoBlur()
  Note over Runner: UI Thread blocks synchronously in nested loop<br/>waiting for Mojo OnPageBlurred ACK

  %% Re-entrant event
  OS->>Delegate: SbEvent (Conceal)
  Note over Delegate: Re-entrant call! lock_ is UNLOCKED during WaitForAck.<br/>HandleEvent locks successfully.
  Note over Delegate: Transition active. Redirect target:<br/>target_state_ = kConcealed
  Delegate-->>OS: HandleEvent returns immediately

  Note over Runner: Mojo OnPageBlurred ACK arrives
  Note over Runner: Quit nested RunLoop!
  Runner-->>Delegate: DoBlur complete
  Note over Delegate: 1. Acquire lock_<br/>2. SetApplicationState(kBlurred)
  Delegate->>Delegate: ExecuteNextStepLocked()
  Note over Delegate: application_state_ (kBlurred) != target_state_ (kConcealed)
  Note over Delegate: Step 2: kBlurred ➔ kConcealed (Conceal)
  Delegate->>Runner: OnConceal() -> DoConceal()
  Note over Runner: UI Thread blocks synchronously...
```

### B. Reversal Redirect: Conceal Received During Active Reveal
This diagram illustrates a direction reversal: an incoming `Conceal` event is received during an active `Reveal` (activating) transition. The sequencer completes the active step first (to prevent broken rendering states), then immediately detects the reversal and executes `Conceal` to return the application to `kConcealed`:

```mermaid
sequenceDiagram
  autonumber
  participant OS as Starboard <br/> Event Loop
  participant Delegate as AppEventDelegate <br/> (Sequencer)
  participant Runner as AppEventRunnerImpl <br/> (Orchestrator)

  Note over Delegate: Application is kConcealed
  OS->>Delegate: SbEvent (Reveal)
  Note over Delegate: target_state_ = kBlurred<br/>is_transitioning_ = true
  Delegate->>Runner: OnReveal() -> DoReveal()
  Note over Runner: UI Thread blocks synchronously in nested loop<br/>waiting for Mojo OnPageVisibilityChanged(visible) ACK

  %% Re-entrant event
  OS->>Delegate: SbEvent (Conceal)
  Note over Delegate: Re-entrant call! lock_ is UNLOCKED during WaitForAck.<br/>HandleEvent locks successfully.
  Note over Delegate: Transition active. Redirect/Reverse target:<br/>target_state_ = kConcealed
  Delegate-->>OS: HandleEvent returns immediately

  Note over Runner: Mojo OnPageVisibilityChanged(visible) ACK arrives
  Note over Runner: Quit nested RunLoop!
  Runner-->>Delegate: DoReveal complete
  Note over Delegate: 1. Acquire lock_<br/>2. SetApplicationState(kBlurred)
  Delegate->>Delegate: ExecuteNextStepLocked()
  Note over Delegate: application_state_ (kBlurred) != target_state_ (kConcealed)
  Note over Delegate: Sequencer detects reversal (is_activating = false)
  Note over Delegate: Step 2: kBlurred ➔ kConcealed (Conceal)
  Delegate->>Runner: OnConceal() -> DoConceal()
  Note over Runner: UI Thread blocks synchronously...
```

---

## 7. Detailed Coordination Steps

1.  **OS Event Dispatch**: The Starboard OS event loop dispatches a system event (e.g., `Freeze`) to `AppEventDelegate`.
2.  **Transition Sequencing**: `AppEventDelegate` locks the state machine and resolves the immediate next intermediate step (e.g., `kConcealed -> kFrozen`) via `GetNextState()`.
3.  **Synchronous Trigger**: The delegate calls `AppEventRunnerImpl`'s corresponding transition wrapper synchronously (e.g., `DoFreeze(callback)`).
4.  **Wait-State Injection**: The runner caches any test mock callback, sets the active wait type (`pending_ack_ = kCookieFlush`), and calls its unified blocking helper `WaitForAck()`.
5.  **UI Main Thread Sleep**: Inside `WaitForAck()`, the runner authorizes synchronous waits, triggers the transition work (either registering Mojo layout ACKs in `CobaltLifecycleManager` OR launching local Cookies and LocalStorage flushes in `ContentBrowserClient`), and **synchronously sleeps the UI thread inside a nested `base::RunLoop`**.
6.  **Mojo/Storage Completion**:
    *   *Mojo Viewports*: As the Blink frame renders, it sends visibility changes over Mojo (`CobaltLifecycleObserver`). `CobaltLifecycleManager` aggregates these frame signals and broadcasts completion (e.g., `OnAllFramesConcealed`) back to the runner.
    *   *Disk Storage*: Once the Cookies and LocalStorage storage thread finishes writing files to disk, the storage client executes the runner's local callback `OnCookieFlushComplete()`.
7.  **Nested Loop Quit**: The runner's callback handler receives the completion signal and calls `run_loop.Quit()`, waking up the sleeping UI thread.
8.  **State Finalization**: The nested loop exits, `WaitForAck()` returns, the runner transition wrapper returns synchronously to `AppEventDelegate`, and the delegate immediately updates its canonical state (`SetApplicationState(kFrozen)`), safely triggering the next sequential step.

---

## 8. Starboard Thread Restrictions Whitelisting

To prevent UI janks and deadlocks, Chromium strictly disallows any synchronous blocking wait primitives on the **Browser UI Main Thread** by default:

*   **The Starboard Pump Constraint**: On Starboard-based platforms, Cobalt implements **`base::MessagePumpUIStarboard`** which blocks using **`base::WaitableEvent`** under the hood when running nested `base::RunLoop` blocks. This immediately triggers Chromium's thread-restriction assertions (`DCHECK`) and aborts the process!
*   **The Resolution**: We utilize **`base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope`** inside `WaitForAck()` to authorize the thread to block during critical, unavoidable OS deactivation events.
*   **Layering Whitelist**: To prevent arbitrary developer abuse, Chromium whitelists instantiation of this Scoped helper using a strict **`friend class`** list in **`base/threading/thread_restrictions.h`**. Since `AppEventRunnerImpl` is our high-level modular shell orchestrator, we whitelist it inside the base library and guard the addition cleanly using **`#if BUILDFLAG(IS_COBALT)`**:

```cpp
#if BUILDFLAG(IS_COBALT)
  // Cobalt's platform deactivation lifecycle transitions (Reveal, Conceal,
  // Freeze, Unfreeze, Blur) synchronously block the UI Main Thread using a nested
  // base::RunLoop to guarantee atomic, linear state progression. Since
  // Starboard's custom base::MessagePumpUIStarboard blocks using
  // base::WaitableEvent under the hood, we must explicitly whitelist the modular
  // platform shell runner here to authorize main-thread sync waiting.
  friend class cobalt::AppEventRunnerImpl;
#endif  // BUILDFLAG(IS_COBALT)
```
