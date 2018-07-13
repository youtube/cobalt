# Cobalt Branching

*(This document assumes you are already familiar
with [Cobalt Versioning][versioning] practices.)*

The Cobalt project uses git branches for two main purposes:

  1. To solidify a release as it is hardened for production.
  2. To isolate `master` branch developers from risky or disruptive work.


## Release Branches

A "Cobalt release" is an official, tested version of Cobalt that is intended to
be deployable to production. We branch for releases to allow development to
continue on the `master` branch while stabilizing and finalizing a set of sources
for release.


### Release Timeline

  1. Feature work is done in the `master` branch.

  2. Once all feature work is completed, a release branch is created. The branch
     will be named "[[Feature Year](versioning.md#Feature-Year)].[[Purpose](versioning.md#Purpose)].[[Update Number](versioning.md#Update-Number)]+".
     Note that while very similar to the structure of the Cobalt
     [version](versioning.md), it features a `+` symbol at the end, indicating
     that the branch may eventually contain multiple release updates,
     all greater than or equal to the specified update number.  In particular, a
     single branch may host multiple releases/updates. Should another release
     branch be cut from master with a pre-existing (feature year, purpose)
     pair, the new branch will have an update number equivalent to the most
     recently released update number, plus one.  Note that we expect it to be
     rare that we will need a branch other than the `1+` branch.

     An example release branch name is `19.lts.1+`.

     An RC announcement will be made to
     [cobalt-dev@googlegroups.com][cobalt-dev].

     Note that a release branch implies that code on that branch is being
     stabilized, not that it is ready for release.  Versions of Cobalt that
     are ready for release will have a dedicated `*.stable` branch pointing to
     them, and will be discussed later.

  3. As bugs are discovered and feedback received from partners, fixes will be
     cherry-picked into the release candidate branch. These cherry-picks should
     not be considered stable again until the `*.stable` branch is updated.

  4. As time goes on, the number of cherry-picks will decrease in number and
     scope.

  5. Once a commit on the branch is deemed to be feature-complete and stable, it
     will be tagged with the current [version](versioning.md) for that branch,
     and the version will be incremented for all subsequent commits.  A special
     branch that acts more like a "moving tag" named "[[Feature Year](versioning.md#Feature-Year)].[[Purpose](versioning.md#Purpose)].stable"
     will be created to point to the newly released version.  Should a
     subsequent update be made for the given feature year and purpose, the
     `*.stable` branch will be updated to point to the newest update.

     An example stable branch name is `19.lts.stable`.

     Some example release tags are:
      - `19.lts.1`
      - `19.lts.2`
      - `20.lts.1`

     A release announcement will be made
     to [cobalt-dev@googlegroups.com][cobalt-dev].


## Work Branches

If a set of work is deemed to be particularly risky or disruptive, or if a
serious contributor wants a sandbox to prepare an extensive patch, a work branch
may be created to facilitate such development.

Work branch names are of the form `work_<topic>`, where `<topic>` is the purpose
for which the branch was created. Work branches are generally in use by a
specific and limited set of people, and may disappear at any time.


## Older branching schemes

Older branches have been following different branch naming schemes, and for
a description of those schemes, the version of this branching.md file within
those branches should be consulted.

## Other Reading

  * [Cobalt Versioning][versioning]

[cobalt-dev]: https://groups.google.com/forum/#!forum/cobalt-dev "cobalt-dev@googlegroups.com"
[versioning]: versioning.md "Cobalt Versioning"
