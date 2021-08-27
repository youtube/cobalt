---
layout: doc
title: "How to Symbolize Dumps"
---
# How to Symbolize Dumps

Evergreen will store the minidumps (`.dmp` files) from the 2 most recent
crashes on the disk. They are stored under `kSbSystemPathCacheDirectory` in the
subdirectory `crashpad_database/`. These files can be used along with
Breakpad's tools to get a full stacktrace of the past crashes. This can help in
debugging, as these minidumps have the information for the dynamic
`libcobalt.so` module correctly mapped, which a out-of-the-box dumper could not
manage.

## Obtaining the Tools to Symbolize Minidumps

Tools for symbolizing these dumps are available through
[Breakpad](https://chromium.googlesource.com/breakpad/breakpad/). Breakpad is
an open source crash reporting library that we use to obtain symbol files
(`.sym`) from unstripped binaries, and to process the symbol files with the
minidumps to produce human-readable stacktraces.


### Building Breakpad

[Breakpad](https://chromium.googlesource.com/breakpad/breakpad/) provides
instructions for building these tools yourself. The
[Getting Started with Breakpad](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/getting_started_with_breakpad.md)
guide is a good place to start if you want to go through the docs yourself, but
below is a brief overview of how to get and build the tools.

Download depot_tools:
```
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
$ export PATH=/path/to/depot_tools:$PATH
```

Get breakpad:
```
$ mkdir breakpad && cd breakpad
$ fetch breakpad
$ cd src
```

Build breakpad:
```
$ ./configure && make
```

This will build the processor (`src/processor/minidump_stackwalk`), and when
building on Linux it will also build the `dump_syms` tool
(`src/tools/linux/dump_syms/dump_syms`).

**IMPORTANT:** Once you have fetched Breakpad, you should remove the path to
depot_tools from your `$PATH` environment variable, as it can conflict with
Cobalt's depot_tools.

## Symbolizing Minidumps

Now that you have all the tools you need, we can symbolize the dumps. To be
able to symbolize Cobalt using Evergreen, you need to be get the unstripped
`libcobalt.so` binary. These will be available as assets in GitHub releases
[on Cobalt's public GitHub repo](https://github.com/youtube/cobalt/releases).

libcobalt releases will be labeled by the Evergreen version, the architecture,
the config, and the ELF build id, for example
"libcobalt_1.0.10_unstripped_armeabi_softfp_qa_ac3132014007df0e.tgz". Here, we
have:
* Evergreen Version: 1.0.10
* Architecture: armeabi_softfp
* Config: qa
* ELF Build Id: ac3132014007df0e

Knowing the architecture and config you want, you'll just have to know which
version of Evergreen you're on or obtain the build id of the library. If you
need to obtain the ELF build id, you can do so easily by running
`readelf -n /path/to/libcobalt.so` and look at the hash displayed after "Build
ID:".

Now you can get the debug symbols from the library using the tools we
downloaded previously. Unpack libcobalt and dump its symbols into a file:

```
$ tar xzf /path/to/libcobalt.tgz
$ /path/to/dump_syms /path/to/unzipped/libcobalt > libcobalt.so.sym
$ head -n1 libcobalt.so.sym
MODULE Linux x86_64 6462A5D44C0843D100000000000000000 libcobalt.so
```

We run `head` on the symbol file to get the debug identifier, the hash
displayed above (in this case, it's `6462A5D44C0843D100000000000000000`). Now
we can create the file structure that `minidump_stackwalker` expects and run
the stackwalker against the minidump:

```
$ mkdir -p symbols/libcobalt.so/<debug identifier>/
$ mv libcobalt.so.sym symbols/libcobalt.so/<debug identifier>/
$ /path/to/minidump_stackwalk /path/to/your/minidump.dmp symbols/
```

`minidump_stackwalk` produces verbose output on stderr, and the stacktrace on
stdout, so you may want to redirect stderr.

### Addendum: Adding Other Symbols

We can use the process above to add symbols for any library or executable you
use, not just `libcobalt.so`. To do this, all you have to do is run the
`dump_syms` tools on the binary you want symbolized and put that in the
"symbols/" folder.

```
$ /path/to/dump_syms /path/to/<your-binary> > <your-binary>.sym
$ head -n1 <your-binary.sym>
MODULE Linux x86_64 <debug-identifier> <your-binary>
$ mkdir -p symbols/<your-binary>/<debug-identifier>
$ mv <your-binary>.sym symbols/<your-binary>/<debug-identifier>/
```

Now, `minidump_stackwalk` should symbolize sections within `<your-binary>`. For
example, if you decided to symbolize the `loader_app`, it would transform the
stacktrace output from `minidump_stackwalk` from:

```
9  loader_app + 0x3a31130
```

to:

```
9  loader_app!SbEventHandle [sandbox.cc : 44 + 0x8]
```

Note that the addresses will vary.
