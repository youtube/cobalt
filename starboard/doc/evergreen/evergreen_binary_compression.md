# Evergreen Binary Compression

## What is Evergreen Binary Compression?

Evergreen Binary Compression is a feature that reduces the amount of space used
on-device to store Cobalt Core binaries. The binaries are stored compressed,
using the
[LZ4 Frame Format](https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md),
and decompressed when they are loaded and run. This optional feature is off by
default but partners can enable it by following the instructions below.

## Storage Savings

Across all reference devices tested, including Raspberry Pi 2, we have seen
compression ratios just above 2.0. We therefore expect that partners can halve
the space used for Cobalt binary storage by enabling the feature for all
installation slots: the system image slot and the writable slots.

## Caveats

### Performance Costs

Because the Cobalt Core shared library must be decompressed before it's loaded,
there is necessarily some effect on startup latency and CPU memory usage. Based
on our analysis across a range of devices these effects are relatively small.

For startup latency, we measured an increase in `libcobalt` load time in the
hundreds of milliseconds for the low-powered Raspberry Pi 2 and in the tens of
milliseconds for more representative, higher-powered devices. This increase is
small in the context of overall app load time.

For CPU memory usage, `libcobalt.lz4` is decompressed to an in-memory ELF file
before the program is loaded. We therefore expect CPU memory usage to
temporarily increase by roughly the size of the uncompressed `libcobalt` and
then to return to a normal level once the library is loaded. And this is exactly
what we've seen in our analysis.

### Incompatibilities

This feature is incompatible with the Memory Mapped file feature that is
controlled by the `--loader_use_mmap_file` switch. With compression, we lose the
required 1:1 mapping between the file and virtual memory. So,
`--loader_use_mmap_file` should not be set if compression is enabled for any
installation slots (next section).

## Enabling Binary Compression

Separate steps are required in order for a partner to enable compression for a)
the system image slot and b) (Evergreen-Full only) the 2+ writable slots on a
device.

Compression for the system image slot is enabled by installing `libcobalt.lz4`
instead of `libcobalt.so` in the read-only system image slot (i.e., SLOT_0).
Starting with a to be determined Cobalt 23 release, the open-source releases on
[GitHub](https://github.com/youtube/cobalt/releases) include separate CRX
packages, denoted by a "compressed" suffix, that contain the pre-built and
LZ4-compressed Cobalt Core binary.

Compression for the writable slots is enabled by configuring Cobalt to launch
with the `--use_compressed_updates` flag:

```
$ ./loader_app --use_compressed_updates
```

This flag instructs the Cobalt Updater to request, download, and install
packages containing compressed Cobalt Core binaries from Google Update and
Google Downloads. As a result, the device ends up with `libcobalt.lz4` instead
of `libcobalt.so` in the relevant slot after its next update.

Note that the system image slot is independent from the writable slots with
respect to the compression feature: the feature can be enabled for the system
image slot but disabled for the writable slots, or vice versa. However, we
expect that partners will typically enable the feature for all slots in order to
take full advantage of the storage reduction.

## Disabling Binary Compression

The compression feature is turned off by default. Once enabled, though, it can
be disabled by undoing the steps required for enablement.

Compression for the system image slot is disabled by installing the uncompressed
`libcobalt.so` instead of `libcobalt.lz4` in the read-only system image slot.

Compression for the writable slots is disabled by configuring Cobalt to launch
**without** the `--use_compressed_updates` flag. The Cobalt Updater will then be
instructed to request, download, and install packages containing the
uncompressed Cobalt Core binaries.

Note that disabling compression for the writable slots is eventual in the sense
that the compressed Cobalt Core binaries in these slots will remain and continue
to be used until newer versions of Cobalt Core are available on Google Update
and Google Downloads.

## FAQ

### Should multiple apps that share binaries also share compression settings?

The [Cobalt Evergreen Overview doc](cobalt_evergreen_overview.md) describes
multi-app support, where multiple Cobalt-based applications share Cobalt Core
binaries (i.e., they share the same installation slot(s)). When multi-app
support is used then, yes, the apps should also share compression settings.

For the system image slot, this means that either `libcobalt.lz4` or
`libcobalt.so` is installed in the one, shared slot.

For the writable slots, this means that the loader app is either launched with
`--use_compressed_updates` set for all apps or launched without it for all apps.
Otherwise, the different apps would compete with respect to whether the writable
slots are upgraded to compressed or uncompressed binaries.
