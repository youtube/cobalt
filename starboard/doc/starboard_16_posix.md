# Starboard 16 POSIX APIs


## Background
Updating existing Chromium components in the Cobalt repository or
bringing new ones requires porting to Starboard. This Starboardization process
takes a lot of time, however the majority of the APIs
involved in the porting are well established POSIX APIs with duplicates in
Starboard e.g: `malloc()` - `SbMemoryAllocate()`,
`socket()` - `SbSocketCreate()`, `open()` - `SbFileOpen()`,
`opendir()` - `SbDirectoryOpen()`, `pthread_mutex_create()` - `SbMutexCreate()`
 etc...

## Deprecated Starboard APIs
Starting with Starboard 16 the POSIX equivalent Starboard APIs were
deprecated and removed, and the standard POSIX APIs are used instead:

### Memory Management
Removed:

```
SbMemoryAllocate
SbMemoryAllocateAligned
SbMemoryAllocateAlignedUnchecked
SbMemoryAllocateNoReport
SbMemoryAllocateUnchecked
SbMemoryDeallocate
SbMemoryDeallocateAligned
SbMemoryDeallocateNoReport
SbMemoryFlush
SbMemoryFree
SbMemoryFreeAligned
SbMemoryMap
SbMemoryProtect
SbMemoryReallocate
SbMemoryReallocateUnchecked
SbMemoryUnmap
```

Supported POSIX equivalents:

```
calloc
free
malloc
posix_memalign
realloc
mmap
munmap
mprotect
msync
```


### Concurrency
Removed:

```
SbConditionVariableBroadcast
SbConditionVariableDestroy
SbConditionVariableCreate
SbConditionVariableSignal
SbConditionVariableWait
SbConditionVariableWaitTimed
SbMutexAcquire
SbMutexAcquireTry
SbMutexCreate
SbMutexDestroy
SbMutexRelease
SbThreadCreate
SbThreadCreateLocalKey
SbThreadDestroyLocalKey
SbThreadDetach
SbThreadGetCurrent
SbThreadGetLocalValue
SbThreadGetName
SbThreadIsEqual
SbThreadJoin
SbThreadSetLocalValue
SbThreadSetName
SbThreadSleep
SbThreadYield
SbOnce
```

Supported POSIX equivalents:

```
pthread_attr_init
pthread_attr_destroy
pthread_attr_getdetachstate
pthread_attr_getstacksize
pthread_attr_setdetachstate
pthread_attr_setstacksize
pthread_cond_broadcast
pthread_cond_destroy
pthread_cond_init
pthread_cond_signal
pthread_cond_timedwait
pthread_cond_wait
pthread_condattr_destroy
pthread_condattr_getclock
pthread_condattr_init
pthread_condattr_setclock
pthread_create
pthread_detach
pthread_equal
pthread_join
pthread_mutex_destroy
pthread_mutex_init
pthread_mutex_lock
pthread_mutex_unlock
pthread_mutex_trylock
pthread_once
pthread_self
pthread_getspecific
pthread_key_create
pthread_key_delete
pthread_setspecific
pthread_setname_np
pthread_getname_np
sched_yield
usleep
```

### I/O, sockets, files, directories
Removed:

```
SbDirectoryCanOpen
SbDirectoryClose
SbDirectoryCreate
SbDirectoryGetNext
SbDirectoryOpen
SbFileCanOpen
SbFileClose
SbFileDelete
SbFileExists
SbFileFlush
SbFileGetInfo
SbFileGetPathInfo
SbFileModeStringToFlags
SbFileOpen
SbFileRead
SbFileSeek
SbFileTruncate
SbFileWrite
SbSocketAccept
SbSocketBind
SbSocketClearLastError
SbSocketConnect
SbSocketCreate
SbSocketDestroy
SbSocketFreeResolution
SbSocketGetInterfaceAddress
SbSocketGetLastError
SbSocketGetLocalAddress
SbSocketIsConnected
SbSocketIsConnectedAndIdle
SbSocketIsIpv6Supported
SbSocketJoinMulticastGroup
SbSocketListen
SbSocketReceiveFrom
SbSocketResolve
SbSocketSendTo
SbSocketSetBroadcast
SbSocketSetReceiveBufferSize
SbSocketSetReuseAddress
SbSocketSetSendBufferSize
SbSocketSetTcpKeepAlive
SbSocketSetTcpNoDelay
SbSocketSetTcpWindowScaling
```

Supported POSIX equivalents:

