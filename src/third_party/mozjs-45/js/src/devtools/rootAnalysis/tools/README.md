# Tools [![Build Status](https://travis-ci.org/mozilla/build-tools.png)](https://travis-ci.org/mozilla/build-tools)

This repository contains tools used by Mozilla Release Engineering. This repository
is checked out on Buildbot masters and Buildbot slaves.

This repository is a downstream read-only copy of:
https://hg.mozilla.org/build/tools/

To submit a patch, please create a bug on http://bugzilla.mozilla.org/ under
Product: Release Engineering, Component: Tools.

To run tests:
```
pip install tox
tox
```

Please also see:
* https://github.com/mozilla/build-buildbotcustom
* https://github.com/mozilla/build-buildbot-configs
