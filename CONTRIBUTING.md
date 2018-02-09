# Contributing to Cobalt

We'd love to hear about how you would like to contribute to Cobalt! It's worth
reading through this modest document first, to understand the process and to
make sure you know what to expect.


## Before You Contribute

### As an Individual

Before Cobalt can use your code, as an unaffiliated individual, you must sign
the [Google Individual Contributor License Agreement](https://cla.developers.google.com/about/google-individual) (CLA), which you can do online.

### As a Company

If you are a company that wishes to have one or more employees contribute to
Cobalt on-the-clock, that is covered by a different agreement, the
[Software Grant and Corporate Contributor License Agreement](https://cla.developers.google.com/about/google-corporate).

### What is a CLA?

The Contributor License Agreement is necessary mainly because you own the
copyright to your changes, even after your contribution becomes part of our
codebase, so we need your permission to use and distribute your code. We also
need to be sure of various other things — for instance that you'll tell us if
you know that your code infringes on other people's patents. You don't have to
sign the CLA until after you've submitted your code for review and a member has
approved it, but you must do it before we can put your code into our codebase.
Before you start working on a larger contribution, you should get in touch with
us first with your idea so that we can help out and possibly guide
you. Coordinating up front makes it much easier to avoid frustration later on.


### Code Reviews

All submissions, including submissions by project members, require review. We
currently use [Gerrit Code Review](https://www.gerritcodereview.com/) for this
purpose. Currently, team-member submissions go through private reviews, and
external submissions go through public reviews.


## Submission Process

We admit that this submission process is currently not completely optimized to
make contributions easy, and we hope to make improvements to it in the
future. It will always include some form of signing the CLA and submitting the
code for review before merging changes into the Cobalt master tree.

  1. Ensure you or your company have signed the appropriate CLA (see "Before You
     Contribute" above).
  1. Rebase your changes down into a single git commit.
  1. Run `git clang-format HEAD~` to apply default C++ formatting rules,
     followed by `git commit -a --amend` to squash any formatting changes
     into your commit.
  1. Run `git cl upload` to upload the review to
     [Cobalt's Gerrit instance](https://cobalt-review.googlesource.com/).
  1. Someone from the maintainers team will review the code, putting up comments
     on any things that need to change for submission.
  1. If you need to make changes, make them locally, test them, then `git commit
     --amend` to add them to the *existing* commit. Then return to step 2.
  1. If you do not need to make any more changes, a maintainer will integrate
     the change into our private repository, and it will get pushed out to the
     public repository after some time.

