# Mojo Proxy

This directory hosts the ChromeOS Mojo Proxy, a standalone service which
bridges processes running Mojo over ipcz with ChromeOS processes (e.g. ARCVM)
still running the legacy (pre-ipcz) Mojo Core implementation.

## Layout

* `service/`: the `mojo_proxy` executable and its integration tests. See
  `service/README.md` for usage. The executable is shipped on ChromeOS as a
  data dependency of the `chrome` binary and launched via fork+exec by
  `MojoInvitationManager`.

* `mojo_core/`: a frozen, self-contained copy of the legacy (pre-ipcz) Mojo
  Core implementation, namespaced as `mojo_legacy`. It must not depend on
  `//mojo`, so that the legacy implementation can eventually be removed from
  `//mojo` without affecting the proxy.
