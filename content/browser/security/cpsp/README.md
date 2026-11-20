# ChildProcessSecurityPolicy

This directory contains Chromium's reference monitor for enforcing security
policies, including Site Isolation, on renderer and other child processes. It is
designed as a singleton in the browser process which grants permissions and
access to origin data. These grants can be global for all Chromium processes or
specific to a child process.

All sensitive operations performed on behalf of a child process must first
consult ChildProcessSecurityPolicy to determine if the operation should be
allowed. This includes access to origin data, local files, etc. Failed requests
should usually result in a renderer kill, because they indicate a misbehaving or
possibly compromised renderer process.

SecurityState tracks process-specific grants, ensuring that the state is kept
alive as long as any requests may need it. This includes a period of time after
the child process has exited, while IPCs or thread hops may be in flight. No new
grants are allowed after the child process has exited, though.

ChildProcessSecurityPolicy is generally designed to be consulted from any thread
in the browser process, using locks. However, some of its functions are only
safe to call from specific threads (e.g., the UI thread) because of the
parameters or state they need to access, as documented in the interface.

## Migration to Rust

ChildProcessSecurityPolicy is being incrementally migrated from a C++
implementation to a Rust implementation in https://crbug.com/482216433.

This effort has multiple goals:
* Get experience with shipping first-party Rust code within Chromium's browser
  process, interacting with existing C++ code. This may clear a path for
  additional uses of Rust within Chromium.
* Improve safety invariants for a key security component in Chromium. Rust can
  more easily rule out classes of logic bugs at compile time. This is important
  for many invariants within ChildProcessSecurityPolicy (including how Site
  Isolation is enforced), as well as for reducing current stability problems in
  this code.
* Avoid any future memory safety concerns within ChildProcessSecurityPolicy,
  even if there are not currently a large number of memory safety issues known
  in this code.

ChildProcessSecurityPolicy is practical to migrate to Rust for multiple reasons:
* It has a clear API boundary with predominantly simple types. Most of its APIs
  take in IDs, strings, Origins, and FilePaths, and they return bools. It is
  primarily a set of data structures that can be populated and queried without
  complex pointer relationships.
* When Chromium C++ objects need to be passed to the Rust data structures, they
  can be owned by Rust. Non-owning pointers (which could pose safety risks) are
  generally not needed.
* It is a singleton, simplifying pointer ownership concerns.
* Most of the calls from Rust back into C++ in this code appear to be trivial
  and can likely be analyzed to be safe.

The migration is guarded behind the `kChildProcessSecurityPolicyRust` feature
flag, which has multiple FeatureParams.
* `kCppOnly`: Only the C++ implementation of ChildProcessSecurityPolicy is used.
   This is the default if the feature is disabled, or via
   `--disable-features=ChildProcessSecurityPolicyRust`
* `kRustOnly`: Only the Rust implementation of ChildProcessSecurityPolicy is
   used. This is the default if the feature is enabled, or via
   `--enable-features=ChildProcessSecurityPolicyRust`
* `kRustAndCpp`: Both the C++ and Rust implementations of
   ChildProcessSecurityPolicy are used, and CHECK failures occur if the results
   differ. This mode can be enabled via
   `--enable-features=ChildProcessSecurityPolicyRust:policy/rust-and-cpp`
All modes can be set via `chrome://flags/#child-process-security-policy-rust`.
