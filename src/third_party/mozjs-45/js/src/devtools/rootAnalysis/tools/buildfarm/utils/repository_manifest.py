#! /usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Script name:   repository_manifest.py
# Author(s):     Zambrano Gasparnian, Armen <armenzg@mozilla.com>
# Target:        Python 2.7.x
#
"""
   Reads a repository manifest and outputs the repo and
   revision/branch in a format digestable for buildbot
   properties ("key: value").
"""
import json
import logging
import random
import time
import urllib2

from optparse import OptionParser
from ssl import SSLError

SUCCESS_CODE = 0
# This is not an infra error and we can't recover from it
FAILURE_CODE = 1
# When an infra error happens we want to turn purple and
# let sheriffs determine if re-triggering is needed
INFRA_CODE = 3

EXIT_CODE = FAILURE_CODE

logging.basicConfig(format='%(asctime)s %(message)s')
log = logging.getLogger(__name__)

# This has been copied from lib.python.util.retry
def retrier(attempts=5, sleeptime=10, max_sleeptime=300, sleepscale=1.5,
            jitter=1):
    ''' It helps us retry '''
    for _ in range(attempts):
        log.debug("attempt %i/%i", _ + 1, attempts)
        yield
        if jitter:
            sleeptime += random.randint(-jitter, jitter)
        if _ == attempts - 1:
            # Don't need to sleep the last time
            break
        log.debug("sleeping for %.2fs (attempt %i/%i)",
                  sleeptime, _ + 1, attempts)
        time.sleep(sleeptime)
        sleeptime *= sleepscale
        if sleeptime > max_sleeptime:
            sleeptime = max_sleeptime

def options_args():
    '''
    Validate options and args and return them.
    '''
    parser = OptionParser(__doc__)
    parser.add_option("--manifest-url", dest="manifest_url",
                      help="URL to json file which specifies 'repo' and "
                      "'revision' values.")
    parser.add_option("--default-repo", dest="default_repo",
                      help="If manifest_url is a 404 this is the repository we "
                      "default to.")
    parser.add_option("--default-revision", dest="default_revision",
                      help="If manifest_url is a 404 this is the revision we "
                      "default to.")
    parser.add_option("--default-checkout", dest="default_checkout",
                      help="If manifest_url's repo is the same as default_repo "
                      "then use the default_checkout, otherwise, use checkout."
                      "This is an optimization for when we have the ability "
                      "of using an hg-shared checkout.")
    parser.add_option("--checkout", dest="checkout",
                      help="If default_repo and the repo in manifest_url are "
                      "distinct we use a different checkout. In other words, "
                      "we don't want to use an hg-shared checkout since we're "
                      "trying to checkout a complete different repository")
    parser.add_option("--timeout", dest="timeout", type="float", default=30,
                      help="Used to specify how long to wait until timing out "
                      "for network requests.")
    parser.add_option("--max-retries", dest="max_retries", type="int",
                      default=10,
                      help="A maximum number of retries for network requests.")
    parser.add_option("--sleeptime", dest="sleeptime", type="int", default=10,
                      help="How long to sleep in between network requests.")
    parser.add_option("--debug", dest="debug", action="store_true",
                      default=False, help="Enable debug logging.")

    options, args = parser.parse_args()

    if not options.manifest_url or \
       not options.default_repo or \
       not options.default_revision:
        parser.error("You have to call the script with all required options.")

    if options.default_checkout and options.checkout \
       and options.default_checkout == options.checkout:
        parser.error("If you use --default-checkout and --checkout,"
                     "you can't have them be the same value.")

    if options.debug:
        log.setLevel(logging.DEBUG)
        log.info("Setting DEBUG logging.")

    return options, args

