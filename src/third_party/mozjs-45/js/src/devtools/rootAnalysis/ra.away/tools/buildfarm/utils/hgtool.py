#!/usr/bin/env python
"""%prog [-p|--props-file] [-r|--rev revision] [-b|--branch branch]
         [-s|--shared-dir shared_dir] [--check-outgoing] repo [dest]

Tool to do safe operations with hg.

revision/branch on commandline will override those in props-file"""

# Import snippet to find tools lib
import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), "../../lib/python"))

from util.hg import mercurial, out, remove_path

if __name__ == '__main__':
    from optparse import OptionParser
    import logging

    parser = OptionParser(__doc__)
    parser.set_defaults(
        revision=os.environ.get('HG_REV'),
        branch=os.environ.get('HG_BRANCH', None),
        outgoing=False,
        propsfile=os.environ.get('PROPERTIES_FILE'),
        loglevel=logging.INFO,
        shared_dir=os.environ.get('HG_SHARE_BASE_DIR'),
        clone_by_rev=False,
        mirrors=None,
        bundles=None,
    )
    parser.add_option(
        "-v", "--verbose", dest="loglevel", action="store_const",
        const=logging.DEBUG, help="verbose logging")
    parser.add_option(
        "-r", "--rev", dest="revision", help="which revision to update to")
    parser.add_option(
        "-b", "--branch", dest="branch", help="which branch to update to")
    parser.add_option("-p", "--props-file", dest="propsfile",
                      help="build json file containing revision information")
    parser.add_option("-s", "--shared-dir", dest="shared_dir",
                      help="clone to a shared directory")
    parser.add_option("--check-outgoing", dest="outgoing", action="store_true",
                      help="check for and clobber outgoing changesets")
    parser.add_option(
        "--clone-by-revision", dest="clone_by_rev", action="store_true",
        help="do initial clone with -r <rev> instead of cloning the entire repo. "
             "This is slower but is useful when cloning repositories with many "
             "heads which may timeout otherwise.")
    parser.add_option("--mirror", dest="mirrors", action="append",
                      help="add a mirror to try cloning/pulling from before repo")
    parser.add_option("--bundle", dest="bundles", action="append",
                      help="add a bundle to try downloading/unbundling from before doing a full clone")
    parser.add_option("--purge", dest="auto_purge", action="store_true",
                      help="Purge the destination directory (if it exists).")

    options, args = parser.parse_args()

    logging.basicConfig(level=options.loglevel, format="%(message)s")

    if len(args) not in (1, 2):
        parser.error("Invalid number of arguments")

    repo = args[0]
    if len(args) == 2:
        dest = args[1]
    else:
        dest = os.path.basename(repo)

    # Parse propsfile
    if options.propsfile:
        try:
            import json
        except ImportError:
            import simplejson as json
        js = json.load(open(options.propsfile))
        if options.revision is None:
            options.revision = js['sourcestamp']['revision']
        if options.branch is None:
            options.branch = js['sourcestamp']['branch']

    # look for and clobber outgoing changesets
    if options.outgoing:
        if out(dest, repo):
            remove_path(dest)
        if options.shared_dir and out(options.shared_dir, repo):
            remove_path(options.shared_dir)

    got_revision = mercurial(repo, dest, options.branch, options.revision,
                             shareBase=options.shared_dir,
                             clone_by_rev=options.clone_by_rev,
                             mirrors=options.mirrors,
                             bundles=options.bundles,
                             autoPurge=options.auto_purge)

    print "Got revision %s" % got_revision
