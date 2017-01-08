This directory provides an API for clients that build Cobalt into a lib using
the 'cobalt_enable_lib' flag.

It is highly experimental at this point and is expected to change significantly
across releases.

As a general convention, functions whose symbols are exported are defined in an
'exported' subdirectory and functions that are required to be implemented by
clients are defined in a 'imported' subdirectory.
