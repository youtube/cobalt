---
layout: doc
title: "Starboard API Review Process"
---
# Starboard API Review Process

## Why do we need a process?
The Starboard API is the contract between Starboard applications and
implementors of the Starboard API. Changing existing APIs and adding new
required APIs breaks that contract and increases the cost of implementors
keeping their Starboard port up-to-date. Pushing a release to the Open Source
repository signals to Starboard implementors that any non-experimental APIs in
that version will not change for as long as that version of Starboard is
supported by the Starboard applications. We cannot change those newly frozen
APIs without causing a potentially significant disruption to any partners who
have already implemented them, or are in the process of implementing them.

While having a process may make it harder to add new things to Starboard, it is
much harder to remove or change things that are already there.

Thus we need to give special focus to changes to the Starboard API to ensure its
consistency with existing APIs design principles. Unnecessary churn on the
Starboard API creates more work for Starboard application developers and may
discourage porters from keeping Starboard applications up-to-date on their
platforms. This process is intended to save time and effort for both Starboard
application developers and Starboard implementors in the long run, and
illustrates the complexity of dealing with a wide variety of platforms
simultaneously.

## So you want to add a new API?
Historically, we have done API review as a part of the Code Review process using
Gerrit. This works well for small-ish changes. For larger changes, consider
writing a design document up front before defining the new API.

### Who does the review?
Send a message to the public cobalt-dev group to request a review.

### What is the process?
Developers are strongly encouraged to create the interface and upload that to
Gerrit for review before spending time on stubs and tests. Iteration on the
interface will necessarily result in changes to the stubs and tests, which can
result in more work for the implementer of a new API.

1. Upload a .h file with Module Overview and (optionally) initial function
   prototypes
    * New APIs should be declared in the experimental version, as described in the
      starboard versioning doc.
2. Discuss the new API with whoever is performing the review, and address
   comments.
3. Iterate.
    * As a part of the review process, the reviewer will work with you to ensure
      that the new API adheres to the starboard principles.
4. Finalize function declarations.
5. Implement tests and stubs.
    * Existing platforms on trunk should not break as a result of this change.
    * At this point, you may submit the interface, tests, and stubs with your
      reviewer’s +2.
6. Implement the interface for at least one platform.
7. Iterate
8. It may be that implementation of the API reveals things that were overlooked
   during the earlier stages of the review.

Ideally most major points of feedback will be caught early in the review process
before much time has been spent on implementation.
In the case that the platform in (6) is an internal platform, provide a
reference implementation for at least one external reference platform. This can
be in a follow-up CL, but must be implemented before the new API is frozen
(see [versioning.md](versioning.md)).

## How to design a new API
See [principles.md](principles.md) for a guide on how to design a good Starboard
API.
