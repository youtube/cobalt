# Debugging with Swarming

This document outlines how to debug a test failure on a specific builder
configuration without needing to repeatedly upload new CL revisions or do CQ dry
runs.

[TOC]

## Overview & Terms

*Swarming* is a system operated by the infra team that schedules and runs tasks
under a specific set of constraints, like "this must run on a macOS 10.13 host"
or "this must run on a host with an intel GPU". It is somewhat similar to part
of [Borg], or to [Kubernetes].

An *isolate* is an archive containing all the files needed to do a specific task
on the swarming infrastructure. It contains binaries as well as any libraries
they link against or support data. An isolate can be thought of like a tarball,
but held by the CAS server and identified by a digest of its contents. The
isolate also includes the command(s) to run, which is why the command is
specified when building the isolate, not when executing it.

Normally, when you do a CQ dry run, something like this happens:

```
  for type in builders_to_run:
    targets = compute_targets_for(type)
    isolates = use_swarming_to_build(type, targets) # uploads isolates for targets
    wait_for_swarming_to_be_done()

    for isolate in isolates:
      use_swarming_to_run(type, isolate) # downloads isolates onto the bots used
    wait_for_swarming_to_be_done()
```

When you do a CQ retry on a specific set of bots, that simply constrains
`builders_to_run` in the pseudocode above. However, if you're trying to rerun a
specific target on a specific bot, because you're trying to reproduce a failure
or debug, doing a CQ retry will still waste a lot of time - the retry will still
build and run *all* targets, even if it's only for one bot.

Fortunately, you can manually invoke some steps of this process. What you really
want to do is:

```
  isolate = use_swarming_to_build(type, target) # can't do this yet, see below
  use_swarming_to_run(type, isolate)
```

or perhaps:

```
  isolate = upload_to_cas(target_you_built_locally)
  use_swarming_to_run(type, isolate)
```

## The easy way

