#!/usr/bin/env python

import json
import os
import logging
import sys


# Use explicit version of python-requests
sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                "../../lib/python/vendor/requests-2.7.0"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../lib/python"))

from balrog.submitter.cli import BlobTweaker

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option("-j", "--json", dest="blob_data")
    parser.add_option("-b", "--blobname", dest="blob_name")
    parser.add_option("-a", "--api-root", dest="api_root")
    parser.add_option("-c", "--credentials-file", dest="credentials_file")
    parser.add_option("-u", "--username", dest="username", default="ffxbld")
    parser.add_option("-v", "--verbose", dest="verbose", action="store_true")
    options, args = parser.parse_args()

    logging_level = logging.INFO
    if options.verbose:
        logging_level = logging.DEBUG
    logging.basicConfig(stream=sys.stdout, level=logging_level,
                        format="%(message)s")

    for opt in ('blob_data', 'blob_name', 'api_root', 'credentials_file', 'username'):
        if not getattr(options, opt):
            print >>sys.stderr, "Required option %s not present" % opt
            sys.exit(1)

    credentials = {}
    execfile(options.credentials_file, credentials)
    auth = (options.username, credentials['balrog_credentials'][options.username])

    fp = open(options.blob_data)
    blob_data = json.load(fp)
    fp.close()

    submitter = BlobTweaker(options.api_root, auth)
    submitter.run(options.blob_name, blob_data)
