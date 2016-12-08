#!/usr/bin/env python
"""%prog [options] -x|-t|-c marfile [files]

Utility for managing mar files"""

### The canonical location for this file is
###   https://hg.mozilla.org/build/tools/file/default/buildfarm/utils/mar.py
###
### Please update the copy in puppet to deploy new changes to
### stage.mozilla.org, see
# https://wiki.mozilla.org/ReleaseEngineering/How_To/Modify_scripts_on_stage

import struct
import os
import bz2
import hashlib
import tempfile
from subprocess import Popen, PIPE


def rsa_sign(digest, keyfile):
    proc = Popen(['openssl', 'pkeyutl', '-sign', '-inkey', keyfile],
                 stdin=PIPE, stdout=PIPE)
    proc.stdin.write(digest)
    proc.stdin.close()
    sig = proc.stdout.read()
    return sig


def rsa_verify(digest, signature, keyfile):
    tmp = tempfile.NamedTemporaryFile()
    tmp.write(signature)
    tmp.flush()
    proc = Popen(['openssl', 'pkeyutl', '-pubin', '-verify', '-sigfile',
                 tmp.name, '-inkey', keyfile], stdin=PIPE, stdout=PIPE)
    proc.stdin.write(digest)
    proc.stdin.close()
    data = proc.stdout.read()
    return "Signature Verified Successfully" in data


def packint(i):
    return struct.pack(">L", i)


def unpackint(s):
    return struct.unpack(">L", s)[0]


def generate_signature(fp, updatefunc):
    fp.seek(0)
    # Magic
    updatefunc(fp.read(4))
    # index_offset
    updatefunc(fp.read(4))
    # file size
    updatefunc(fp.read(8))
    # number of signatures
    num_sigs = fp.read(4)
    updatefunc(num_sigs)
    num_sigs = unpackint(num_sigs)
    for i in range(num_sigs):
        # signature algo
        updatefunc(fp.read(4))

        # signature size
        sigsize = fp.read(4)
        updatefunc(sigsize)
        sigsize = unpackint(sigsize)

        # Read this, but don't update the signature with it
        fp.read(sigsize)

    # Read the rest of the file
    while True:
        block = fp.read(512 * 1024)
        if not block:
            break
        updatefunc(block)


class MarSignature:
    """Represents a signature"""
    size = None
    sigsize = None
    algo_id = None
    signature = None
    _offset = None  # where in the file this signature is located
    keyfile = None  # what key to use

    @classmethod
    def from_fileobj(cls, fp):
        _offset = fp.tell()
        algo_id = unpackint(fp.read(4))
        self = cls(algo_id)
        self._offset = _offset
        sigsize = unpackint(fp.read(4))
        assert sigsize == self.sigsize
        self.signature = fp.read(self.sigsize)
        return self

    def __init__(self, algo_id, keyfile=None):
        self.algo_id = algo_id
        self.keyfile = keyfile
        if self.algo_id == 1:
            self.sigsize = 256
            self.size = self.sigsize + 4 + 4
            self._hsh = hashlib.new('sha1')
        else:
            raise ValueError("Unsupported signature algorithm: %s" % algo_id)

    def update(self, data):
        self._hsh.update(data)

    def verify_signature(self):
        if self.algo_id == 1:
            print "digest is", self._hsh.hexdigest()
            assert self.keyfile
            return rsa_verify(self._hsh.digest(), self.signature, self.keyfile)

    def write_signature(self, fp):
        assert self.keyfile
        print "digest is", self._hsh.hexdigest()
        self.signature = rsa_sign(self._hsh.digest(), self.keyfile)
        assert len(self.signature) == self.sigsize
        fp.seek(self._offset)
        fp.write("%s%s%s" % (
            packint(self.algo_id),
            packint(self.sigsize),
            self.signature))
        print "wrote signature %s" % self.algo_id


