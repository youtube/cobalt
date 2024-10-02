"""Add things to old Pythons so I can pretend they are newer."""

# This file does lots of tricky stuff, so disable a bunch of lintisms.
# pylint: disable=F0401,W0611,W0622
# F0401: Unable to import blah
# W0611: Unused import blah
# W0622: Redefining built-in blah

import os, re, sys

# Python 2.3 doesn't have `set`
try:
    set = set       # new in 2.4
except NameError:
    from sets import Set as set

# Python 2.3 doesn't have `sorted`.
try:
    sorted = sorted
except NameError:
    def sorted(iterable):
        """A 2.3-compatible implementation of `sorted`."""
        lst = list(iterable)
        lst.sort()
        return lst

# Python 2.3 doesn't have `reversed`.
try:
    reversed = reversed
except NameError:
    def reversed(iterable):
        """A 2.3-compatible implementation of `reversed`."""
        lst = list(iterable)
        return lst[::-1]

# rpartition is new in 2.5
try:
    "".rpartition
except AttributeError:
    def rpartition(s, sep):
        """Implement s.rpartition(sep) for old Pythons."""
        i = s.rfind(sep)
        if i == -1:
            return ('', '', s)
        else:
            return (s[:i], sep, s[i+len(sep):])
else:
    def rpartition(s, sep):
        """A common interface for new Pythons."""
        return s.rpartition(sep)

# Pythons 2 and 3 differ on where to get StringIO
try:
    from cStringIO import StringIO
    BytesIO = StringIO
except ImportError:
    from io import StringIO, BytesIO

# What's a string called?
try:
    string_class = basestring
except NameError:
    string_class = str

# Where do pickles come from?
try:
    import cPickle as pickle
except ImportError:
    import pickle

# range or xrange?
try:
    range = xrange
except NameError:
    range = range

# A function to iterate listlessly over a dict's items.
try:
    {}.iteritems
except AttributeError:
    def iitems(d):
        """Produce the items from dict `d`."""
        return d.items()
else:
    def iitems(d):
        """Produce the items from dict `d`."""
        return d.iteritems()

# Exec is a statement in Py2, a function in Py3
if sys.version_info >= (3, 0):
    def exec_code_object(code, global_map):
        """A wrapper around exec()."""
        exec(code, global_map)
else:
    # OK, this is pretty gross.  In Py2, exec was a statement, but that will
    # be a syntax error if we try to put it in a Py3 file, even if it is never
    # executed.  So hide it inside an evaluated string literal instead.
    eval(
        compile(
            "def exec_code_object(code, global_map):\n"
            "    exec code in global_map\n",
            "<exec_function>", "exec"
            )
        )

# Reading Python source and interpreting the coding comment is a big deal.
if sys.version_info >= (3, 0):
    # Python 3.2 provides `tokenize.open`, the best way to open source files.
    import tokenize
    try:
        open_source = tokenize.open     # pylint: disable=E1101
    except AttributeError:
        from io import TextIOWrapper
        detect_encoding = tokenize.detect_encoding  # pylint: disable=E1101
        # Copied from the 3.2 stdlib:
        def open_source(fname):
            """Open a file in read only mode using the encoding detected by
            detect_encoding().
            """
            buffer = open(fname, 'rb')
            encoding, _ = detect_encoding(buffer.readline)
            buffer.seek(0)
            text = TextIOWrapper(buffer, encoding, line_buffering=True)
            text.mode = 'r'
            return text
else:
    def open_source(fname):
        """Open a source file the best way."""
        return open(fname, "rU")


# Python 3.x is picky about bytes and strings, so provide methods to
# get them right, and make them no-ops in 2.x
if sys.version_info >= (3, 0):
    def to_bytes(s):
        """Convert string `s` to bytes."""
        return s.encode('utf8')

    def to_string(b):
        """Convert bytes `b` to a string."""
        return b.decode('utf8')

    def binary_bytes(byte_values):
        """Produce a byte string with the ints from `byte_values`."""
        return bytes(byte_values)

    def byte_to_int(byte_value):
        """Turn an element of a bytes object into an int."""
        return byte_value

    def bytes_to_ints(bytes_value):
        """Turn a bytes object into a sequence of ints."""
        # In Py3, iterating bytes gives ints.
        return bytes_value

else:
    def to_bytes(s):
        """Convert string `s` to bytes (no-op in 2.x)."""
        return s

    def to_string(b):
        """Convert bytes `b` to a string (no-op in 2.x)."""
        return b

    def binary_bytes(byte_values):
        """Produce a byte string with the ints from `byte_values`."""
        return "".join([chr(b) for b in byte_values])

    def byte_to_int(byte_value):
        """Turn an element of a bytes object into an int."""
        return ord(byte_value)

    def bytes_to_ints(bytes_value):
        """Turn a bytes object into a sequence of ints."""
        for byte in bytes_value:
            yield ord(byte)

# Md5 is available in different places.
try:
    import hashlib
    md5 = hashlib.md5
except ImportError:
    import md5
    md5 = md5.new
