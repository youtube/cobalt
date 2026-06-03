# Cobalt Versioning

Cobalt versions, as they appear in the user agent, use semantic versioning and have the following structure:

**[Major Version]**.**[Purpose]**.**[Minor Version]**.**[Build ID]**

The definitions of these version structure components are described below.

Example Cobalt versions would be:

  * `27.lts.1.1050000`
  * `27.lts.2.1050100`
  * `27.lts.3.1050200`
  * `27.lts.10.1051000`
  * `27.lts.12.1051200`
  * `27.lts.20.1052000`
  * `...`
  * `27.lts.40.1054000`

## Major Version

Cobalt major version is tied to a yearly release cycle and this number indicates
the yearly set that this version of Cobalt supports.  It is the last
two digits of the year of the target yearly set.  For example for the 2027
yearly set, this value will be `27`.

## Purpose

The purpose of this build, usually named after the reason that a branch is cut.
On the main branch it will be `main`, and on LTS branches for example it
will be `lts`.

## Minor Version

The current update revision number (e.g. release number). This will always be `0` on the main branch. When a release
branch is cut, it will be modified to start at `1`, and be incremented each time a
release or update is released.  It is possible that multiple updates are
released off of the same release branch, if new bugs are discovered and fixed.

## Build ID

The Cobalt Build ID represents `fine-grained` information about the state of the source tree for a given build. The number is simply a sequential, monotonically increasing commit number. E.g. every cherry-picked or merged commit on the release branch increments it by one.

A [`src/cobalt/build/get_build_id.py`](../build/get_build_id.py) script is used during the build to resolve the build ID number during builds, and can be similarly used to verify it in a checkout. Here is an example to get the Build ID in 27.lts.10 tag (hypothetical):
```sh
$ git checkout 27.lts.10
$ cobalt/build/get_build_id.py
1051000
```
*(Note: The actual path and availability of `get_build_id.py` may vary in newer releases, please refer to the build tools in your checkout.)*

# Stable Version

Starting with Cobalt 24, the version numbering scheme for **LTS Major and Minor
Stable releases are now numbered by multiples of 10**. This change aligns with
semantic versioning, adds more sensible differentiation for Stable vs
incremental non-Stable releases, and enables the Cobalt team to deliver faster
fixes, optimizations, and features to our mutual users. **Please only use
Cobalt LTS releases with `Minor Version` that are multiples of 10 for YouTube
Certification and Cobalt Device Maintenance Updates.**

NOTE: Pre-built Evergreen binaries are only available for Stable (non-RC)
releases at this time and available on the release page:
  * https://github.com/youtube/cobalt/releases

## For Example:

|Cobalt Release | Stable (Usable for cert)?|
|----------|--------|
|27.lts.1  | No     |
|27.lts.2  | No     |
|27.lts.3  | No     |
|27.lts.10 | **Yes**|
|27.lts.11 | No     |
|27.lts.12 | No     |
|27.lts.20 | **Yes**|

## FAQs

### What version should I use for certification?

  * Only Stable Cobalt versions. This is the same policy as before with the
    Cobalt versioning scheme.
  * E.g. Cobalt 27.lts.10, 27.lts.20, ..., 27.lts.40

### What about Evergreen binary versions?

  * Stable Evergreen binaries will also be numbered similarly with minor version numbers
  * E.g. 27.lts.40 (Evergreen binary version example: 7.10.2 - *actual versioning may vary*)

### Will Evergreen binaries be provided for all Cobalt releases?

  * No, at this time, pre-built Evergreen binaries will only be available for
    Stable releases (e.g. 27.lts.10, 27.lts.20, …, 27.lts.40). In the future,
    the Cobalt Team is planning to release matching pre-built Evergreen binaries
    for RC (non-Stable) releases that will be announced at a later time.
  * Please refer the release page to get all the available binaries:
    - https://github.com/youtube/cobalt/releases

# Other Reading

  * [Cobalt Branching](branching.md)
