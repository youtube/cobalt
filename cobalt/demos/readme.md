# Cobalt HTML Demos

## How to build

`ninja cobalt_with_demos`

## How to run

These html pages can be executed by running cobalt and pointing the `--url`
parameter to the file in question. For example:

`out/.../cobalt --url=file:///demos/transparent-animated-webp-demo/index.html`

Note that `file:///demos` maps to the `src/out/<PLATFORM>/content/test/demos`
directory, whose contents are copied from the `src/cobalt/demos/content` source
directory when `ninja cobalt_with_demos` is run.
