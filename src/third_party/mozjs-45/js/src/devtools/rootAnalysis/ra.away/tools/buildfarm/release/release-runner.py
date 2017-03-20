#!/usr/bin/env python

import site
import time
import logging
import sys
import os
from os import path
from optparse import OptionParser
from twisted.python.lockfile import FilesystemLock

site.addsitedir(path.join(path.dirname(__file__), "../../lib/python"))

import requests
from kickoff.api import Releases, Release, ReleaseL10n
from release.info import readBranchConfig
from release.l10n import parsePlainL10nChangesets
from release.versions import getAppVersion
from releasetasks import make_task_graph
from taskcluster import Scheduler, Index
from taskcluster.utils import slugId
from util.hg import mercurial
from util.retry import retry
from util.file import load_config, get_config

log = logging.getLogger(__name__)


class ReleaseRunner(object):
    def __init__(self, api_root=None, username=None, password=None,
                 timeout=60):
        self.new_releases = []
        self.releases_api = Releases((username, password), api_root=api_root,
                                     timeout=timeout)
        self.release_api = Release((username, password), api_root=api_root,
                                   timeout=timeout)
        self.release_l10n_api = ReleaseL10n((username, password),
                                            api_root=api_root, timeout=timeout)

    def get_release_requests(self):
        new_releases = self.releases_api.getReleases()
        if new_releases['releases']:
            self.new_releases = [self.release_api.getRelease(name) for name in
                                 new_releases['releases']]
            return True
        else:
            log.info("No new releases: %s" % new_releases)
            return False

    def get_release_l10n(self, release):
        return self.release_l10n_api.getL10n(release)

    def update_status(self, release, status):
        log.info('updating status for %s to %s' % (release['name'], status))
        try:
            self.release_api.update(release['name'], status=status)
        except requests.HTTPError, e:
            log.warning('Caught HTTPError: %s' % e.response.content)
            log.warning('status update failed, continuing...', exc_info=True)

    def mark_as_completed(self, release):#, enUSPlatforms):
        log.info('mark as completed %s' % release['name'])
        self.release_api.update(release['name'], complete=True,
                                status='Started')
                                #enUSPlatforms=json.dumps(enUSPlatforms))

    def mark_as_failed(self, release, why):
        log.info('mark as failed %s' % release['name'])
        self.release_api.update(release['name'], ready=False, status=why)


def getPartials(release):
    partials = {}
    for p in release['partials'].split(','):
        partialVersion, buildNumber = p.split('build')
        partials[partialVersion] = {
            'appVersion': getAppVersion(partialVersion),
            'buildNumber': buildNumber,
        }
    return partials


# TODO: actually do this. figure out how to get the right info without having a release config.
# maybe we don't need revision info any more? or maybe we have from some other source like branch config?
#def sendMailRD(smtpServer, From, cfgFile, r):
#    # Send an email to the mailing after the build
#    contentMail = ""
#    release_config = readReleaseConfig(cfgFile)
#    sources = release_config['sourceRepositories']
#    To = release_config['ImportantRecipients']
#    comment = r.get("comment")
#
#    if comment:
#        contentMail += "Comment:\n" + comment + "\n\n"
#
#    contentMail += "A new build has been submitted through ship-it:\n"
#
#    for name, source in sources.items():
#
#        if name == "comm":
#            # Thunderbird
#            revision = source["revision"]
#            path = source["path"]
#        else:
#            revision = source["revision"]
#            path = source["path"]
#
#        # For now, firefox has only one source repo but Thunderbird has two
#        contentMail += name + " commit: https://hg.mozilla.org/" + path + "/rev/" + revision + "\n"
#
#    contentMail += "\nCreated by " + r["submitter"] + "\n"
#
#    contentMail += "\nStarted by " + r["starter"] + "\n"
#
#    subjectPrefix = ""
#
#    # On r-d, we prefix the subject of the email in order to simplify filtering
#    # We don't do it for thunderbird
#    if "Fennec" in r["name"]:
#        subjectPrefix = "[mobile] "
#    if "Firefox" in r["name"]:
#        subjectPrefix = "[desktop] "
#
#    Subject = subjectPrefix + 'Build of %s' % r["name"]
#
#    sendmail(from_=From, to=To, subject=Subject, body=contentMail,
#             smtp_server=smtpServer)

