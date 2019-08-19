## Trampoline Readme

**Trampoline**:
  An operation which will forward control to a delegate.

*Maps f(device_id) -> g(platform, config, device_id)*

Trampolines reduce the complexity for invoking cobalt/starboard binaries
and tests by reading parameters from the `metadata.json` file of a Cobalt
Archive. The delegate is then run with the full command-line argument
set including platform and configuration data. The calling signature is
therefore *stable* and any Cobalt archive can be run the same binaries/tests
using the same shell command.

Example:
    python __cobalt_archive/run/run_cobalt.py --device_id IP

  Could translate into:
    python starboard/tools/example/app_launcher_client.py
    -t cobalt --platform linux --config devel --device_id IP
