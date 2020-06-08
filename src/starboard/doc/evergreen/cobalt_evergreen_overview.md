# Cobalt Evergreen Overview

![Cobalt non-Evergreen vs
Evergreen](resources/cobalt_evergreen_overview_flow.png)

## What is Cobalt Evergreen?

Cobalt Evergreen is an end-to-end framework for cloud-based deployment of Cobalt
updates without the need for supplemental Cobalt integration work on device
platforms.

For a bit of background context, as the number of Cobalt devices in the field
increases there is a growing proliferation of version fragmentation. Many of
these devices are unable to take advantage of the benefits of Cobalt
performance, security, and functional improvements as it is costly to integrate
and port new versions of Cobalt. We recognized this issue, listened to feedback
from the Cobalt community and as a result developed Cobalt Evergreen as a way to
make updating Cobalt a much simpler process for everyone involved.

This relies on separating the Starboard(platform) and Cobalt(core) components of
a Cobalt implementation into the following discrete components:

**Google-built** (on Google toolchain)

*   Cobalt Core
    *   Pre-built shared library available for all supported architectures
*   Cobalt Updater
    *   Part of Cobalt Core and used to query servers to check for and download
        updated Cobalt Core

**Partner-built** (on Partner toolchain)

*   Starboard
    *   Platform-specific implementation
    *   Contains system dependencies (e.g. `libpthread.so`, `libEGL.so`)
*   Cobalt Loader (Loader App)
    *   Selects the appropriate Cobalt core for usage
    *   An ELF loader is used to load the Cobalt core and resolves symbols with
        Starboard APIs when Cobalt starts up in Evergreen mode

With this new Cobalt platform architecture, less engineering effort is necessary
for a full Cobalt integration/deployment. The idea here is you should only need
to implement Starboard one time and any Cobalt-level updates should only require
platform testing. NOTE that certain new Cobalt features may require Starboard
changes, so if you want to take advantage of some of these new features,
Starboard changes may be necessary.

### Main Benefits

*   More stable platform as there is less Cobalt version fragmentation
*   Little-to-no engineering effort necessary for Cobalt updates
*   Longer device lifetime due to more Cobalt updates
*   Less engineering work/accelerated timeline for Cobalt integration/deployment
    as Google builds the Cobalt components and partners are only responsible for
    the Starboard and `loader_app` portion

### New in Evergreen

*   Larger storage and system permissions requirements in order to update and
    store multiple Cobalt binaries
*   Access permissions to download binaries onto a device platform from Google
    servers
*   New `loader_app` component required to be built on platform toolchains
*   Additional testing/verification required to ensure new Cobalt releases work
    properly

## How is Evergreen different from porting Cobalt previously?

There are minimal differences in switching to Evergreen as the Cobalt team has
already done a majority of the work building the necessary components to support
the Evergreen architecture. You will still be responsible for building the
Starboard and platform-specific components as usual. Thereafter, switching to
Evergreen is as simple as building a different configuration. Please see the
Raspberry Pi 2 Evergreen reference port
([Instructions](cobalt_evergreen_reference_port_raspi2.md)) for an example.

![Cobalt non-Evergreen vs
Evergreen](resources/cobalt_evergreen_overview_vs_non_evergreen.png)

### Building Cobalt Evergreen Components

Cobalt Evergreen requires that there are two separate build(`gyp`)
configurations used due to the separation of the Cobalt core(`libcobalt.so`) and
the platform-specific Starboard layer(`loader_app`). As a result, you will have
to initiate a separate gyp process for each. This is required since the Cobalt
core binary is built with the Google toolchain settings and the
platform-specific Starboard layer is built with partner toolchain
configurations.

Cobalt Evergreen is built by a separate gyp platform using the Google toolchain:

```
$ cobalt/build/gyp_cobalt evergreen-arm-softfp-sbversion-12
$ ninja -C out/evergreen-arm-softfp-sbversion-12_qa cobalt
```

Which produces a shared library `libcobalt.so` targeted for specific
architecture, ABI and Starboard version.

The gyp variable `sb_evergreen` is set to 1 when building `libcobalt.so`.

The partner port of Starboard is built with the partner’s toolchain and is
linked into the **`loader_app` which knows how to dynamically load
`libcobalt.so`.

```
cobalt/build/gyp_cobalt <partner_port_name>
ninja -C out/<partner_port_name>_qa loader_app
```