# TODO: deal with platform-specific locales
def get_platform_locales(l10n_changesets, platform):
    return l10n_changesets.keys()


def get_l10n_config(release, branchConfig, branch, l10n_changesets, index):
    l10n_platforms = {}
    for platform in branchConfig["l10n_release_platforms"]:
        task = index.findTask("buildbot.revisions.{revision}.{branch}.{platform}".format(
            revision=release["mozillaRevision"],
            branch=branch,
            platform=platform,
        ))

        # TODO: Replace this with simple names
        if platform.startswith("win"):
            binary_fmt = "public/build/{product}-{version}.en-US.{platform}.installer.exe"
        elif platform.startswith("mac"):
            binary_fmt = "public/build/{product}-{version}.en-US.{platform}.dmg"
        elif platform.startswith("linux"):
            binary_fmt = "public/build/{product}-{version}.en-US.{platform}.tar.bz2"

        filename = binary_fmt.format(
            product=release["product"],
            version=release["version"],
            platform=platform
        )
        url = "https://queue.taskcluster.net/v1/task/{taskid}/artifacts/{filename}".format(
            taskid=task["taskId"],
            filename=filename
        )
        l10n_platforms[platform] = {
            "locales": get_platform_locales(l10n_changesets, platform),
            "en_us_binary_url": url,
            "chunks": branchConfig["platforms"][platform].get("l10n_chunks", 6),
        }

    return {
        "platforms": l10n_platforms,
        "changesets": l10n_changesets,
    }


def validate_graph_kwargs(**kwargs):
    # TODO: validate partials
    # TODO: validate l10n changesets
    # TODO: go through release sanity for other validations to do
    for url in kwargs.get("l10n_platforms", {}).values():
        ret = requests.head(url, allow_redirects=True)
        if not ret.ok():
            log.error("en_us_binary url (%s) not accessible (got http %s)", url, ret.status_code)