class MarInfo:
    """Represents information about a member of a MAR file. The following
    attributes are supported:
        `size`:     the file's size
        `name`:     the file's name
        `flags`:    file permission flags
        `_offset`:  where in the MAR file this member exists
    """
    size = None
    name = None
    flags = None
    _offset = None

    # The member info is serialized as a sequence of 4-byte integers
    # representing the offset, size, and flags of the file followed by a null
    # terminated string representing the filename
    _member_fmt = ">LLL"

    @classmethod
    def from_fileobj(cls, fp):
        """Return a MarInfo object by reading open file object `fp`"""
        self = cls()
        data = fp.read(12)
        if not data:
            # EOF
            return None
        if len(data) != 12:
            raise ValueError("Malformed mar?")
        self._offset, self.size, self.flags = struct.unpack(
            cls._member_fmt, data)
        name = ""
        while True:
            c = fp.read(1)
            if c is None:
                raise ValueError("Malformed mar?")
            if c == "\x00":
                break
            name += c
        self.name = name
        return self

    def __repr__(self):
        return "<%s %o %s bytes starting at %i>" % (
            self.name, self.flags, self.size, self._offset)

    def to_bytes(self):
        return struct.pack(self._member_fmt, self._offset, self.size, self.flags) + \
            self.name + "\x00"


