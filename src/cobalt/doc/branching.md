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

  2. Once all feature work is completed,
     a [Release Number](versioning.md#Release-Number) is reserved and a branch
     is created. The branch will be named `rc_<release-number>` to indicate that
     the branch represents a release candidate for version `<release-number>`,
     and not the final release.

     An RC announcement will be made to [cobalt-dev@googlegroups.com][cobalt-dev].

     It should be understood that RC branches are being stabilized and are
     subject to changes.

  3. As bugs are discovered and feedback received from partners, fixes will be
     cherry-picked into the release candidate branch. Each update to
     `rc_<release-number>` will have an
     increasing [Build ID](versioning.md#Build-ID).

  4. As time goes on, the number of cherry-picks will decrease in number and
     scope.

  5. Once the branch is deemed to be feature-complete and stable, it will be
     **renamed** to `release_<release-number>`, representing an actual Cobalt
     release. This means that the RC branch will be removed and the release
     branch will permanently take its place.

     A release announcement will be made
     to [cobalt-dev@googlegroups.com][cobalt-dev].

  6. The Cobalt project will strongly resist updating released versions, but may
     need to back-port fixes from time to time. This is especially true if there
     are any severe security flaws that need to be addressed. So cherry-picks
     are still possible into `release_<release-number>` release branches.

     In these rare cases, announcements will be made
     to [cobalt-dev@googlegroups.com][cobalt-dev].


## Work Branches

If a set of work is deemed to be particularly risky or disruptive, or if a
serious contributor wants a sandbox to prepare an extensive patch, a work branch
may be created to facilitate such development.

Work branch names are of the form `work_<topic>`, where `<topic>` is the purpose
for which the branch was created. Work branches are generally in use by a
specific and limited set of people, and may disappear at any time.


## Capitalization, Branch Names, and You

Some older branches followed an `ALL_CAPS_SNAKE_CASE` naming convention.  Going
forward, branch names will follow a `lower_snake_case` naming convention.


## Other Reading

  * [Cobalt Versioning][versioning]

[cobalt-dev]: https://groups.google.com/forum/#!forum/cobalt-dev "cobalt-dev@googlegroups.com"
[versioning]: versioning.md "Cobalt Versioning"
