# Abstract Toolchain

## Motivation

The aim of implementing an Abstract Toolchain is to allow porters to add
new toolchains or customize existing ones without the need of modifying
common code.

Initially all targets were defined in one common shared file,
`src/tools/gyp/pylib/gyp/generator/ninja.py`.
Modifications to this file were required for replacing any of the toolchain
components, adding platform-specific tooling, adding new toolchains, or
accommodating platform-specific flavor of reference tool. Doing this in a
shared file does not scale with the number of ports.

## Overview

The solution implemented to solve toolchain abstraction consists of adding two
new functions to the platform specific `gyp_configuration.py` file found under:

`starboard/<PLATFORM>/gyp_configuration.py`

The functions to implement are:

`GetHostToolchain` and `GetTargetToolchain`

## Example

The simplest complete GCC based toolchain, where a target and host are the same,
and all tools are in the PATH:

  class ExamplePlatformConfig(starboard.PlatformConfig)
    # ...

    def GetTargetToolchain(self):
      return [
        starboard.toolchains.gnu.CCompiler(),
        starboard.toolchains.gnu.CPlusPlusCompiler(),
        starboard.toolchains.gnu.Assembler(),
        starboard.toolchains.gnu.StaticLinker(),
        starboard.toolchains.gnu.DynamicLinker(),
        starboard.toolchains.gnu.Bison(),
        starboard.toolchains.gnu.Stamp(),
        starboard.toolchains.gnu.Copy(),
      ]

    def GetHostToolchain(self):
      return self.GetTargetToolchain()

You can find real examples of this in the Open Source repo:
[Linux x8611](https://cobalt.googlesource.com/cobalt/+/refs/heads/21.lts.1+/src/starboard/linux/x64x11/gyp_configuration.py)

[Raspberry Pi 2](https://cobalt.googlesource.com/cobalt/+/refs/heads/21.lts.1+/src/starboard/raspi/shared/gyp_configuration.py)
