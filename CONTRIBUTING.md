# Contributing guidelines

## Pull Request Checklist

Before sending your pull requests, make sure you do the following:

-   Read the [contributing guidelines](CONTRIBUTING.md).
-   Ensure you have signed the
    [Contributor License Agreement (CLA)](https://cla.developers.google.com/).
-   Check if your changes are consistent with the:
    -   [General guidelines](#general-guidelines-and-philosophy-for-contribution).
    -   [Coding Style](#coding-style).
-   Run the [unit tests](#running-unit-tests).

### Contributor License Agreements

We'd love to accept your patches! Before we can take them, we have to jump a couple of legal hurdles.

Please fill out either the individual or corporate Contributor License Agreement (CLA).

  * If you are an individual writing original source code and you're sure you own the intellectual property, then you'll need to sign an [individual CLA](https://code.google.com/legal/individual-cla-v1.0.html).
  * If you work for a company that wants to allow you to contribute your work, then you'll need to sign a [corporate CLA](https://code.google.com/legal/corporate-cla-v1.0.html).

Follow either of the two links above to access the appropriate CLA and instructions for how to sign and return it. Once we receive it, we'll be able to accept your pull requests.

***NOTE***: Only original source code from you and other people that have signed the CLA can be accepted into the main repository.

### Community Guidelines

This project follows
[Google's Open Source Community Guidelines](https://opensource.google/conduct/).

### Contributing code

If you have improvements to Cobalt, send us your pull requests! For those
just getting started, Github has a
[how to](https://help.github.com/articles/using-pull-requests/).

Cobalt team members will be assigned to review your pull requests. A team
member will need to approve the workflow runs for each pull request. Once the
pull requests are approved and pass *all* presubmit checks, a Cobalt
team member will merge the pull request.

### Contribution guidelines and standards

Before sending your pull request for
[review](https://github.com/youtube/cobalt/pulls),
make sure your changes are consistent with the guidelines and follow the
Cobalt coding style.

#### General guidelines and philosophy for contribution

*   Ensure your change references the bug you are addressing. Follow the
    instructions
    [here](https://cobalt.dev/communication.html#filing-bugs-and-feature-requests)
    to view and create bugs.
*   Include unit tests when you contribute new features, as they help to:
    1.   Prove that your code works correctly
    1.   Guard against future breaking changes to lower the maintenance cost.
*   Bug fixes also generally require unit tests, because the presence of bugs
    usually indicates insufficient test coverage.
*   When you contribute a new feature to Cobalt, the maintenance burden is
    (by default) transferred to the Cobalt team. This means that the benefit
    of the contribution must be compared against the cost of maintaining the
    feature.
*   As every PR requires several CPU/GPU hours of CI testing, we discourage
    submitting PRs to fix one typo, one warning,etc. We recommend fixing the
    same issue at the file level at least (e.g.: fix all typos in a file, fix
    all compiler warning in a file, etc.)

#### License

Include a license at the top of new files. Check existing files for license examples.

#### Coding style

Cobalt follows the
[Chromium style guide](https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/styleguide.md).

Cobalt uses pre-commit to ensure good coding style. Create a python 3 virtual
environment for working with Cobalt, then install `pre-commit` with:

```bash
$ pre-commit install -t post-checkout -t pre-commit -t pre-push --allow-missing-config
```

`pre-commit` will mostly run automatically, and can also be invoked manually.
You can find documentation about it at https://pre-commit.com/.

#### Running unit tests

First, ensure Docker and Docker Compose are installed on your system. Then,
you can run unit tests for our linux reference implementation using:

```bash
$ docker compose up --build --no-start linux-x64x11-unittest
$ PLATFORM=linux-x64x11 CONFIG=devel TARGET=all docker compose run linux-x64x11
$ PLATFORM=linux-x64x11 CONFIG=devel docker compose run linux-x64x11-unittest
```
