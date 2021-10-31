---
layout: doc
title: "Contributing to Cobalt"
---

We'd love to hear about how you would like to contribute to Cobalt!
Please read through this document first to understand the process and
to make sure you know what to expect.

## Before You Contribute

### As an Individual

Before Cobalt can use your code, as an unaffiliated individual, you must sign
the [Google Individual Contributor License
Agreement](https://cla.developers.google.com/about/google-individual) (CLA).
You can complete that process online.

### As a Company

If you represent a company that wishes to have one or more employees contribute
to Cobalt on behalf of your company, you need to agree to the
[Software Grant and Corporate Contributor License Agreement](
https://cla.developers.google.com/about/google-corporate).

### What is a CLA?

The Contributor License Agreement is necessary mainly because you own the
copyright to your changes, even after your contribution becomes part of our
codebase, so we need your permission to use and distribute your code. We also
need to be sure of various other things — for instance that you‘ll tell us if
you know that your code infringes on other people’s patents.

You don‘t have to sign the CLA until after you’ve submitted your code for
review and a member has approved it, but you must do it before we can put
your code into our codebase. Before you start working on a larger
contribution, get in touch with us to discuss your idea so that we can help
out and possibly guide you. Early coordination makes it much easier to avoid
frustration later on.

### Code Reviews

All submissions, including submissions by project members, require review. We
currently use [Gerrit Code Review](https://www.gerritcodereview.com/) for this
purpose. Currently, team-member submissions are reviewed privately, and
external submissions go through public reviews.

## Submission Process

The following steps explain the submission process:

*  Ensure you or your company have signed the appropriate CLA as discussed
   in the [Before You Contribute](#before-you-contribute) section above.
*  Rebase your changes down into a single git commit.
*  Run `git push origin HEAD:refs/for/master` to upload the review to
   [Cobalt's Gerrit instance](https://cobalt-review.googlesource.com/).
*  Someone from the maintainers team reviews the code, adding comments on
   any things that need to change before the code can be submitted.
*  If you need to make changes, make them locally, test them, then
   `git commit --amend` to add them to the **existing** commit. Then return
   to step 2.
*  If you do not need to make any more changes, a maintainer integrates the
   change into our private repository, and it is pushed out to the public
   repository after some time.
