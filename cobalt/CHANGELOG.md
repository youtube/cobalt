# Cobalt Version Changelog

This document records all notable changes made to Cobalt since the last release.

## Version 10

### Initially clear output to transparent, not pink

Previously, Cobalt would clear the screen to pink on the first three frames
rendered in order to ensure that no data from a previous framebuffer could
appear in the output.  Unfortunately, this was found to be problematic in
cases where Cobalt was used to host an overlay that was designed to be fully
transparent in some areas.  Thus, the initial screen clears are now made with
the color RGBA(0, 0, 0, 0), and you will no longer see that pink color.