def fetch_manifest(url, options):
    '''
    This function simply fetches a manifest from a URL.
    We return the manifest (None if unsuccessful) and the EXIT_CODE.
    '''
    global EXIT_CODE

    manifest = None

    log.debug("Fetching %s", url)

    try:
        num = 0
        for _ in retrier(attempts=options.max_retries,
                         sleeptime=options.sleeptime,
                         max_sleeptime=options.max_retries * options.sleeptime):
            log.debug("Attempt number %d out of %d", num+1, options.max_retries)
            try:
                url_opener = urllib2.urlopen(url, timeout=options.timeout)
                http_code = url_opener.getcode()
                if http_code == 200:
                    log.debug("Attempting to load manifest...")
                    try:
                        manifest = json.load(url_opener)
                        log.debug("Contents of manifest: %s", manifest)
                    except ValueError:
                        log.exception("We have a non-valid json manifest.")
                        EXIT_CODE = FAILURE_CODE
                    break
                else:
                    log.error("We have failed to retrieve the manifest "
                                  "(http code: %s)", http_code)
                    EXIT_CODE = INFRA_CODE
            except urllib2.HTTPError, e:
                if e.getcode() == 404:
                    # Fallback to default values for branches where the manifest
                    # is not defined
                    print_values(options.default_repo,
                                 options.default_revision,
                                 options.default_checkout,
                                 options)

                    break
                else:
                    EXIT_CODE = FAILURE_CODE
            except SSLError:
                if num == options.max_retries - 1:
                    log.exception("SSLError for %s", url)
                    EXIT_CODE = INFRA_CODE
                    break
            except urllib2.URLError:
                if num == options.max_retries - 1:
                    log.exception("URLError for %s", url)
                    EXIT_CODE = INFRA_CODE
                    break

            num += 1
    except:
        log.exception("Unknown case")
        EXIT_CODE = FAILURE_CODE

    return manifest

def valid_repository_revision(url, options):
    '''
    We validate that the URL passes is existent.
    If not, we dump the exception's infromation and change the global EXIT_CODE.
    '''
    global EXIT_CODE

    log.debug("Making sure that %s is a valid URL.", url)
    num = 0
    for _ in retrier(attempts=options.max_retries,
                     sleeptime=options.sleeptime,
                     max_sleeptime=options.max_retries * options.sleeptime):
        log.debug("Attempt number %d out of %d", num+1, options.max_retries)
        try:
            # This is just to check that the repository referenced exists
            urllib2.urlopen(url, timeout=options.timeout)
            log.debug("We have been able to reach %s", url)
            return True
        except urllib2.HTTPError:
            if num == options.max_retries - 1:
                # This is a 404 case because either the repo or the
                # revision in the manifest have invalid values
                log.exception("We are trying to reach a non-existant "
                              "url: %s", url)
                EXIT_CODE = FAILURE_CODE
                return False
        except urllib2.URLError:
            if num == options.max_retries - 1:
                # The manifest has invalid data
                log.exception("URLError for %s", url)
                EXIT_CODE = FAILURE_CODE
                return False
        num += 1

def print_values(repo, revision, checkout, options):
    '''
    This function simply prints the correct values for:
        script_repo_url
        script_repo_revision
        script_repo_checkout - if applicable
    '''
    global EXIT_CODE
    log.debug("About to print the values...")

    print "script_repo_url: %s" % repo
    print "script_repo_revision: %s" % revision

    # This block allow us to take advantage of default shared
    # checkouts
    if options.default_checkout and options.checkout:
        if options.default_repo == repo:
            # In the manifest we've requested to checkout the defult
            # repo, hence, we will use the default_checkout
            # location in order to use the hg-shared checkout
            checkout = options.default_checkout
        else:
            # We are not requesting the default repository, hence,
            # we can't use the hg-shared checkout
            checkout = options.checkout

        print "script_repo_checkout: %s" % checkout

    EXIT_CODE = SUCCESS_CODE

def main():
    '''
    Determine which repository and revision mozharness.json indicates.
    If none is found we fall back to the default repository
    '''
    options, args = options_args()

    global EXIT_CODE
    EXIT_CODE = FAILURE_CODE

    url = options.manifest_url
    manifest = fetch_manifest(options.manifest_url, options)

    if manifest is not None:
        log.debug("We have a manifest %s", manifest)
        repo = manifest["repo"]
        revision = manifest["revision"]
        url = '%s/rev/%s' % (repo, revision)
        if valid_repository_revision(url, options):
            print_values(repo, revision, options.checkout, options)

    exit(EXIT_CODE)

if __name__ == '__main__':
    main()
