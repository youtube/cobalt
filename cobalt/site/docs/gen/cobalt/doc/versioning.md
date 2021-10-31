---
layout: doc
title: "Cobalt Versioning"
---
# Cobalt Versioning

Cobalt versions, as they appear in the user agent, have the following structure:

**[Feature Year]**.**[Purpose]**.**[Update Number]**.**[Build ID]**

The meansions of these components are described below.

Example Cobalt versions would be:

  * `19.lts.1.40455`
  * `19.lts.2.53047`
  * `20.lts.1.64553`

## Feature Year

Cobalt features are tied to a yearly release cycle and this number indicates
the yearly feature set that this version of Cobalt supports.  It is the last
two digits of the year of the target feature set.  For example for the 2019
feature set, this value will be `19`.

## Purpose

The purpose of this build, usually named after the reason that a branch is cut.
On the master branch it will be `master`, and on LTS branches for example it
will be `lts`.

## Update Number

The current update revision number (e.g. release number) for a given pair of
values above.  This will always be `0` on the master branch.  When a release
branch is cut, will be modified to start at `1`, and be incremented each time a
release or update is released.  It is possible that multiple updates are
released off of the same release branch, if new bugs are discovered and fixed.

## Build ID

The Cobalt Build ID represents **fine-grained** information about the state of
the source tree for a given build. An internal Cobalt build server generates a
monotonically increasing number for each unique set of sources that it
sees. When an open-source release is published,
a [`src/cobalt/build/build.id`](../build/build.id) file is included that
specifies the build ID of that source release. The Cobalt team can reproduce the
exact sources for a given Build ID.

Note that since the Build ID always increases with time, it means that the
latest version of an older Cobalt release can have a higher Build ID than the
latest version of a new Cobalt release. An example from above: Cobalt `4.16134`
was produced earlier than Cobalt `3.16138`, thus has a lower Build ID.

## Older Cobalt versioning scheme

A Cobalt version consists of two components: The Release Number and the Build
ID. Some real historical Cobalt version examples:

  * `2.15147`
  * `3.16138`
  * `4.16134`
  * `6.18971`

You get the idea. The number before the dot is the "Release Number." The number
after the dot is the "Build ID."

### Release Number

In older Cobalt versioning schemes, a "Cobalt release" is an official, tested
version of Cobalt that is intended to be deployable to production. The
"Release Number" is a single counting number, starting at "1" for our first
release, and increasing by one for every release thereafter. This number is
checked into [`src/cobalt/version.h`](../version.h), and represents **coarse**
information about the state of the source tree when we decided to do a release.

It is important to note that there are no point releases, or major or minor
releases. Each release gets its own unique counting number.

## Other Reading

  * [Cobalt Branching](branching.md)
