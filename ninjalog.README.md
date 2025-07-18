# Ninja build log collection

[TOC]

## Overview

When chromium developers use autoninja for their build,

e.g.
```
$ autoninja -C out/Release chrome
```

autoninja uploads ninja's build log for Google employees but we don't collect
logs from external contributors.

We use `goma_auth info` to decide whether an autoninja user is a Googler or not.

Before uploading logs, autoninja shows a message 10 times to warn users that we
will collect build logs.

autoninja users can also opt in or out by using the following commands:

* `ninjalog_uploader_wrapper.py opt-in`
* `ninjalog_uploader_wrapper.py opt-out`

## What type of data are collected?

The collected build log contains

* output file of build tasks (e.g. chrome, obj/url/url/url_util.o)
* hash of build command
* start and end time of build tasks

See [manual of ninja](https://ninja-build.org/manual.html#ref_log) for more
details.

In addition to ninja's build log, we send the following data for further
analysis:

* OS (e.g. Win, Mac or Linux)
* number of cpu cores of building machine
* build targets (e.g. chrome, browser_tests)
* parallelism passed by -j flag
* following build configs
  * host\_os, host\_cpu
  * target\_os, target\_cpu
  * symbol\_level
  * use\_goma
  * is\_debug
  * is\_component\_build

 We don't collect personally identifiable information
(e.g. username, ip address).

## Why ninja log is collected? / How the collected logs are used?

We (goma team) collect build logs to find slow build tasks that harm developer's
productivity. Based on collected stats, we find the place/build tasks where we
need to focus on. Also we use collected stats to track chrome build performance
on developer's machine. We'll use this stats to measure how much we can/can't
improve build performance on developer's machine.
