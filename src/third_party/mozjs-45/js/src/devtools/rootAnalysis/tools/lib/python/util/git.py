"""Functions for interacting with hg"""
import os
import subprocess
import urlparse
import urllib
import re

from util.commands import run_cmd, remove_path, run_quiet_cmd
from util.file import safe_unlink

import logging
log = logging.getLogger(__name__)


class DefaultShareBase:
    pass
DefaultShareBase = DefaultShareBase()


def _make_absolute(repo):
    if repo.startswith("file://"):
        # Make file:// urls absolute
        path = repo[len("file://"):]
        repo = "file://%s" % os.path.abspath(path)
    elif "://" not in repo:
        repo = os.path.abspath(repo)
    return repo


def get_repo_name(repo):
    bits = urlparse.urlsplit(repo)
    host = urllib.quote(bits.netloc, "")
    path = urllib.quote(bits.path.lstrip("/"), "")
    return os.path.join(host, path)


def has_revision(dest, revision):
    """Returns True if revision exists in dest"""
    try:
        run_quiet_cmd(['git', 'log', '--oneline', '-n1', revision], cwd=dest)
        return True
    except subprocess.CalledProcessError:
        return False


def has_ref(dest, refname):
    """Returns True if refname exists in dest.
    refname can be a branch or tag name."""
    try:
        run_quiet_cmd(['git', 'show-ref', '-d', refname], cwd=dest)
        return True
    except subprocess.CalledProcessError:
        return False


def init(dest, bare=False):
    """Initializes an empty repository at dest. If dest exists and isn't empty, it will be removed.
    If `bare` is True, then a bare repo will be created."""
    if not os.path.isdir(dest):
        log.info("removing %s", dest)
        safe_unlink(dest)
    else:
        for f in os.listdir(dest):
            f = os.path.join(dest, f)
            log.info("removing %s", f)
            if os.path.isdir(f):
                remove_path(f)
            else:
                safe_unlink(f)

    # Git will freak out if it tries to create intermediate directories for
    # dest, and then they exist. We can hit this when pulling in multiple repos
    # in parallel to shared repo paths that contain common parent directories
    # Let's create all the directories first
    try:
        os.makedirs(dest)
    except OSError, e:
        if e.errno == 20:
            # Not a directory error...one of the parents of dest isn't a
            # directory
            raise

    if bare:
        cmd = ['git', 'init', '--bare', '-q', dest]
    else:
        cmd = ['git', 'init', '-q', dest]

    log.info(" ".join(cmd))
    run_quiet_cmd(cmd)


def is_git_repo(dest):
    """Returns True if dest is a valid git repo"""
    if not os.path.isdir(dest):
        return False

    try:
        proc = subprocess.Popen(["git", "rev-parse", "--git-dir"], cwd=dest, stdout=subprocess.PIPE)
        if proc.wait() != 0:
            return False
        output = proc.stdout.read().strip()
        git_dir = os.path.normpath(os.path.join(dest, output))
        retval = (git_dir == dest or git_dir == os.path.join(dest, ".git"))
        return retval
    except subprocess.CalledProcessError:
        return False


def get_git_dir(dest):
    """Returns the path to the git directory for dest. For bare repos this is
    dest itself, for regular repos this is dest/.git"""
    assert is_git_repo(dest)
    cmd = ['git', 'config', '--bool', '--get', 'core.bare']
    proc = subprocess.Popen(cmd, cwd=dest, stdout=subprocess.PIPE)
    proc.wait()
    is_bare = proc.stdout.read().strip()
    if is_bare == "false":
        d = os.path.join(dest, ".git")
    else:
        d = dest
    assert os.path.exists(d)
    return d


def set_share(repo, share):
    alternates = os.path.join(get_git_dir(repo), 'objects', 'info', 'alternates')
    share_objects = os.path.join(get_git_dir(share), 'objects')

    with open(alternates, 'w') as f:
        f.write("%s\n" % share_objects)


def clean(repo):
    # Two '-f's means "clean submodules", which is what we want so far.
    run_cmd(['git', 'clean', '-f', '-f', '-d', '-x'], cwd=repo, stdout=subprocess.PIPE)


def add_remote(repo, remote_name, remote_repo):
    """Adds a remote named `remote_name` to the local git repo in `repo`
    pointing to `remote_repo`"""
    run_cmd(['git', 'remote', 'add', remote_name, remote_repo], cwd=repo)


def set_remote_url(repo, remote_name, remote_repo):
    """Sets the url for a remote"""
    run_cmd(['git', 'remote', 'set-url', remote_name, remote_repo], cwd=repo)


