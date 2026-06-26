Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Getting started

Cobalt is a lightweight HTML5/CSS/JS application container designed to provide
a rich application development environment with minimal resource consumption
(deployment size, RAM, CPU, and GPU). It enables a rich, low-latency user
experience across a wide variety of platforms and devices.

#### Target audiences

Cobalt's documentation is written with two audiences in mind:

*   **Porters** enable Cobalt to run on new platforms by using Starboard,
    Cobalt's porting layer and OS abstraction. Each Starboard module
    (such as memory, socket, and thread) defines functions that porters must
    implement.

*   **Developers** want to build applications in familiar environments with
    advanced debugging tools without having to worry about compatibility with
    a highly fragmented set of browsers. At the same time, they want to have
    full control over their codebase so that they can ship features for
    constrained platforms, like TVs, on time and without technical risk.

## Benefits of Cobalt

Cobalt significantly reduces the cost of supporting a browser on non-standard,
resource-constrained platforms. Because Cobalt operates at a consolidated,
versioned platform abstraction layer, porting takes only a few weeks, and subsequent
rebases are straightforward.

These are some other benefits that Cobalt provides:

*   **More platforms**

    *   Cobalt does not require platforms to support JIT compilation and can
        run on platforms that disallow execution of dynamically generated code.
    *   Cobalt is a single-process application and does not rely on the ability
        to spawn multiple processes.
    *   Cobalt precompiles a set of shaders that are sufficient to express all
        graphical effects, thereby accommodating platforms that cannot compile
        shaders at runtime.
    *   Cobalt requires a compliant C++17 compiler.

*   **Small footprint**
    *   Cobalt is optimized for memory. Its surface cache never exceeds a
        predefined budget, and it never creates duplicate layers, reducing
        the likelihood of out-of-memory crashes.
    *   Cobalt's small binary footprint minimizes storage requirements. By
        supporting a subset of HTML5/CSS/JS, Cobalt reduces package size, even
        allowing CJK fonts to be bundled on low-end devices.

*   **Reduced input latency**

    *   Cobalt produces consistent 60FPS animations by only supporting
        animation of properties that don't affect layout, like `transform`,
        and always running animations on a separate thread.
    *   Cobalt is optimized to run on single-core CPUs, resulting in better
        input latency since the renderer and resource loader do not compete
        with layout operations.
    *   On platforms that support GLES2, Cobalt avoids CPU painting by
        performing almost all rendering operations on the GPU.

## Getting started

### Porters

Porters should begin with the [porting guide](starboard/porting.md),
which explains how to use Starboard, Cobalt's porting layer, to customize the
platform-specific functionality that Cobalt uses. There are several reference
documents to help porters customize configuration files and to implement
module-specific functionality. The [Testing with
NPLB](starboard/testing.md) document provides an overview of
Starboard's compliance test suite.

### Developers

Developers can use the setup instructions for
[Linux](development/setup-linux.md),
[RDK](development/setup-rdk.md), or
[Android](development/setup-android.md) to set up their Cobalt development
environment, clone the repository, and build a binary. The
[Cobalt support](development/reference/supported-features.md) guide lists
supported HTML elements, CSS properties, CSS selectors, and JavaScript Web
APIs.
