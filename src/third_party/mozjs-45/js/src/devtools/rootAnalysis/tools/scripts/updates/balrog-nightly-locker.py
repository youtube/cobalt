#!/usr/bin/env python
import os
from os import path
import logging
import site
import sys

site.addsitedir(path.join(path.dirname(__file__), "../../lib/python"))
# Use explicit version of python-requests
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../lib/python/vendor/requests-2.7.0"))

from balrog.submitter.api import Rule, Release


if __name__ == '__main__':
    from argparse import ArgumentParser, REMAINDER
    parser = ArgumentParser()

    parser.add_argument("-a", "--api-root", dest="api_root", required=True,
        help="The root of the Balrog API. Eg: https://aus4-admin-dev.allizom.org/api")
    parser.add_argument("-c", "--credentials-file", dest="credentials_file", required=True,
        help="""The file containing the credentials to use when authenticating to Balrog.
             It must containing a dict called "balrog_credentials" which must have a key
             matching the --username passed.""")
    parser.add_argument("-u", "--username", dest="username", required=True,
        help="The username to use when authenticating to Balrog.")
    parser.add_argument("-v", "--verbose", dest="verbose", action="store_true", default=False)
    parser.add_argument("-r", "--rule-id", dest="rule_ids", action="append", required=True,
        help="""A rule id to perform an action on. This must be given at least once, but
             can be passed multiple times to update multiple rules at the same time.""")
    parser.add_argument("-n", "--dry-run", dest="dry_run", action="store_true", default=False,
        help="""When set, the script will print out what would have been done. No rules
             will be updated.""")
    parser.add_argument("action", nargs=1, choices=["lock", "unlock"],
        help="""The action to perform on the given rules.
             When "lock" is requested, this script will change the mapping for the given
             rules to match what they are currently pointing at. If no other arguments
             are given, this is accomplished by looking at the current mapping for each
             rule, finding the most recent buildid referenced in it, and constructing
             that release's name based on the current mapping. If you want to lock rules
             to a specific release, you may pass that as the next argument.

             When "unlock" is requested, this script will change the mapping for the
             given rules to point at their "latest" blob. This is accomplished by
             stripping the current mapping of its last section, and appending "-latest".
             For example: Firefox-mozilla-central-nightly-201410120103 becomes
             Firefox-mozilla-central-nightly-latest."""
        )
    parser.add_argument("action_args", nargs=REMAINDER,
        help="""See help for "action". Only valid when "action" is "lock".""")

    args = parser.parse_args()

    logging_level = logging.INFO
    if args.verbose:
        logging_level = logging.DEBUG
    logging.basicConfig(stream=sys.stdout, level=logging_level,
                        format="%(message)s")

    credentials = {}
    execfile(args.credentials_file, credentials)
    auth = (args.username, credentials['balrog_credentials'][args.username])

    if args.action == ["lock"]:
        my_args = args.action_args
        if len(my_args) > 1:
            parser.error("Unsupported number of args for the lock command.")

        for rule_id in args.rule_ids:
            release_name = None
            if len(my_args) == 1:
                release_name = args[0]

            if not release_name:
                rule_data, _ = Rule(api_root=args.api_root, auth=auth,
                                    rule_id=rule_id).get_data()
                latest_blob_name = rule_data["mapping"]
                release = Release(api_root=args.api_root, auth=auth,
                                  name=latest_blob_name)
                release_data, _ = release.get_data()
                buildid = None
                for p in release_data["platforms"].values():
                    enUS = p.get("locales", {}).get("en-US", {})
                    next_buildid = enUS.get("buildID")
                    if next_buildid > buildid:
                        buildid = next_buildid
                if not buildid:
                    logging.error("Couldn't find latest buildid for rule %s", rule_id)
                    sys.exit(1)

                root_name = "-".join(latest_blob_name.split("-")[:-1])
                release_name = "%s-%s" % (root_name, buildid)

            if not args.dry_run:
                logging.info("Locking rule %s to %s", rule_id, release_name)
                Rule(api_root=args.api_root, auth=auth, rule_id=rule_id
                     ).update_rule(mapping=release_name)
            else:
                logging.info("Would've locked rule %s to %s", rule_id, release_name)

    elif args.action == ["unlock"]:
        if args.action_args:
            parser.error("Unlock command does not accept any args.")

        for rule_id in args.rule_ids:
            rule = Rule(api_root=args.api_root, auth=auth, rule_id=rule_id)
            rule_data, _ = rule.get_data()
            root_name = "-".join(rule_data["mapping"].split("-")[:-1])
            release_name = "%s-latest" % root_name

            if not args.dry_run:
                logging.info("Unlocking rule %s back to %s", rule_id, release_name)
                rule.update_rule(mapping=release_name)
            else:
                logging.info("Would've unlocked rule %s back to %s", rule_id, release_name)