def get_remote(repo, remote_name):
    """Returns the url for the given remote, or None if the remote doesn't
    exist"""
    cmd = ['git', 'remote', '-v']
    proc = subprocess.Popen(cmd, cwd=repo, stdout=subprocess.PIPE)
    proc.wait()
    for line in proc.stdout.readlines():
        m = re.match(r"%s\s+(\S+) \(fetch\)$" % re.escape(remote_name), line)
        if m:
            return m.group(1)


def set_remote(repo, remote_name, remote_repo):
    """Sets remote named `remote_name` to `remote_repo`"""
    log.debug("%s: getting remotes", repo)
    my_remote = get_remote(repo, remote_name)
    if my_remote == remote_repo:
        return

    if my_remote is not None:
        log.info("%s: setting remote %s to %s (was %s)", repo, remote_name, remote_repo, my_remote)
        set_remote_url(repo, remote_name, remote_repo)
    else:
        log.info("%s: adding remote %s %s", repo, remote_name, remote_repo)
        add_remote(repo, remote_name, remote_repo)


def git(repo, dest, refname=None, revision=None, update_dest=True,
        shareBase=DefaultShareBase, mirrors=None, clean_dest=False):
    """Makes sure that `dest` is has `revision` or `refname` checked out from
    `repo`.

    Do what it takes to make that happen, including possibly clobbering
    dest.

    If `mirrors` is set, will try and use the mirrors before `repo`.
    """

    if shareBase is DefaultShareBase:
        shareBase = os.environ.get("GIT_SHARE_BASE_DIR", None)

    if shareBase is not None:
        repo_name = get_repo_name(repo)
        share_dir = os.path.join(shareBase, repo_name)
    else:
        share_dir = None

    if share_dir is not None and not is_git_repo(share_dir):
        log.info("creating bare repo %s", share_dir)
        try:
            init(share_dir, bare=True)
            os.utime(share_dir, None)
        except Exception:
            log.warning("couldn't create shared repo %s; disabling sharing", share_dir, exc_info=True)
            shareBase = None
            share_dir = None

    dest = os.path.abspath(dest)

    log.info("Checking dest %s", dest)
    if not is_git_repo(dest):
        if os.path.exists(dest):
            log.warning("%s doesn't appear to be a valid git directory; clobbering", dest)
            remove_path(dest)

        if share_dir is not None:
            # Initialize the repo and set up the share
            init(dest)
            set_share(dest, share_dir)
        else:
            # Otherwise clone into dest
            clone(repo, dest, refname=refname, mirrors=mirrors, update_dest=False)

    # Make sure our share is pointing to the right place
    if share_dir is not None:
        lock_file = os.path.join(get_git_dir(share_dir), "index.lock")
        if os.path.exists(lock_file):
            log.info("removing %s", lock_file)
            safe_unlink(lock_file)
        set_share(dest, share_dir)

    # If we're supposed to be updating to a revision, check if we
    # have that revision already. If so, then there's no need to
    # fetch anything.
    do_fetch = False
    if revision is None:
        # we don't have a revision specified, so pull in everything
        do_fetch = True
    elif has_ref(dest, revision):
        # revision is actually a ref name, so we need to run fetch
        # to make sure we update the ref
        do_fetch = True
    elif not has_revision(dest, revision):
        # we don't have this revision, so need to fetch it
        do_fetch = True

    if do_fetch:
        if share_dir:
            # Fetch our refs into our share
            try:
                # TODO: Handle fetching refnames like refs/tags/XXXX
                if refname is None:
                    fetch(repo, share_dir, mirrors=mirrors)
                else:
                    fetch(repo, share_dir, mirrors=mirrors, refname=refname)
            except subprocess.CalledProcessError:
                # Something went wrong!
                # Clobber share_dir and re-raise
                log.info("error fetching into %s - clobbering", share_dir)
                remove_path(share_dir)
                raise

            try:
                if refname is None:
                    fetch(share_dir, dest, fetch_remote="origin")
                else:
                    fetch(share_dir, dest, fetch_remote="origin", refname=refname)
            except subprocess.CalledProcessError:
                log.info("clobbering %s", share_dir)
                remove_path(share_dir)
                log.info("error fetching into %s - clobbering", dest)
                remove_path(dest)
                raise

        else:
            try:
                fetch(repo, dest, mirrors=mirrors, refname=refname)
            except Exception:
                log.info("error fetching into %s - clobbering", dest)
                remove_path(dest)
                raise

    # Set our remote
    set_remote(dest, 'origin', repo)

    if update_dest:
        log.info("Updating local copy refname: %s; revision: %s", refname, revision)
        # Sometimes refname is passed in as a revision
        if revision:
            if not has_revision(dest, revision) and has_ref(dest, 'origin/%s' % revision):
                log.info("Using %s as ref name instead of revision", revision)
                refname = revision
                revision = None

        rev = update(dest, refname=refname, revision=revision)
        if clean_dest:
            clean(dest)
        return rev

    if clean_dest:
        clean(dest)