Partners should set `sb_evergreen_compatible` to 1 in their gyp platform config.
DO NOT set the `sb_evergreen` to 1 in your platform-specific configuration as it
is used only by Cobalt when building with the Google toolchain.

The following additional Starboard interfaces are necessary to implement for
Evergreen:

*   `kSbSystemPathStorageDirectory`
    *   Dedidated location for storing Cobalt Evergreen-related binaries
    *   This path must be writable and have at least 96MB of reserved space for
        Evergreen updates. Please see the “Platforms Requirements” section below
        for more details.
*   `kSbMemoryMapProtectExec`
    *   Ensures mapped memory can be executed
*   `#define SB_CAN_MAP_EXECUTABLE_MEMORY 1`
    *   Specifies that the platform can map executable memory
    *   Defined in `configuration_public.h`

Only if necessary, create a customized SABI configuration for your architecture.
Note, we do not anticipate that you will need to make a new configuration for
your platform unless it is not one of our supported architectures:

*   x86\_32
*   x86\_64
*   arm32
*   arm64

If your target architecture falls outside the support list above, please reach
out to us for guidance.

### What is an example for how this would help me?

Some common versions of Cobalt in the field may show a bug in the implementation
of the CSS which can cause layout behavior to cause components to overlap and
give users a poor user experience. A fix for this is identified and pushed to
Cobalt open source ready for integration and deployment on devices.

#### Without Cobalt Evergreen:

Though a fix for this was made available in the latest Cobalt open source,
affected devices in the field are not getting updated (e.g. due to engineering
resources, timing, device end-of-life), users continue to have a poor experience
and have negative sentiment against a device. In parallel, the web app team
determines a workaround for this particular situation, but the workaround is
obtuse and causes app bloat and technical debt from on-going maintenance of
workarounds for various Cobalt versions.

#### With Cobalt Evergreen:

The Cobalt team can work with you to guide validation and deployment of a shared
Cobalt library to all affected devices much more quickly without all the
engineering effort required to deploy a new Cobalt build. With this simpler
updating capability, device behavior will be more consistent and there is less
technical debt from workarounds on the web application side. Additionally, users
can benefit from the latest performance, security, and functional fixes.

## Platform Requirements

Cobalt Evergreen currently supports the following

Target Architectures:

*   x86\_32
*   x86\_64
*   armv7 32
*   armv8 64

Supported Javascript Engines

*   V8

Additional reserved storage (96MB) is required for Evergreen binaries. We expect
Evergreen implementations to have an initial Cobalt preloaded on the device and
an additional reserved space for additional Cobalt update storage.

*   Initial Cobalt binary deployment - 64MB
*   Additional Cobalt update storage - 96MB
    *   Required for 2 update slots under `kSbSystemPathStorageDirectory`

As Cobalt Evergreen is intended to be updated from Google Cloud architecture
without the need for device FW updates, it is important that this can be done
easily and securely on the target platform. There are a set of general minimum
requirements to do so:

*   Writable access to the file system of a device platform to download Cobalt
    Evergreen binaries
*   Enough reserved storage for Cobalt updates
*   Platform supporting mmap API with writable memory (`PROT_WRITE`,
    `PROT_EXEC`) for loading in-memory and performing relocations for Cobalt
    Evergreen binaries

## Verifying Platform Requirements

In order to verify the platform requirements you should run the
‘nplb\_evergreen\_compat\_tests’. These tests ensure that the platform is
configured appropriately for Evergreen.

To enable the test, set the `sb_evergreen_compatible gyp` variable to 1 in the
`gyp_configuration.gypi`. For more details please take a look at the Raspberry
Pi 2 gyp files.

There is a reference implementation available for Raspberry Pi 2 with
instructions available [here](cobalt_evergreen_reference_port_raspi2.md).

## System Design

![Cobalt Evergreen
Components](resources/cobalt_evergreen_overview_components.png)

The above diagram is a high-level overview of the components in the Cobalt
Evergreen architecture.

* **Partner-built** represents components the Partner is responsible for
  implementing and building.

* **Cobalt-built** represents components the Cobalt team is responsible for
  implementing and building.

### Cobalt Evergreen Components

#### Cobalt Updater

