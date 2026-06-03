# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium [![Build Matrix](https://img.shields.io/badge/-Build%20Matrix-blueviolet)](https://github.com/youtube/cobalt/blob/main/cobalt/BUILD_STATUS.md)

[![codecov](https://codecov.io/github/youtube/cobalt/branch/main/graph/badge.svg?token=RR6MKKNYNV)](https://codecov.io/github/youtube/cobalt)
[![android](https://github.com/youtube/cobalt/actions/workflows/android.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/android.yaml?query=event%3Apush+branch%3Amain)
[![linux](https://github.com/youtube/cobalt/actions/workflows/linux.yaml/badge.svg?branch=main&event=push)](https://github.com/youtube/cobalt/actions/workflows/linux.yaml?query=event%3Apush+branch%3Amain)

Chromium is an open-source browser project that aims to build a safer, faster,
and more stable way for all users to experience the web.

The project's web site is https://www.chromium.org.

To check out the source code locally, don't use `git clone`! Instead,
follow [the instructions on how to get the code](docs/get_the_code.md).

Documentation in the source is rooted in [docs/README.md](docs/README.md).

Learn how to [Get Around the Chromium Source Code Directory
Structure](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code).

For historical reasons, there are some small top level directories. Now the
guidance is that new top level directories are for product (e.g. Chrome,
Android WebView, Ash). Even if these products have multiple executables, the
code should be in subdirectories of the product.

If you found a bug, please file it at https://crbug.com/new.