def main(options):
    log.info('Loading config from %s' % options.config)
    config = load_config(options.config)

    if config.getboolean('release-runner', 'verbose'):
        log_level = logging.DEBUG
    else:
        log_level = logging.INFO
    logging.basicConfig(format="%(asctime)s - %(levelname)s - %(message)s",
                        level=log_level)
    # Suppress logging of retry(), see bug 925321 for the details
    logging.getLogger("util.retry").setLevel(logging.WARN)

    # Shorthand
    api_root = config.get('api', 'api_root')
    username = config.get('api', 'username')
    password = config.get('api', 'password')
    buildbot_configs = config.get('release-runner', 'buildbot_configs')
    buildbot_configs_branch = config.get('release-runner',
                                         'buildbot_configs_branch')
    sleeptime = config.getint('release-runner', 'sleeptime')
    notify_from = get_config(config, 'release-runner', 'notify_from', None)
    notify_to = get_config(config, 'release-runner', 'notify_to', None)
    docker_worker_key = get_config(config, 'release-runner',
                                   'docker_worker_key', None)
    if isinstance(notify_to, basestring):
        notify_to = [x.strip() for x in notify_to.split(',')]
    smtp_server = get_config(config, 'release-runner', 'smtp_server',
                             'localhost')
    tc_config = {
        "credentials": {
            "clientId": get_config(config, "taskcluster", "client_id", None),
            "accessToken": get_config(config, "taskcluster", "access_token", None),
        }
    }
    configs_workdir = 'buildbot-configs'

    # TODO: replace release sanity with direct checks of en-US and l10n revisions (and other things if needed)

    rr = ReleaseRunner(api_root=api_root, username=username, password=password)
    scheduler = Scheduler(tc_config)
    index = Index(tc_config)

    # Main loop waits for new releases, processes them and exits.
    while True:
        try:
            log.debug('Fetching release requests')
            rr.get_release_requests()
            if rr.new_releases:
                for release in rr.new_releases:
                    log.info('Got a new release request: %s' % release)
                break
            else:
                log.debug('Sleeping for %d seconds before polling again' %
                          sleeptime)
                time.sleep(sleeptime)
        except:
            log.error("Caught exception when polling:", exc_info=True)
            sys.exit(5)

    retry(mercurial, args=(buildbot_configs, configs_workdir), kwargs=dict(branch=buildbot_configs_branch))

    if 'symlinks' in config.sections():
        format_dict = dict(buildbot_configs=configs_workdir)
        for target in config.options('symlinks'):
            symlink = config.get('symlinks', target).format(**format_dict)
            if path.exists(symlink):
                log.warning("Skipping %s -> %s symlink" % (symlink, target))
            else:
                log.info("Adding %s -> %s symlink" % (symlink, target))
                os.symlink(target, symlink)

    # TODO: this won't work for Thunderbird...do we care?
    branch = release["branch"].split("/")[-1]
    branchConfig = readBranchConfig(path.join(configs_workdir, "mozilla"), branch=branch)

    rc = 0
    for release in rr.new_releases:
        try:
            rr.update_status(release, 'Generating task graph')
            l10n_changesets = parsePlainL10nChangesets(rr.get_release_l10n(release["name"]))

            kwargs = {
                "public_key": docker_worker_key,
                "version": release["version"],
                "buildNumber": release["buildNumber"],
                "source_enabled": True,
                "repo_path": release["branch"],
                "revision": release["mozillaRevision"],
                "product": release["product"],
                "partial_updates": getPartials(release),
                "branch": branch,
                "updates_enabled": bool(release["partials"]),
                "enUS_platforms": branchConfig["release_platforms"],
                "l10n_config": get_l10n_config(release, branchConfig, branch, l10n_changesets, index),
                "balrog_api_root": branchConfig["balrog_api_root"],
                "signing_class": "dep-signing",
            }

            validate_graph_kwargs(**kwargs)

            graph_id = slugId()
            graph = make_task_graph(**kwargs)

            rr.update_status(release, "Submitting task graph")

            log.info("Task graph generated!")
            import pprint
            log.debug(pprint.pformat(graph, indent=4, width=160))
            print scheduler.createTaskGraph(graph_id, graph)

            rr.mark_as_completed(release)
        except:
            # We explicitly do not raise an error here because there's no
            # reason not to start other releases if creating the Task Graph
            # fails for another one. We _do_ need to set this in order to exit
            # with the right code, though.
            rc = 2
            rr.update_status(release, 'Failed to start release promotion')
            log.exception("Failed to start release promotion for {}: ".format(release))

    if rc != 0:
        sys.exit(rc)

if __name__ == '__main__':
    parser = OptionParser(__doc__)
    parser.add_option('-l', '--lockfile', dest='lockfile',
                      default=path.join(os.getcwd(), ".release-runner.lock"))
    parser.add_option('-c', '--config', dest='config',
                      help='Configuration file')

    options = parser.parse_args()[0]

    if not options.config:
        parser.error('Need to pass a config')

    lockfile = options.lockfile
    log.debug("Using lock file %s", lockfile)
    lock = FilesystemLock(lockfile)
    if not lock.lock():
        raise Exception("Cannot acquire lock: %s" % lockfile)
    log.debug("Lock acquired: %s", lockfile)
    if not lock.clean:
        log.warning("Previous run did not properly exit")
    try:
        main(options)
    finally:
        log.debug("Releasing lock: %s", lockfile)
        lock.unlock()