This is a new component in the Cobalt Shared Library component that is built on
top of the Starboard API. The purpose of this module is to check the update
servers if there is a new version of the Cobalt Shared Library available for the
target device. If a new version is available, the Cobalt updater will download,
verify, and install the new package on the target platform. The new package can
be used the next time Cobalt is started or it can be forced to update
immediately and restart Cobalt as soon as the new package is available and
verified on the platform. This behavior will take into account the
suspend/resume logic on the target platform.

Functionally, the Cobalt Updater itself runs as a separate thread within the
Cobalt process when Cobalt is running. This behavior depends on what the target
platform allows.

For more detailed information on Cobalt Updater, please take a look
[here](cobalt_update_framework.md).

#### Google Update (Update Server)

We use Google Update as the infrastructure to manage the Cobalt Evergreen
package install and update process. This has been heavily used across Google for
quite some time and has the level of reliability required for Cobalt Evergreen.
There are also other important features such as:

*   Fine grained device targeting
*   Rollout and rollback
*   Multiple update channels (e.g. production, testing, development)
*   Staged rollouts

For more detailed information on Google Update for Cobalt Evergreen, please take
a look [here](cobalt_update_framework.md).

#### Google Downloads (Download Server)

We use Google Downloads to manage the downloads available for Cobalt Evergreen.
The Cobalt Updater will use Google Downloads in order to download available
packages onto the target platform. We selected Google Downloads for this purpose
due to its ability to scale across billions of devices as well as the
flexibility to control download behavior for reliability.

For more detailed information on Google Downloads (Download Server) for Cobalt
Evergreen, please take a look [here](cobalt_update_framework.md).

### Cobalt Evergreen Interfaces

#### Starboard ABI

The Starboard ABI was introduced to provide a single, consistent method for
specifying the Starboard API version and the ABI. This specification ensures
that any two binaries, built with the same Starboard ABI and with arbitrary
toolchains, are compatible.

Note that Cobalt already provides default SABI files for the following
architectures:

*   x86\_32
*   x86\_64
*   arm v7 (32-bit)
*   arm v8 (64-bit)

You should not need to make a new SABI file for your target platform unless it
is not a currently supported architecture. We recommend that you do not make any
SABI file changes. If you believe it is necessary to create a new SABI file for
your target platform, please reach out to the Cobalt team to advise.

For more detailed information on the Starboard ABI for Cobalt Evergreen, please
take a look here.

### Installation Slots

Cobalt Evergreen provides support for maintaining multiple separate versions of
the Cobalt binary on a platform. These versions are stored in installation
slots(i.e. known locations on disk), and are used to significantly improve the
resilience and reliability of Cobalt updates.

The number of installation slots available will be determined by the platform
owner. A minimum of 2 must be available with the limit being the storage
available on the platform.

It is worth noting that adding a third installation slot allows a factory image
to be permanently installed as a fail-safe option.

