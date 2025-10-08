This folder contains the sources for the
https://get.perfetto.dev/XXX web service.

This service simply proxies request of the form

https://get.perfetto.dev/XXX

<<<<<<< HEAD
to the raw content of the files hosted on Github, e.g.:

https://github.com/google/perfetto/tree/main/XXX
=======
to the raw content of the files hosted on AOSP Gerrit, e.g.:

https://android.googlesource.com/platform/external/perfetto/+/master/XXX
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

See main.py for the dictionary that maps /XXX into the actual path in the
perfetto repository.