class MarFile:
    """Represents a MAR file on disk.

    `name`:     filename of MAR file
    `mode`:     either 'r' or 'w', depending on if you're reading or writing.
                defaults to 'r'
    """

    _longint_fmt = ">L"

    def __init__(self, name, mode="r", signature_versions=[]):
        if mode not in "rw":
            raise ValueError("Mode must be either 'r' or 'w'")

        self.name = name
        self.mode = mode
        if mode == 'w':
            self.fileobj = open(name, 'wb')
        else:
            self.fileobj = open(name, 'rb')

        self.members = []

        # Current offset of our index in the file. This gets updated as we add
        # files to the MAR. This also refers to the end of the file until we've
        # actually written the index
        self.index_offset = 8

        # Flag to indicate that we need to re-write the index
        self.rewrite_index = False

        # Signatures, if any
        self.signatures = []
        self.signature_versions = signature_versions

        if mode == "r":
            # Read the file's index
            self._read_index()
        elif mode == "w":
            # Create placeholder signatures
            if signature_versions:
                # Space for num_signatures and file size
                self.index_offset += 4 + 8

            # Write the magic and placeholder for the index
            self.fileobj.write("MAR1" + packint(self.index_offset))

            # Write placeholder for file size
            self.fileobj.write(struct.pack(">Q", 0))

            # Write num_signatures
            self.fileobj.write(packint(len(signature_versions)))

            for algo_id, keyfile in signature_versions:
                sig = MarSignature(algo_id, keyfile)
                sig._offset = self.index_offset
                self.index_offset += sig.size
                self.signatures.append(sig)
                # algoid
                self.fileobj.write(packint(algo_id))
                # numbytes
                self.fileobj.write(packint(sig.sigsize))
                # space for signature
                self.fileobj.write("\0" * sig.sigsize)

    def _read_index(self):
        fp = self.fileobj
        fp.seek(0)
        # Read the header
        header = fp.read(8)
        magic, self.index_offset = struct.unpack(">4sL", header)
        if magic != "MAR1":
            raise ValueError("Bad magic")
        fp.seek(self.index_offset)

        # Read the index_size, we don't use it though
        # We just read all the info sections from here to the end of the file
        fp.read(4)

        self.members = []

        while True:
            info = MarInfo.from_fileobj(fp)
            if not info:
                break
            self.members.append(info)

        # Sort them by where they are in the file
        self.members.sort(key=lambda info: info._offset)

        # Read any signatures
        first_offset = self.members[0]._offset
        signature_offset = 16
        if signature_offset < first_offset:
            fp.seek(signature_offset)
            num_sigs = unpackint(fp.read(4))
            for i in range(num_sigs):
                sig = MarSignature.from_fileobj(fp)
                for algo_id, keyfile in self.signature_versions:
                    if algo_id == sig.algo_id:
                        sig.keyfile = keyfile
                        break
                else:
                    print "no key specified to validate %i signature" % sig.algo_id
                self.signatures.append(sig)

    def verify_signatures(self):
        if not self.signatures:
            return

        fp = self.fileobj

        generate_signature(fp, self._update_signatures)

        for sig in self.signatures:
            if not sig.verify_signature():
                raise IOError("Verification failed")
            else:
                print "Verification OK (%s)" % sig.algo_id

    def _update_signatures(self, data):
        for sig in self.signatures:
            sig.update(data)

    def add(self, path, name=None, fileobj=None, flags=None):
        """Adds `path` to this MAR file.

        If `name` is set, the file is named with `name` in the MAR, otherwise
        use the normalized version of `path`.

        If `fileobj` is set, file data is read from it, otherwise the file
        located at `path` will be opened and read.

        If `flags` is set, it will be used as the permission flags in the MAR,
        otherwise the permissions of `path` will be used.
        """
        if self.mode != "w":
            raise ValueError("File not opened for writing")

        # If path refers to a directory, add all the files inside of it
        if os.path.isdir(path):
            self.add_dir(path)
            return

        info = MarInfo()
        if not fileobj:
            info.name = name or os.path.normpath(path)
            info.size = os.path.getsize(path)
            info.flags = flags or os.stat(path).st_mode & 0777
            info._offset = self.index_offset

            f = open(path, 'rb')
            self.fileobj.seek(self.index_offset)
            while True:
                block = f.read(512 * 1024)
                if not block:
                    break
                self.fileobj.write(block)
        else:
            assert flags
            info.name = name or path
            info.size = 0
            info.flags = flags
            info._offset = self.index_offset
            self.fileobj.seek(self.index_offset)
            while True:
                block = fileobj.read(512 * 1024)
                if not block:
                    break
                info.size += len(block)
                self.fileobj.write(block)

        # Shift our index, and mark that we have to re-write it on close
        self.index_offset += info.size
        self.rewrite_index = True
        self.members.append(info)

    def add_dir(self, path):
        """Add all of the files under `path` to the MAR file"""
        for root, dirs, files in os.walk(path):
            for f in files:
                self.add(os.path.join(root, f))

    def close(self):
        """Close the MAR file, writing out the new index if required.

        Furthur modifications to the file are not allowed."""
        if self.mode == "w" and self.rewrite_index:
            self._write_index()

        # Update file size
        self.fileobj.seek(0, 2)
        totalsize = self.fileobj.tell()
        self.fileobj.seek(8)
        # print "File size is", totalsize, repr(struct.pack(">Q", totalsize))
        self.fileobj.write(struct.pack(">Q", totalsize))

        if self.mode == "w" and self.signatures:
            self.fileobj.flush()
            fileobj = open(self.name, 'rb')
            generate_signature(fileobj, self._update_signatures)
            for sig in self.signatures:
                # print sig._offset
                sig.write_signature(self.fileobj)

        self.fileobj.close()
        self.fileobj = None

    def __del__(self):
        """Close the file when we're garbage collected"""
        if self.fileobj:
            self.close()

    def _write_index(self):
        """Writes the index of all members at the end of the file"""
        self.fileobj.seek(self.index_offset + 4)
        index_size = 0
        for m in self.members:
            member_bytes = m.to_bytes()
            index_size += len(member_bytes)

        for m in self.members:
            member_bytes = m.to_bytes()
            self.fileobj.write(member_bytes)
        self.fileobj.seek(self.index_offset)
        self.fileobj.write(packint(index_size))

        # Update the offset to the index
        self.fileobj.seek(4)
        self.fileobj.write(packint(self.index_offset))

    def extractall(self, path=".", members=None):
        """Extracts members into `path`. If members is None (the default), then
        all members are extracted."""
        if members is None:
            members = self.members
        for m in members:
            self.extract(m, path)

    def extract(self, member, path="."):
        """Extract `member` into `path` which defaults to the current
        directory."""
        dstpath = os.path.join(path, member.name)
        dirname = os.path.dirname(dstpath)
        if not os.path.exists(dirname):
            os.makedirs(dirname)

        self.fileobj.seek(member._offset)
        open(dstpath, "wb").write(self.fileobj.read(member.size))
        os.chmod(dstpath, member.flags)