The number of installation slots is directly controlled using
`kMaxNumInstallations`, defined in
[loader\_app.cc](https://cobalt.googlesource.com/cobalt/+/refs/heads/master/src/starboard/loader_app/loader_app.cc).

There are subtle differences between using 2 installation slots and using 3 or
more installation slots outlined below.

#### 2 Slot Configuration

Both of the installation slots are located in the directory specified by
`kSbSystemPathStorageDirectory`. This will vary depending on the platform.

On the Raspberry Pi this value will be `/home/pi/.cobalt_storage`. The paths to
the installation slots will be as follows:

`/home/pi/.cobalt_storage/installation_0` (initial installation slot)

```
/home/pi/.cobalt_storage/installation_1
```

Where the most recent update is stored will alternate between the two available
slots. No factory image is maintained with this configuration so only the two
most recent updates will be stored on the machine.

Finally, the installation slot that will be used when Cobalt Evergreen is run
would be stored in

`/home/pi/.cobalt_storage/installation_store.pb` on the Raspberry Pi.

#### 3+ Slot Configuration

The 3+ slot configuration is very similar to the 2 slot configuration, but has
one major difference: the original installation is maintained, and is stored in
a read-only installation slot within the content directory.

On the Raspberry Pi, the installation slots will be as follows:

`/home/pi/path-to-cobalt/content/app/cobalt` (initial installation slot)
(read-only)

```
/home/pi/.cobalt_storage/installation_1
/home/pi/.cobalt_storage/installation_2
```

Similar to the 2 slot configuration, where the most recent update is stored will
alternate between the two available slots (not the read-only installation slot).

The installation slot that will be used when Cobalt Evergreen is run is
determined, and stored, exactly the same as it is for the 2 slot configuration.

### Fonts
The system font directory `kSbSystemPathStorageDirectory` should be configured to
point to the `standard` (23MB) or the `limited` (3.1MB) cobalt font packages. An
easy way to do that is to use the `loader_app/content` directory and setting the
`cobalt_font_package` to `standard` or `limited` in your port.

Cobalt Evergreen, built by Google, will by default use the `minimal` cobalt
package which is around 16KB to minimize storage requirements. A separate
`cobalt_font_package` variable is set to `minimal` in the Evergreen platform.

On Raspberry Pi this is:

`minimal` set of fonts under:
```
~/.cobalt_storage/installation_0/content/fonts/
```

`standard` or `limited` set of fonts under:
```
loader_app/content/fonts
```

### ICU Tables
The ICU table should be deployed under the kSbSystemPathStorageDirectory.
This way all Cobalt Evergreen installations would be able to share the same
tables. The current storage size for the ICU tables is 7MB.

On Raspberry Pi this is:

```
/home/pi/.cobalt_storage/icu
```
The Cobalt Evergreen package will not carry ICU tables by default but may add
them in the future if needed. When the package has ICU tables they would be
stored under the content location for the installation.

### Platform Security

As Cobalt binary packages ([CRX
format](https://docs.google.com/document/d/1pAVB4y5EBhqufLshWMcvbQ5velk0yMGl5ynqiorTCG4/edit#heading=h.ke61kmpkapku))
are downloaded from the Google Downloads server, the verification of the Cobalt
update package is critical to the reliability of Cobalt Evergreen. There are
mechanisms in place to ensure that the binary is verified and a chain of trust
is formed. The Cobalt Updater is responsible for downloading the available
Cobalt update package and verifies that the package is authored by Cobalt(and
not an imposter), before trying to install the downloaded package.

#### Understanding Verification

![Cobalt Evergreen CRX
Verification](resources/cobalt_evergreen_overview_crx_verification.png)

In the above diagram, the Cobalt Updater downloads the update package if
available, and parses the CRX header of the package for verification, before
unpacking the whole package. A copy of the Cobalt public key is contained in the
CRX header, so the updater retrieves the key and generates the hash of the key
coming from the header of the package, say _Key_ _hash1_.

At the same time, the updater has the hash of the Cobalt public key hard-coded
locally, say _Key hash2_.

The updater compares _Key hash1_ with _Key hash2._ If they match, verification
succeeds.

## **FAQ**

### Can I host the binaries for Cobalt core myself to deploy on my devices?

Not at this time. All Cobalt updates will be deployed through Google
infrastructure. We believe Google hosting the Cobalt core binaries allows us to
ensure a high-level of reliability and monitoring in case issues arise.

### What is the performance impact of switching to Cobalt Evergreen?

We expect performance to be similar to a standard non-Evergreen Cobalt port.

### How can I ensure that Cobalt updates work well on our platform?

Google will work closely with device partners to ensure that the appropriate
testing is in place to prevent regressions.

### Will there be tests provided to verify functionality and detect regression?

Yes, there are tests available to help validate the implementation:

*   NPLB tests to ensure all necessary APIs are implemented
*   Cobalt Evergreen Test Plan to verify the functionality of all components and
    use cases

### How can I be sure that Cobalt space requirements will not grow too large for my system resources?

The Cobalt team is focusing a large amount of effort to identify and integrate
various methods to reduce the size of the Cobalt binary such as compression and
using less fonts.

### What if something goes wrong in the field? Can we rollback?

Yes, this is one of the benefits of Evergreen. We can initiate an update from
the server side that addresses problems that were undetected during full
testing. There are a formal set of guidelines to verify an updated binary
deployed to the device to ensure that it will work properly with no regressions
that partners should follow to ensure that there are no regressions. In
addition, it is also critical to do your own testing to exercise
platform-specific behavior.

### How can I be sure that Cobalt Evergreen will be optimized for my platform?

Much of the optimization work remains in the Starboard layer and configuration
so you should still expect good performance using Cobalt Evergreen. That being
said, the Cobalt Evergreen configuration allows you to customize Cobalt features
and settings as before.
