# This file requires the clang build system, at least for now. So to use this
# Makefile, you should execute the following commands to copy this directory
# into a clang checkout:
#
#   cp -R <this directory> /path/to/clang/examples/chromeplugin
#   cd /path/to/clang/examples/chromeplugin
#   make
#
# You can't just link the directories, or specify clang as a full path for some
# reason. :(
#
# TODO(thakis): Improve the situation.

CLANG_LEVEL := ../..
LIBRARYNAME = FindBadConstructs

LINK_LIBS_IN_SHARED = 1
SHARED_LIBRARY = 1

include $(CLANG_LEVEL)/Makefile