A lot of the steps described in this doc have been bundled up into 2
tools. Before using either of these you will need to
[authenticate](#authenticating).

### run-swarmed.py

A lot of the logic below is wrapped up in `tools/run-swarmed.py`, which you can run
like this:

```
$ tools/run-swarmed.py $outdir $target [-- --gtest_filter=...]
```

See the `--help` option of `run-swarmed.py` for more details about that script.
Many flags are converted into dimensions to pass to `mb.py`; see
[Bot selection criteria](#bot-selection-criteria) for possible values. (Most
dimensions have the same name as the flag, but `--swarming-os` is just the `os`
dimension.)

Note you might need `--swarming-os Ubuntu-14.04` if you get an error like,
`UnboundLocalError: local variable 'dbus_pid' referenced before assignment`.

Web tests can be also run on swarmed. However:
- the only supported output folders are `Debug` and `Release`
- `--no-test-flags` must be specified.

For example, you can run all Web Platform Tests inside `<some-wpt-folder>` like this:
```
$ autoninja -C out/Release blink_tests
$ tools/run-swarmed.py --no-test-flags out/Release blink_wpt_tests <some-wpt-folder>
```

### mb.py run

Similar to `tools/run-swarmed.py`, `mb.py run` bundles much of the logic into a
single command line. Unlike `tools/run-swarmed.py`, `mb.py run` allows the user
to specify extra arguments to pass to the test, but has a messier command line.

To use it, run:
```
$ tools/mb/mb.py run \
    -s --no-default-dimensions \
    -d pool $pool \
    $criteria \
    $outdir $target \
    -- $extra_args
```

## A concrete example

Here's how to run `chrome_public_test_apk` on a bot with a Nexus 5 running KitKat.

```sh
$ tools/mb/mb.py run \
    -s --no-default-dimensions \
    -d pool chromium.tests \
    -d device_os_type userdebug -d device_os KTU84P -d device_type hammerhead \
    out/Android-arm-dbg chrome_public_test_apk
```

This assumes you have an `out/Android-arm-dbg/args.gn` like

```
ffmpeg_branding = "Chrome"
is_component_build = false
is_debug = true
proprietary_codecs = true
strip_absolute_paths_from_debug_symbols = true
symbol_level = 1
system_webview_package_name = "com.google.android.webview"
target_os = "android"
use_goma = true
```

## Bot selection criteria

The examples in this doc use `$criteria`. To figure out what values to use, you
can go to an existing swarming run
([recent tasks page](https://chromium-swarm.appspot.com/tasklist)) and
look at the `Dimensions` section. Each of these becomes a `-d dimension_name
dimension_value` in your `$criteria`. Click on `bots` (or go
[here](https://chromium-swarm.appspot.com/botlist)) to be taken to a UI that
allows you to try out the criteria interactively, so that you can be sure that
there are bots matching your criteria. Sometimes the web page shows a
human-friendly name rather than the name required on the commandline. [This
file](https://cs.chromium.org/chromium/infra/luci/appengine/swarming/ui2/modules/alias.js)
contains the mapping to human-friendly names. You can test your commandline by
entering `dimension_name:dimension_value` in the interactive UI.

## Building an isolate

At the moment, you can only build an isolate locally, like so (commands you type
begin with `$`):

```
$ tools/mb/mb.py isolate //$outdir $target
```

This will produce some files in $outdir. The most pertinent two are
`$outdir/$target.isolate` and `$outdir/target.isolated`. If you've already built
$target, you can save some CPU time and run `tools/mb/mb.py` with `--no-build`:

```
$ tools/mb/mb.py isolate --no-build //$outdir $target
```

Support for building an isolate using swarming, which would allow you to build
for a platform you can't build for locally, does not yet exist.

## Authenticating

You may need to log in to `https://chromium-swarm.appspot.com` to do this

```
$ tools/luci-go/isolate login
```

Use your google.com account for this. On Windows the command would be:

```
$ tools\luci-go\isolate.exe login
```

## Uploading an isolate

You can then upload the resulting isolate to the CAS server:

```
$ tools/luci-go/isolate archive \
      -cas-instance chromium-swarm \
      -i $outdir/$target.isolate \
      -dump-json $outdir/$target.archive.json
```

The archive json looks like this:

```
{
  "unit_tests": "5bee0815d2ddd2b876b49d4cce8aaa23de8a6f9e2dbf134ec409fbdc224e8c64/398"
}
```

Do not ctrl-c it after it does this, even if it seems to be hanging for a
minute - just let it finish.

## Running an isolate

Now that the isolate is on the CAS server with digest `$digest` from the
previous step, you can run on bots of your choice:

```
$ tools/luci-go/swarming trigger \
    -server https://chromium-swarm.appspot.com \
    -dimension pool=$pool \
    $criteria \
    -digest $digest
```

There are two more things you need to fill in here. The first is the pool name;
you should pick "chromium.tests" unless you know otherwise. The pool is the
collection of hosts from which swarming will try to pick bots to run your tasks.

The second is the criteria, which is how you specify which bot(s) you want your
task scheduled on. These are specified via "dimensions", which are specified
with `-dimension key=val`. In fact, the `-dimension pool=$pool` in the
command above is selecting based on the "pool" dimension. There are a lot of
possible dimensions; one useful one is "os", like `-dimension os=Linux`. Examples of
other dimensions include:

* `-dimension os=Mac10.13.6` to select a specific OS version
* `-dimension device_type="Pixel 3"` to select a specific Android device type
* `-dimension gpu=8086:1912` to select a specific GPU

The [swarming bot list] allows you to see all the dimensions and the values they
can take on.

And you can pass shard number via env flags, e.g.

* `-env GTEST_SHARD_INDEX=0` to specify which shard to run.
* `-env GTEST_TOTAL_SHARDS=1` to specify total number of shards.

If you need to pass additional arguments to the test, simply add
`-- $extra_args` to the end of the `swarming trigger` command line - anything
after the `--` will be passed directly to the test.

When you invoke `swarming trigger`, it will emit a piece of information: a
command you can run to collect the results of that task. For example:

```
To collect results use:
  swarming collect -server https://chromium-swarm.appspot.com 4a0e739053fddd10
```

The 'collect' command given there will block until the task is complete, then
produce the task's results, or you can load that URL and watch the task's
progress.

## Other notes

If you are looking at a Swarming task page, be sure to check the bottom of the
page, which gives you commands to:

* Download the contents of the isolate the task used
* Reproduce the task's configuration locally
* Download all output results from the task locally

[borg]: https://ai.google/research/pubs/pub43438
[kubernetes]: https://kubernetes.io/
[swarming bot list]: https://chromium-swarm.appspot.com/botlist
