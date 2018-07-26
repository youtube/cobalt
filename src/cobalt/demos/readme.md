# Cobalt HTML Demos

## How to build

`ninja cobalt_with_demos`

## How to run

These html pages can be executed by running cobalt and pointing the `--url`
parameter to the file in question. For example:

`out/.../cobalt.exe --url=file:///cobalt/demos/transparent-animated-webp-demo/index.html`

Note that `file:///cobalt/demos` maps to the `src/out/<PLATFORM>/content/data/test/cobalt/demos`
directory, whose contents are copied from the `src/cobalt/demos/content` source directory when
`ninja cobalt_with_demos` is run

## HTML test runner

To assist in running the html tests the interactive `html_test_runner.py` in this directory.