class BZ2MarFile(MarFile):
    """Subclass of MarFile that compresses/decompresses members using BZ2.

    BZ2 compression is used for most update MARs."""
    def extract(self, member, path="."):
        """Extract and decompress `member` into `path` which defaults to the
        current directory."""
        dstpath = os.path.join(path, member.name)
        dirname = os.path.dirname(dstpath)
        if not os.path.exists(dirname):
            os.makedirs(dirname)

        self.fileobj.seek(member._offset)
        decomp = bz2.BZ2Decompressor()
        output = open(dstpath, "wb")
        toread = member.size
        while True:
            thisblock = min(128 * 1024, toread)
            block = self.fileobj.read(thisblock)
            if not block:
                break
            toread -= len(block)
            output.write(decomp.decompress(block))
        output.close()
        os.chmod(dstpath, member.flags)

    def add(self, path, name=None, fileobj=None, mode=None):
        """Adds `path` compressed with BZ2 to this MAR file.

        If `name` is set, the file is named with `name` in the MAR, otherwise
        use the normalized version of `path`.

        If `fileobj` is set, file data is read from it, otherwise the file
        located at `path` will be opened and read.

        If `flags` is set, it will be used as the permission flags in the MAR,
        otherwise the permissions of `path` will be used.
        """
        if self.mode != "w":
            raise ValueError("File not opened for writing")
        if os.path.isdir(path):
            self.add_dir(path)
            return
        info = MarInfo()
        info.name = name or os.path.normpath(path)
        info.size = 0
        if not fileobj:
            info.flags = os.stat(path).st_mode & 0777
        else:
            info.flags = mode
        info._offset = self.index_offset

        if not fileobj:
            f = open(path, 'rb')
        else:
            f = fileobj
        comp = bz2.BZ2Compressor(9)
        self.fileobj.seek(self.index_offset)
        while True:
            block = f.read(512 * 1024)
            if not block:
                break
            block = comp.compress(block)
            info.size += len(block)
            self.fileobj.write(block)
        block = comp.flush()
        info.size += len(block)
        self.fileobj.write(block)

        self.index_offset += info.size
        self.rewrite_index = True
        self.members.append(info)

if __name__ == "__main__":
    from optparse import OptionParser

    parser = OptionParser(__doc__)
    parser.set_defaults(
        action=None,
        bz2=False,
        chdir=None,
        keyfile=None,
        verify=False,
    )
    parser.add_option("-x", "--extract", action="store_const", const="extract",
                      dest="action", help="extract MAR")
    parser.add_option("-t", "--list", action="store_const", const="list",
                      dest="action", help="print out MAR contents")
    parser.add_option("-c", "--create", action="store_const", const="create",
                      dest="action", help="create MAR")
    parser.add_option("-j", "--bzip2", action="store_true", dest="bz2",
                      help="compress/decompress members with BZ2")
    parser.add_option("-k", "--keyfile", dest="keyfile",
                      help="sign/verify with given key")
    parser.add_option("-v", "--verify", dest="verify", action="store_true",
                      help="verify the marfile")
    parser.add_option("-C", "--chdir", dest="chdir",
                      help="chdir to this directory before creating or extracing; location of marfile isn't affected by this option.")

    options, args = parser.parse_args()

    if not options.action:
        parser.error("Must specify something to do (one of -x, -t, -c)")

    if not args:
        parser.error("You must specify at least a marfile to work with")

    marfile, files = args[0], args[1:]
    marfile = os.path.abspath(marfile)

    if options.bz2:
        mar_class = BZ2MarFile
    else:
        mar_class = MarFile

    signatures = []
    if options.keyfile:
        signatures.append((1, options.keyfile))

    # Move into the directory requested
    if options.chdir:
        os.chdir(options.chdir)

    if options.action == "extract":
        m = mar_class(marfile)
        m.extractall()

    elif options.action == "list":
        m = mar_class(marfile, signature_versions=signatures)
        if options.verify:
            m.verify_signatures()
        print "%-7s %-7s %-7s" % ("SIZE", "MODE", "NAME")
        for m in m.members:
            print "%-7i %04o    %s" % (m.size, m.flags, m.name)

    elif options.action == "create":
        if not files:
            parser.error("Must specify at least one file to add to marfile")
        m = mar_class(marfile, "w", signature_versions=signatures)
        for f in files:
            m.add(f)
        m.close()