```
accept
close
bind
connect
freeifaddrs
freeaddrinfo
fstat
fsync
ftruncate
getifaddrs
getaddrinfo
inet_ntop
listen
lseek
mkdir
read
recv
recvfrom
rmdir
setsockopt
send
sendto
stat
socket
unlink
write

```

### Strings
Removed:

```
SbStringCompareNoCase
SbStringCompareNoCaseN
SbStringDuplicate
SbStringScan
SbStringFormat
SbStringFormatWide
```

Supported POSIX equivalents:
```
strdup
strcasecmp
strncasecmp
vfwprintf
vsnprintf
vsscanf
```

### Time
Removed:

```
SbTimeGetMonotonicNow
SbTimeGetMonotonicThreadNow
SbTimeGetNow
SbTimeIsTimeThreadNowSupported
```

Supported POSIX equivalents:

```
clock_gettime
gettimeofday
gmtime_r
time
```
### Evergreen integration
![Evergreen Architecture](resources/starboard_16_posix.png)

#### POSIX ABI headers (musl_types)
The POSIX standard doesn't provide an ABI specification. There is guidance
around type definitions, but the actual memory representation is left
to the platform implementation. For example the `clock_gettime(CLOCK_MONOTONIC, &ts)` uses
various constants (e.g. CLOCK_MONOTONIC) to define which system clock to query
and a `struct timespec` pointer to populate the time data. The struct has 2
members, but the POSIX standard does not explicitly require that these members
be a specific number of bytes (e.g. the type long may be 4 or 8 bytes),
nor the padding/layout of the members in the struct
(e.g. a compiler is allowed to add additional padding).

The Starboard 16 POSIX ABI is defined by a combination of existing
[third_party/musl](../../third_party/musl) types and values and new types
and values added under [third_party/musl/src/starboard](../../third_party/musl/src/starboard).
All of the types have a concrete stable ABI defined per CPU architecture.

#### POSIX wrapper impl
The musl ABI types are translated to the native POSIX types in the
POSIX wrapper implementation.
The actual implementation, which assumes a native POSIX support is
implemented in `starboard/shared/modular`. For example
[starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h](../shared/modular/starboard_layer_posix_time_abi_wrappers.h),
[starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.cc](../shared/modular/starboard_layer_posix_time_abi_wrappers.cc).

If the provided implementation doesn't work out of the box
partners can fix it and adjust for their own internal platform. If the
fix is applicable to other POSIX implementations they can upstream the change
to the Cobalt project.

#### Exported Symbols Map
The Exported Symbols Map is part of the Evergreen loader and provides all the unresolved
external symbols required by the Coabalt Core binary. The map is defined as
`std::map<std::string, const void*>` and all the Starboard and POSIX symbols are
registered there. The implementation resides in
[starboard/elf_loader/exported_symbols.cc](../elf_loader/exported_symbols.cc).
For POSIX APIs a wrapper function is used whenever a translation
from `musl` types to platform POSIX types is needed e.g.

```
map_["clock_gettime"] = reinterpret_cast<const void*>(&__abi_wrap_clock_gettime);
```
The symbol may be registered directly without a wrapper if there is no need
for any translation or ajustements of the API e.g.
```
REGISTER_SYMBOL(malloc);
```

### Verification
A test suite [starboard/nplb/posix_compliance](../nplb/posix_compliance) is added to `nplb`
to verify the POSIX APIs specification and to enforce uniformity across all platforms.

The `elf_loader_sandbox` binary can be used to run tests in Evergreen mode.

The `elf_loader_sandbox` is run using two command line switches:
`--evergreen_library` and `--evergreen_content`. These switches are the path to
the shared library to be run and the path to that shared library's content.
These paths should be *relative to the content of the elf_loader_sandbox*.

For example, if we wanted to run the `nplb` set of tests and had the following
directory tree,

```
.../elf_loader_sandbox
.../content/app/nplb/lib/libnplb.so
.../content/app/nplb/content
```

we would use the following command to run `nplb`:

```
.../elf_loader_sandbox --evergreen_library=app/nplb/lib/libnplb.so
                       --evergreen_content=app/nplb/content
```

To build the `nplb` tests for Evergreen use the following commands.
The first of which builds the test using the Evergreen toolchain and
the second builds `elf_loader_sandbox` using the partner's toolchain:

```

ninja -C out/evergreen-arm-softfp_devel/ nplb_install
ninja -C out/${PLATFORM}_devel/ elf_loader_sandbox_install

```


### Backwards compatibility with Starboard 14 and Starboard 15
For older Starboard Versions e.g. 14 and 15 an emulation layer was provided through
the `third_party/musl` library. This should be completely transparent to
partner's integrations.
