### Raspi-2 image tool

This script customizes standard Raspbian Lite release image for running
Cobalt.

There are a few standard Linux tools expected to be available on the host,
and QEMU version 5.1 or newer is needed for native Raspberry emulation.

On Debian based systems, QEMU should be available via:

   `apt install qemu-system-arm`

Additionally, `dcfldd` disk image tool is required, install via:

   `apt install dcfldd`

Caution: The script requires multiple commands to be run with `sudo` under
superuser privileges. Please use caution when running and verify the commands
executed step by step, as necessary.
