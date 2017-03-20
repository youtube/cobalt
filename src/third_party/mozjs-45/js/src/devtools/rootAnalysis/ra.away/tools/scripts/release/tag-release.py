#!/usr/bin/env python

import logging
from os import path
from traceback import format_exc
import subprocess
import sys

sys.path.append(path.join(path.dirname(__file__), "../../lib/python"))
logging.basicConfig(
    stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

from util.commands import run_cmd, get_output
from util.hg import mercurial, apply_and_push, update, get_revision, \
    make_hg_url, out, BRANCH, get_branches, cleanOutgoingRevs
from util.retry import retry
from build.versions import bumpFile
from release.info import readReleaseConfig, getTags, generateRelbranchName
from release.l10n import getL10nRepositories

HG = "hg.mozilla.org"
DEFAULT_BUILDBOT_CONFIGS_REPO = make_hg_url(HG, 'build/buildbot-configs')
DEFAULT_MAX_PUSH_ATTEMPTS = 10
REQUIRED_CONFIG = ('version', 'appVersion', 'appName', 'productName',
                   'buildNumber', 'hgUsername', 'hgSshKey',
                   'baseTag', 'l10nRepoPath', 'sourceRepositories',
                   'l10nRevisionFile')
REQUIRED_SOURCE_REPO_KEYS = ('path', 'revision')


def getBumpCommitMessage(productName, version):
    return 'Automated checkin: version bump for ' + productName + ' ' + \
           version + ' release. DONTBUILD CLOSED TREE a=release'


def getTagCommitMessage(revision, tags):
    return "Added " + " ".join(tags) + " tag(s) for changeset " + revision + \
           ". DONTBUILD CLOSED TREE a=release"


def bump(repo, bumpFiles, versionKey):
    for f, info in bumpFiles.iteritems():
        fileToBump = path.join(repo, f)
        contents = open(fileToBump).read()
        # If info[versionKey] is a function, this function will do the bump.
        # It takes the old contents as its input to generate the new content.
        if callable(info[versionKey]):
            newContents = info[versionKey](contents)
        else:
            newContents = bumpFile(f, contents, info[versionKey])
        if contents != newContents:
            fh = open(fileToBump, "w")
            fh.write(newContents)
            fh.close()


def tag(repo, revision, tags, username):
    cmd = ['hg', 'tag', '-u', username, '-r', revision,
           '-m', getTagCommitMessage(revision, tags), '-f']
    cmd.extend(tags)
    run_cmd(cmd, cwd=repo)


def tagRepo(config, repo, reponame, revision, tags, bumpFiles, relbranch,
            pushAttempts, defaultBranch='default'):
    remote = make_hg_url(HG, repo)
    mercurial(remote, reponame)

    def bump_and_tag(repo, attempt, config, relbranch, revision, tags,
                     defaultBranch):
        # set relbranchChangesets=1 because tag() generates exactly 1 commit
        relbranchChangesets = 1
        defaultBranchChangesets = 0

        if relbranch in get_branches(reponame):
            update(reponame, revision=relbranch)
        else:
            update(reponame, revision=revision)
            run_cmd(['hg', 'branch', relbranch], cwd=reponame)

        if len(bumpFiles) > 0:
            # Bump files on the relbranch, if necessary
            bump(reponame, bumpFiles, 'version')
            run_cmd(['hg', 'diff'], cwd=repo)
            try:
                get_output(['hg', 'commit', '-u', config['hgUsername'],
                            '-m', getBumpCommitMessage(config['productName'], config['version'])],
                           cwd=reponame)
                relbranchChangesets += 1
            except subprocess.CalledProcessError, e:
                # We only want to ignore exceptions caused by having nothing to
                # commit, which are OK. We still want to raise exceptions caused
                # by any other thing.
                if e.returncode != 1 or "nothing changed" not in e.output:
                    raise

        # We always want our tags pointing at the tip of the relbranch
        # so we need to grab the current revision after we've switched
        # branches and bumped versions.
        revision = get_revision(reponame)
        # Create the desired tags on the relbranch
        tag(repo, revision, tags, config['hgUsername'])

        # This is the bump of the version on the default branch
        # We do it after the other one in order to get the tip of the
        # repository back on default, thus avoiding confusion.
        if len(bumpFiles) > 0:
            update(reponame, revision=defaultBranch)
            bump(reponame, bumpFiles, 'nextVersion')
            run_cmd(['hg', 'diff'], cwd=repo)
            try:
                get_output(['hg', 'commit', '-u', config['hgUsername'],
                            '-m', getBumpCommitMessage(config['productName'], config['version'])],
                           cwd=reponame)
                defaultBranchChangesets += 1
            except subprocess.CalledProcessError, e:
                if e.returncode != 1 or "nothing changed" not in e.output:
                    raise

        # Validate that the repository is only different from the remote in
        # ways we expect.
        outgoingRevs = out(src=reponame, remote=remote,
                           ssh_username=config['hgUsername'],
                           ssh_key=config['hgSshKey'])

        if len([r for r in outgoingRevs if r[BRANCH] == "default"]) != defaultBranchChangesets:
            raise Exception(
                "Incorrect number of revisions on 'default' branch")
        if len([r for r in outgoingRevs if r[BRANCH] == relbranch]) != relbranchChangesets:
            raise Exception("Incorrect number of revisions on %s" % relbranch)
        if len(outgoingRevs) != (relbranchChangesets + defaultBranchChangesets):
            raise Exception("Wrong number of outgoing revisions")

    pushRepo = make_hg_url(HG, repo, protocol='ssh')

    def bump_and_tag_wrapper(r, n):
        bump_and_tag(r, n, config, relbranch, revision, tags, defaultBranch)

    def cleanup_wrapper():
        cleanOutgoingRevs(reponame, pushRepo, config['hgUsername'],
                          config['hgSshKey'])
    retry(apply_and_push, cleanup=cleanup_wrapper,
          args=(reponame, pushRepo, bump_and_tag_wrapper, pushAttempts),
          kwargs=dict(ssh_username=config['hgUsername'],
                      ssh_key=config['hgSshKey']))


def tagOtherRepo(config, repo, reponame, revision, pushAttempts):
    remote = make_hg_url(HG, repo)
    mercurial(remote, reponame)

    def tagRepo(repo, attempt, config, revision, tags):
        # set totalChangesets=1 because tag() generates exactly 1 commit
        totalChangesets = 1
        # update to the desired revision first, then to the tip of revision's
        # branch to avoid new head creation
        update(repo, revision=revision)
        update(repo)
        tag(repo, revision, tags, config['hgUsername'])
        outgoingRevs = retry(out, kwargs=dict(src=reponame, remote=remote,
                                              ssh_username=config[
                                                  'hgUsername'],
                                              ssh_key=config['hgSshKey']))
        if len(outgoingRevs) != totalChangesets:
            raise Exception("Wrong number of outgoing revisions")

    pushRepo = make_hg_url(HG, repo, protocol='ssh')

    def tag_wrapper(r, n):
        tagRepo(r, n, config, revision, tags)

    def cleanup_wrapper():
        cleanOutgoingRevs(reponame, pushRepo, config['hgUsername'],
                          config['hgSshKey'])
    retry(apply_and_push, cleanup=cleanup_wrapper,
          args=(reponame, pushRepo, tag_wrapper, pushAttempts),
          kwargs=dict(ssh_username=config['hgUsername'],
                      ssh_key=config['hgSshKey']))


def validate(options, args):
    err = False
    config = {}
    if not options.configfile:
        log.info("Must pass --configfile")
        sys.exit(1)
    elif not path.exists(path.join('buildbot-configs', options.configfile)):
        log.info("%s does not exist!" % options.configfile)
        sys.exit(1)

    config = readReleaseConfig(
        path.join('buildbot-configs', options.configfile))
    for key in REQUIRED_CONFIG:
        if key not in config:
            err = True
            log.info("Required item missing in config: %s" % key)

    for r in config['sourceRepositories'].values():
        for key in REQUIRED_SOURCE_REPO_KEYS:
            if key not in r:
                err = True
                log.info("Missing required key '%s' for '%s'" % (key, r))

    if 'otherReposToTag' in config:
        if not callable(getattr(config['otherReposToTag'], 'iteritems')):
            err = True
            log.info("otherReposToTag exists in config but is not a dict")
    if err:
        sys.exit(1)

    # Non-fatal warnings only after this point
    if not (options.tag_source or options.tag_l10n or options.tag_other):
        log.info("No tag directive specified, defaulting to all")
        options.tag_source = True
        options.tag_l10n = True
        options.tag_other = True

    return config

if __name__ == '__main__':
    from optparse import OptionParser
    import os

    parser = OptionParser(__doc__)
    parser.set_defaults(
        attempts=os.environ.get(
            'MAX_PUSH_ATTEMPTS', DEFAULT_MAX_PUSH_ATTEMPTS),
        buildbot_configs=os.environ.get('BUILDBOT_CONFIGS_REPO',
                                        DEFAULT_BUILDBOT_CONFIGS_REPO),
    )
    parser.add_option("-a", "--push-attempts", dest="attempts",
                      help="Number of attempts before giving up on pushing")
    parser.add_option("-c", "--configfile", dest="configfile",
                      help="The release config file to use.")
    parser.add_option("-b", "--buildbot-configs", dest="buildbot_configs",
                      help="The place to clone buildbot-configs from")
    parser.add_option("-t", "--release-tag", dest="release_tag",
                      help="Release tag to update buildbot-configs to")
    parser.add_option("--tag-source", dest="tag_source",
                      action="store_true", default=False,
                      help="Tag the source repo(s).")
    parser.add_option("--tag-l10n", dest="tag_l10n",
                      action="store_true", default=False,
                      help="Tag the L10n repo(s).")
    parser.add_option("--tag-other", dest="tag_other",
                      action="store_true", default=False,
                      help="Tag the other repo(s).")

    options, args = parser.parse_args()
    mercurial(options.buildbot_configs, 'buildbot-configs')
    update('buildbot-configs', revision=options.release_tag)
    config = validate(options, args)
    configDir = path.dirname(options.configfile)

    # We generate this upfront to ensure that it's consistent throughout all
    # repositories that use it. However, in cases where a relbranch is provided
    # for all repositories, it will not be used
    generatedRelbranch = generateRelbranchName(config['version'])
    if config.get('relbranchPrefix'):
        generatedRelbranch = generateRelbranchName(
            config['version'], prefix=config['relbranchPrefix'])
    tags = getTags(config['baseTag'], config['buildNumber'])
    l10nRevisionFile = path.join(
        'buildbot-configs', configDir, config['l10nRevisionFile'])
    l10nRepos = getL10nRepositories(
        open(l10nRevisionFile).read(), config['l10nRepoPath'])

    if options.tag_source:
        for repo in config['sourceRepositories'].values():
            relbranch = repo['relbranch'] or generatedRelbranch
            tagRepo(config, repo['path'], repo['name'], repo['revision'], tags,
                    repo['bumpFiles'], relbranch, options.attempts)
    failed = []

    if options.tag_l10n:
        for l in sorted(l10nRepos):
            info = l10nRepos[l]
            relbranch = config['l10nRelbranch'] or generatedRelbranch
            try:
                tagRepo(config, l, path.basename(l), info['revision'], tags,
                        info['bumpFiles'], relbranch, options.attempts)
            # If en-US tags successfully we'll do our best to tag all of the l10n
            # repos, even if some have errors
            except:
                failed.append((l, format_exc()))
    if 'otherReposToTag' in config and options.tag_other:
        for repo, revision in config['otherReposToTag'].iteritems():
            try:
                tagOtherRepo(config, repo, path.basename(repo), revision,
                             options.attempts)
            except:
                failed.append((repo, format_exc()))
    if len(failed) > 0:
        log.info("The following locales failed to tag:")
        for l, e in failed:
            log.info("  %s" % l)
            log.info("%s\n" % e)
        sys.exit(1)