def clone(repo, dest, refname=None, mirrors=None, shared=False, update_dest=True):
    """Clones git repo and places it at `dest`, replacing whatever else is
    there.  The working copy will be empty.

    If `mirrors` is set, will try and clone from the mirrors before
    cloning from `repo`.

    If `shared` is True, then git shared repos will be used

    If `update_dest` is False, then no working copy will be created
    """
    if os.path.exists(dest):
        remove_path(dest)

    if mirrors:
        log.info("Attempting to clone from mirrors")
        for mirror in mirrors:
            log.info("Cloning from %s", mirror)
            try:
                retval = clone(mirror, dest, refname, update_dest=update_dest)
                return retval
            except KeyboardInterrupt:
                raise
            except Exception:
                log.exception("Problem cloning from mirror %s", mirror)
                continue
        else:
            log.info("Pulling from mirrors failed; falling back to %s", repo)

    cmd = ['git', 'clone', '-q']
    if not update_dest:
        # TODO: Use --bare/--mirror here?
        cmd.append('--no-checkout')

    if refname:
        cmd.extend(['--branch', refname])

    if shared:
        cmd.append('--shared')

    cmd.extend([repo, dest])
    run_cmd(cmd)
    if update_dest:
        return get_revision(dest)


def update(dest, refname=None, revision=None, remote_name="origin"):
    """Updates working copy `dest` to `refname` or `revision`.  If neither is
    set then the working copy will be updated to the latest revision on the
    current refname.  Local changes will be discarded."""
    # If we have a revision, switch to that
    # We use revision^0 (and refname^0 below) to force the names to be
    # dereferenced into commit ids, which makes git check them out in a
    # detached state. This is equivalent to 'git checkout --detach', except is
    # supported by older versions of git
    if revision is not None:
        cmd = ['git', 'checkout', '-q', '-f', revision + '^0']
        run_cmd(cmd, cwd=dest)
    else:
        if not refname:
            refname = '%s/master' % remote_name
        else:
            refname = '%s/%s' % (remote_name, refname)
        cmd = ['git', 'checkout', '-q', '-f', refname + '^0']

        run_cmd(cmd, cwd=dest)
    return get_revision(dest)


def fetch(repo, dest, refname=None, remote_name="origin", fetch_remote=None, mirrors=None, fetch_tags=True):
    """Fetches changes from git repo and places it in `dest`.

    If `mirrors` is set, will try and fetch from the mirrors first before
    `repo`."""

    if mirrors:
        for mirror in mirrors:
            try:
                return fetch(mirror, dest, refname=refname)
            except KeyboardInterrupt:
                raise
            except Exception:
                log.exception("Problem fetching from mirror %s", mirror)
                continue
        else:
            log.info("Pulling from mirrors failed; falling back to %s", repo)

    # Convert repo to an absolute path if it's a local repository
    repo = _make_absolute(repo)
    cmd = ['git', 'fetch', '-q', repo]
    if not fetch_tags:
        # Don't fetch tags into our local tags/ refs since we have no way to
        # associate those with this remote and can't purge it later.
        cmd.append('--no-tags')
    if refname:
        if fetch_remote:
            cmd.append("+refs/remotes/{fetch_remote}/{refname}:refs/remotes/{remote_name}/{refname}".format(refname=refname, remote_name=remote_name, fetch_remote=fetch_remote))
        else:
            cmd.append("+refs/heads/{refname}:refs/remotes/{remote_name}/{refname}".format(refname=refname, remote_name=remote_name))
    else:
        if fetch_remote:
            cmd.append("+refs/remotes/{fetch_remote}/*:refs/remotes/{remote_name}/*".format(remote_name=remote_name, fetch_remote=fetch_remote))
        else:
            cmd.append("+refs/heads/*:refs/remotes/{remote_name}/*".format(remote_name=remote_name))

    run_cmd(cmd, cwd=dest)


def get_revision(path):
    """Returns which revision directory `path` currently has checked out."""
    proc = subprocess.Popen(['git', 'rev-parse', 'HEAD'], cwd=path, stdout=subprocess.PIPE)
    proc.wait()
    return proc.stdout.read().strip()
