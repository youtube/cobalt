# Nanobase

Nanobase is essentially a minified/lightweight version of Chromium's base
library, though it does include utility classes that Chromium's base does not.

Its usefulness is found in the fact that its only dependency is starboard,
making it very portable and easy to include in projects that don't want
to depend on Chromium's base (which currently would require the Chromium
repository to be available and that may not be desirable).

An example project that depends on nb is glimp, which was the project that
initiated the creation of nb.  We wanted glimp to be portable alongside
Starboard, and so in order to enable that, we wanted to avoid a dependency
on Chromium's base.
