#!/usr/bin/python
"""%prog [-p|--props-file] [-r|--rev revision] [-b|--branch branch]
         [-s|--shared-dir shared_dir] repo [dest]

Tool to do safe operations with git.

revision/branch on commandline will override those in props-file"""

# Import snippet to find tools lib
import os
import site
import logging
site.addsitedir(os.path.join(os.path.dirname(os.path.realpath(__file__)),
                             "../../lib/python"))

try:
    import simplejson as json
    assert json
except ImportError:
    import json

from util.git import git


if __name__ == '__main__':
    from optparse import OptionParser

    parser = OptionParser(__doc__)
    parser.set_defaults(
        revision=os.environ.get('GIT_REV'),
        branch=os.environ.get('GIT_BRANCH', None),
        propsfile=os.environ.get('PROPERTIES_FILE'),
        loglevel=logging.INFO,
        shared_dir=os.environ.get('GIT_SHARE_BASE_DIR'),
        mirrors=None,
        clean=False,
    )
    parser.add_option(
        "-r", "--rev", dest="revision", help="which revision to update to")
    parser.add_option(
        "-b", "--branch", dest="branch", help="which branch to update to")
    parser.add_option("-p", "--props-file", dest="propsfile",
                      help="build json file containing revision information")
    parser.add_option("-s", "--shared-dir", dest="shared_dir",
                      help="clone to a shared directory")
    parser.add_option("--mirror", dest="mirrors", action="append",
                      help="add a mirror to try cloning/pulling from before repo")
    parser.add_option("--clean", dest="clean", action="store_true", default=False,
                      help="run 'git clean' after updating the local repository")
    parser.add_option("-v", "--verbose", dest="loglevel",
                      action="store_const", const=logging.DEBUG)

    options, args = parser.parse_args()

    logging.basicConfig(
        level=options.loglevel, format="%(asctime)s %(message)s")

    if len(args) not in (1, 2):
        parser.error("Invalid number of arguments")

    repo = args[0]
    if len(args) == 2:
        dest = args[1]
    else:
        dest = os.path.basename(repo)

    # Parse propsfile
    if options.propsfile:
        js = json.load(open(options.propsfile))
        if options.revision is None:
            options.revision = js['sourcestamp']['revision']
        if options.branch is None:
            options.branch = js['sourcestamp']['branch']

    got_revision = git(repo, dest, options.branch, options.revision,
                       shareBase=options.shared_dir,
                       mirrors=options.mirrors,
                       clean_dest=options.clean,
                       )

    print "Got revision %s" % got_revision
