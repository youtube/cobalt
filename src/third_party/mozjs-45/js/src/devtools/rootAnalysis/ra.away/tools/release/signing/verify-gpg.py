#!/usr/bin/python

import re
import os
from tempfile import mkdtemp
from subprocess import Popen, PIPE
from optparse import OptionParser
from shutil import rmtree

from signing import findfiles

supported_files_re = re.compile(r"""
                                   (?:
                                     \.gz
                                   | \.bz2
                                   | \.exe
                                   | \.zip
                                   | \.cab
                                   | \.dmg
                                   | \.bundle
                                   | \.iso
                                   | MD5SUMS
                                   | SHA1SUMS
                                   ) $
                                """, re.VERBOSE)


class GnuPG(object):

    def __init__(self, gpg_bin="gpg", homedir=None, args=None, verbose=False):
        self.gpg_bin = gpg_bin
        self.args = ['--batch', '-v']
        self.verbose = verbose
        if isinstance(args, (list, tuple)):
            self.args = self.args.extend(list(args))
        if homedir:
            self.homedir = homedir
            self.args.extend(['--homedir', self.homedir])
            self.setup_homedir()

    def setup_homedir(self):
        if not os.path.exists(self.homedir):
            os.makedirs(self.homedir)

    def import_key(self, keyfile):
        cmd = [self.gpg_bin]
        cmd.extend(self.args)
        cmd.extend(['--import', keyfile])
        p = Popen(cmd, stdout=PIPE, stderr=PIPE)
        output = p.stderr.read()
        retcode = p.wait()
        if retcode != 0:
            if self.verbose:
                print output
            raise OSError("Error running gpg import")

    def verify(self, file, signature_ext=".asc"):
        signature = file + signature_ext
        if not os.path.exists(signature):
            raise OSError("Signature doesn't exist")
        else:
            cmd = [self.gpg_bin]
            cmd.extend(self.args)
            cmd.extend(['--verify', signature, file])
            p = Popen(cmd, stdout=PIPE, stderr=PIPE)
            output = p.stderr.read()
            retcode = p.wait()
            if retcode != 0:
                if self.verbose:
                    print output
                raise OSError("GPG verification FAILED")


def main():
    parser = OptionParser(usage="%prog -k keyfile dir [dir...]")
    parser.add_option("-k", "--key-file", dest="keyfile", help="GPG key path")
    parser.add_option("-x", "--exclude", dest="excludes", action="append",
                      help="exclude files")
    parser.add_option("-v", "--verbose", dest="verbose", action="store_true",
                      default=False, help="print GPG output to stdout")
    (options, args) = parser.parse_args()

    if not options.keyfile:
        parser.error("Key path is reqiured")

    if len(args) < 1:
        parser.error("At least one directory is required")

    tmphomedir = mkdtemp()
    gpg = GnuPG(homedir=tmphomedir, verbose=options.verbose)
    gpg.import_key(options.keyfile)

    errors = False
    for dir in args:
        files = findfiles(dir)
        files = filter(supported_files_re.search, files)
        if options.excludes:
            for exclude in options.excludes:
                exclude_re = re.compile(exclude)
                files = filter(lambda x: not exclude_re.search(x), files)
        for f in sorted(files):
            try:
                print "Verifying %s..." % f,
                gpg.verify(f)
                print "OK"
            except OSError, e:
                errors = True
                print e

    rmtree(tmphomedir, ignore_errors=True)

    if errors:
        raise SystemExit("FAILED")

if __name__ == '__main__':
    main()
