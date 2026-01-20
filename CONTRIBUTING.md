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

#### Commit messages

The standard guidance is described in [How to write a git commit message](https://cbea.ms/git-commit). It contains a general [introduction](https://cbea.ms/git-commit/#intro), as well as [seven rules](https://cbea.ms/git-commit/#seven-rules) that should be followed when writing commit messages in this repository:

1. [Separate subject from body with a blank line](https://cbea.ms/git-commit/#separate)
2. [Limit the subject line to 50 characters](https://cbea.ms/git-commit/#limit-50)
3. [Capitalize the subject line](https://cbea.ms/git-commit/#capitalize)
4. [Do not end the subject line with a period](https://cbea.ms/git-commit/#end)
5. [Use the imperative mood in the subject line](https://cbea.ms/git-commit/#imperative)
6. [Wrap the body at 72 characters](https://cbea.ms/git-commit/#wrap-72)
7. [Use the body to explain _what_ and _why_ vs. _how_](https://cbea.ms/git-commit/#why-not-how)

##### Tags

To improve the clarity and scannability of our commit history, the commit subject line must have a tag.

Tags are divided into two main categories: **component** tags specify the affected part of the codebase (e.g. media, build, android), while **type** tags describe the nature of the change (e.g., feat, fix, refactor). Prefer component tags over type tags.

When your commit relates to one of these topics, select the most relevant tag from either category and add it to the beginning of your subject line, followed by a colon.

Examples:

* `media: Add support for AV1 playback`
* `fix: Correct screen tearing on resume`
* `docs: Update build instructions for Linux`
* `starboard: Implement SbWindowGetPlatformHandle for Wayland`
* `refactor: Simplify threading model in the renderer process`
* `build: Update third-party dependency versions in DEPS`
* `net: Improve QUIC connection reliability on flaky networks`
* `cleanup: Remove deprecated functions from the public API`

**By Component**

| Tag | Description |
| - | - |
| android | Android-specific changes |
| tvos | tvOS-specific changes |
| build | Changes to the build system (GN files, build scripts) |
| cobalt | Changes specific to the Cobalt browser logic |
| evergreen | For Evergreen-specific changes |
| linux | Linux-specific changes |
| media | Changes related to the media pipeline (player, demuxer, etc.) |
| net | For networking changes (e.g., QUIC, sockets) |
| posix | POSIX-related changes |
| starboard | Changes to the Starboard abstraction layer |

**By Type of Change**

| Tag | Description |
| - | - |
| ci | Changes to CI/CD workflows |
| cleanup | Code cleanup (e.g., removing unused code, style fixes) |
| docs 	Documentation updates |
| feat | A new feature |
| fix | A bug fix |
| refactor | Code refactoring without changing functionality |
| revert | Reverting a previous commit |
| test | For changes to tests (e.g., nplb, unit tests) |

##### Bug references

Each commit message needs to reference at least one bug number.

Reference Buganizer issues in [trailers](https://git-scm.com/docs/git-interpret-trailers) like `Issue: 123456789` or `Bug: 123456789` or `Fixed: 123456789`, any of which should cause the issue to be updated when the associated pull request is merged, and the last of which also causes the issue to be closed as [Fixed (Google-internal link)](http://go/buganizer/concepts/issues#status).

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
