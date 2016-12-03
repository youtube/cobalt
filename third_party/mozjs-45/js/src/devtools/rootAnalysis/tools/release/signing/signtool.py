#!/usr/bin/python
"""signtool.py [options] file [file ...]

If no include patterns are specified, all files will be considered. -i/-x only
have effect when signing entire directories."""
from collections import defaultdict
import os
import sys
import site
# Modify our search path to find our modules
site.addsitedir(os.path.join(os.path.dirname(__file__), "../../lib/python"))

from signing.client import remote_signfile, buildValidatingOpener
from util.archives import packtar, unpacktar
from util.paths import findfiles

import logging
log = logging.getLogger(__name__)

import pefile


def is_authenticode_signed(filename):
    """Returns True if the file is signed with authenticode"""
    p = None
    try:
        p = pefile.PE(filename)
        # Look for a 'IMAGE_DIRECTORY_ENTRY_SECURITY' entry in the optinal data
        # directory
        for d in p.OPTIONAL_HEADER.DATA_DIRECTORY:
            if d.name == 'IMAGE_DIRECTORY_ENTRY_SECURITY' and d.VirtualAddress != 0:
                return True
        return False
    except:
        log.exception("Problem parsing file")
        return False
    finally:
        if p:
            p.close()


def main():
    allowed_formats = ("sha2signcode", "signcode", "osslsigncode", "gpg", "mar", "dmg",
                       "dmgv2", "jar", "b2gmar", "emevoucher")

    from optparse import OptionParser
    import random
    parser = OptionParser(__doc__)
    parser.set_defaults(
        hosts=[],
        cert=None,
        log_level=logging.INFO,
        output_dir=None,
        output_file=None,
        formats=[],
        includes=[],
        excludes=[],
        nsscmd=None,
        tokenfile=None,
        noncefile=None,
        cachedir=None,
    )

    parser.add_option(
        "-H", "--host", dest="hosts", action="append", help="format[:format]:hostname[:port]")
    parser.add_option("-c", "--server-cert", dest="cert")
    parser.add_option("-t", "--token-file", dest="tokenfile",
                      help="file where token is stored")
    parser.add_option("-n", "--nonce-file", dest="noncefile",
                      help="file where nonce is stored")
    parser.add_option("-d", "--output-dir", dest="output_dir",
                      help="output directory; if not set then files are replaced with signed copies")
    parser.add_option("-o", "--output-file", dest="output_file",
                      help="output file; if not set then files are replaced with signed copies. This can only be used when signing a single file")
    parser.add_option("-f", "--formats", dest="formats", action="append",
                      help="signing formats (one or more of %s)" % ", ".join(allowed_formats))
    parser.add_option("-q", "--quiet", dest="log_level", action="store_const",
                      const=logging.WARN)
    parser.add_option(
        "-v", "--verbose", dest="log_level", action="store_const",
        const=logging.DEBUG)
    parser.add_option("-i", "--include", dest="includes", action="append",
                      help="add to include patterns")
    parser.add_option("-x", "--exclude", dest="excludes", action="append",
                      help="add to exclude patterns")
    parser.add_option("--nsscmd", dest="nsscmd",
                      help="command to re-sign nss libraries, if required")
    parser.add_option("--cachedir", dest="cachedir",
                      help="local cache directory")
    # TODO: Concurrency?
    # TODO: Different certs per server?

    options, args = parser.parse_args()

    logging.basicConfig(
        level=options.log_level, format="%(asctime)s - %(message)s")

    if not options.hosts:
        parser.error("at least one host is required")

    if not options.cert:
        parser.error("certificate is required")

    if not os.path.exists(options.cert):
        parser.error("certificate not found")

    if not options.tokenfile:
        parser.error("token file is required")

    if not options.noncefile:
        parser.error("nonce file is required")

    # Covert nsscmd to win32 path if required
    if sys.platform == 'win32' and options.nsscmd:
        nsscmd = options.nsscmd.strip()
        if nsscmd.startswith("/"):
            drive = nsscmd[1]
            options.nsscmd = "%s:%s" % (drive, nsscmd[2:])

    # Handle format
    formats = []
    for fmt in options.formats:
        if "," in fmt:
            for fmt in fmt.split(","):
                if fmt not in allowed_formats:
                    parser.error("invalid format: %s" % fmt)
                formats.append(fmt)
        elif fmt not in allowed_formats:
            parser.error("invalid format: %s" % fmt)
        else:
            formats.append(fmt)

    # bug 1164456
    # GPG signing must happen last because it will be invalid if done prior to
    # any format that modifies the file in-place.
    if "gpg" in formats:
        formats.remove("gpg")
        formats.append("gpg")

    if options.output_file and (len(args) > 1 or os.path.isdir(args[0])):
        parser.error(
            "-o / --output-file can only be used when signing a single file")

    if options.output_dir:
        if os.path.exists(options.output_dir):
            if not os.path.isdir(options.output_dir):
                parser.error(
                    "output_dir (%s) must be a directory", options.output_dir)
        else:
            os.makedirs(options.output_dir)

    if not options.includes:
        # Do everything!
        options.includes.append("*")

    if not formats:
        parser.error("no formats specified")

    format_urls = defaultdict(list)
    for h in options.hosts:
        # The last two parts of a host is the actual hostname:port. Any parts
        # before that are formats - there could be 0..n formats so this is
        # tricky to split.
        parts = h.split(":")
        h = parts[-2:]
        fmts = parts[:-2]
        # If no formats are specified, the host is assumed to support all of them.
        if not fmts:
            fmts = formats

        for f in fmts:
            format_urls[f].append("https://%s" % ":".join(h))

    missing_fmt_hosts = set(formats) - set(format_urls.keys())
    if missing_fmt_hosts:
        parser.error("no hosts capable of signing formats: %s" % " ".join(missing_fmt_hosts))

    log.debug("in %s", os.getcwd())

    buildValidatingOpener(options.cert)
    token = open(options.tokenfile, 'rb').read()

    for fmt in formats:
        urls = format_urls[fmt]
        random.shuffle(urls)

        # The only difference between dmg and dmgv2 are the servers they use.
        # The server side code only understands "dmg" as a format, so we need
        # to translate this now that we've chosen our URLs
        if fmt == "dmgv2":
            fmt = "dmg"

        log.debug("doing %s signing", fmt)
        log.debug("possible hosts are %s" % urls)
        files = []
        # We want to package the ".app" file in a tar for mac signing.
        if fmt == "dmg":
            for fd in args:
                packtar(fd + '.tar.gz', [fd], os.getcwd())
                files.append(fd + '.tar.gz')
        # For other platforms we sign all of the files individually.
        else:
            files = findfiles(args, options.includes, options.excludes)

        for f in files:
            log.debug("%s", f)
            log.debug("checking %s for signature...", f)
            if fmt in ('sha2signcode', 'signcode', 'osslsigncode') and is_authenticode_signed(f):
                log.info("Skipping %s because it looks like it's already signed", f)
                continue
            if options.output_dir:
                dest = os.path.join(options.output_dir, os.path.basename(f))
            else:
                dest = None

            if not remote_signfile(options, urls, f, fmt, token, dest):
                log.error("Failed to sign %s with %s", f, fmt)
                sys.exit(1)

        if fmt == "dmg":
            for fd in args:
                log.debug("unpacking %s", fd)
                unpacktar(fd + '.tar.gz', os.getcwd())
                os.unlink(fd + '.tar.gz')


if __name__ == '__main__':
    main()
