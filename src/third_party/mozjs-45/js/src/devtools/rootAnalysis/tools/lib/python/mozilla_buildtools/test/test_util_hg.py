import unittest
import tempfile
import shutil
import os
import subprocess
import time
import util.hg as hg
from util.hg import clone, pull, update, hg_ver, mercurial, _make_absolute, \
    share, push, apply_and_push, HgUtilError, make_hg_url, get_branch, purge, \
    get_branches, path, init, unbundle, adjust_paths, is_hg_cset, commit, tag, \
    get_hg_output, has_rev
from util.file import touch
from util.commands import run_cmd, get_output

from mock import patch


def getRevisions(dest):
    retval = []
    for rev in get_output(['hg', 'log', '-R', dest, '--template', '{node|short}\n']).split('\n'):
        rev = rev.strip()
        if not rev:
            continue
        retval.append(rev)
    return retval


def getRevInfo(dest, rev):
    output = get_output(['hg', 'log', '-R', dest, '-r', rev, '--template',
                        '{author}\n{desc}\n{tags}']).splitlines()
    info = {
        'user': output[0],
        'msg': output[1],
        'tags': []
    }
    if len(output) > 2:
        info['tags'] = output[2].split()
    return info


def getTags(dest):
    tags = []
    for t in get_output(['hg', 'tags', '-R', dest]).splitlines():
        tags.append(t.split()[0])
    return tags


def yesterday_timestamp():
    """returns a valid yesterday timestamp for touch"""
    yesterday = time.time() - 86400
    return (yesterday, yesterday)


class TestMakeAbsolute(unittest.TestCase):
    def testAbsolutePath(self):
        self.assertEquals(_make_absolute("/foo/bar"), "/foo/bar")

    def testStripTrailingSlash(self):
        self.assertEquals(_make_absolute("/foo/bar/"), "/foo/bar")

    def testRelativePath(self):
        self.assertEquals(
            _make_absolute("foo/bar"), os.path.abspath("foo/bar"))

    def testHTTPPaths(self):
        self.assertEquals(_make_absolute("http://foo/bar"), "http://foo/bar")

    def testAbsoluteFilePath(self):
        self.assertEquals(_make_absolute("file:///foo/bar"), "file:///foo/bar")

    def testRelativeFilePath(self):
        self.assertEquals(_make_absolute(
            "file://foo/bar"), "file://%s/foo/bar" % os.getcwd())


class TestIsHgCset(unittest.TestCase):

    def testShortCset(self):
        self.assertTrue(is_hg_cset('fd06332733e5'))

    def testLongCset(self):
        self.assertTrue(is_hg_cset('bf37aabfd9367aec573487ebe1f784108bbef73f'))

    def testMediumCset(self):
        self.assertTrue(is_hg_cset('1e3391794bac9d0e707a7681de3'))

    def testInt(self):
        self.assertFalse(is_hg_cset(1234567890))

    def testTag(self):
        self.assertFalse(is_hg_cset('FIREFOX_50_0_RELEASE'))

    def testDefault(self):
        self.assertFalse(is_hg_cset('default'))

    def testTip(self):
        self.assertFalse(is_hg_cset('tip'))

    def testBranch(self):
        self.assertFalse(is_hg_cset('GECKO77_203512230833_RELBRANCH'))


class TestHg(unittest.TestCase):
    def setUp(self):
        tmpdir = None
        if os.path.isdir("/dev/shm"):
            tmpdir = "/dev/shm"
        self.tmpdir = tempfile.mkdtemp(dir=tmpdir)
        self.pwd = os.getcwd()
        os.chdir(self.tmpdir)
        self.repodir = os.path.join(self.tmpdir, 'repo')
        # Have a stable hgrc to test with
        os.environ['HGRCPATH'] = os.path.join(os.path.dirname(__file__), "hgrc")
        run_cmd(['%s/init_hgrepo.sh' % os.path.dirname(__file__),
                self.repodir])

        self.revisions = getRevisions(self.repodir)
        self.wc = os.path.join(self.tmpdir, 'wc')
        self.sleep_patcher = patch('time.sleep')
        self.sleep_patcher.start()
        hg.RETRY_ATTEMPTS = 2

    def tearDown(self):
        shutil.rmtree(self.tmpdir)
        os.chdir(self.pwd)
        self.sleep_patcher.stop()

    def testGetBranch(self):
        clone(self.repodir, self.wc)
        b = get_branch(self.wc)
        self.assertEquals(b, 'default')

    def testGetBranches(self):
        clone(self.repodir, self.wc)
        branches = get_branches(self.wc)
        self.assertEquals(sorted(branches), sorted(["branch2", "default"]))

    def testClone(self):
        rev = clone(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)
        self.assertEquals(self.revisions, getRevisions(self.wc))
        self.assertEquals(sorted(os.listdir(self.wc)), ['.hg'])

    def testCloneIntoNonEmptyDir(self):
        os.mkdir(self.wc)
        open(os.path.join(self.wc, 'test.txt'), 'w').write('hello')
        clone(self.repodir, self.wc, update_dest=False)
        self.failUnless(not os.path.exists(os.path.join(self.wc, 'test.txt')))

    def testCloneUpdate(self):
        rev = clone(self.repodir, self.wc, update_dest=True)
        self.assertEquals(rev, self.revisions[0])

    def testCloneBranch(self):
        clone(self.repodir, self.wc, branch='branch2',
              update_dest=False, clone_by_rev=True)
        # On hg 1.6, we should only have a subset of the revisions
        if hg_ver() >= (1, 6, 0):
            self.assertEquals(self.revisions[1:],
                              getRevisions(self.wc))
        else:
            self.assertEquals(self.revisions,
                              getRevisions(self.wc))

    def testCloneUpdateBranch(self):
        rev = clone(self.repodir, os.path.join(self.tmpdir, 'wc'),
                    branch="branch2", update_dest=True, clone_by_rev=True)
        self.assertEquals(rev, self.revisions[1], self.revisions)

    def testCloneRevision(self):
        clone(self.repodir, self.wc,
              revision=self.revisions[0], update_dest=False,
              clone_by_rev=True)
        # We'll only get a subset of the revisions
        self.assertEquals(self.revisions[:1] + self.revisions[2:],
                          getRevisions(self.wc))

    def testUpdateRevision(self):
        rev = clone(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)

        rev = update(self.wc, revision=self.revisions[1])
        self.assertEquals(rev, self.revisions[1])

    def testPull(self):
        # Clone just the first rev
        clone(self.repodir, self.wc, revision=self.revisions[-1],
              update_dest=False, clone_by_rev=True)
        self.assertEquals(getRevisions(self.wc), self.revisions[-1:])

        # Now pull in new changes
        rev = pull(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)
        self.assertEquals(getRevisions(self.wc), self.revisions)

    def testPullRevision(self):
        # Clone just the first rev
        clone(self.repodir, self.wc, revision=self.revisions[-1],
              update_dest=False, clone_by_rev=True)
        self.assertEquals(getRevisions(self.wc), self.revisions[-1:])

        # Now pull in just the last revision
        rev = pull(self.repodir, self.wc, revision=self.revisions[
                   0], update_dest=False)
        self.assertEquals(rev, None)

        # We'll be missing the middle revision (on another branch)
        self.assertEquals(
            getRevisions(self.wc), self.revisions[:1] + self.revisions[2:])

    def testPullBranch(self):
        # Clone just the first rev
        clone(self.repodir, self.wc, revision=self.revisions[-1],
              update_dest=False, clone_by_rev=True)
        self.assertEquals(getRevisions(self.wc), self.revisions[-1:])

        # Now pull in the other branch
        rev = pull(self.repodir, self.wc, branch="branch2", update_dest=False)
        self.assertEquals(rev, None)

        # On hg 1.6, we'll be missing the last revision (on another branch)
        if hg_ver() >= (1, 6, 0):
            self.assertEquals(getRevisions(self.wc), self.revisions[1:])
        else:
            self.assertEquals(getRevisions(self.wc), self.revisions)

    def testPullTag(self):
        clone(self.repodir, self.wc)
        run_cmd(['hg', 'tag', '-f', 'TAG1'], cwd=self.repodir)
        revisions = getRevisions(self.repodir)

        rev = pull(self.repodir, self.wc, revision='TAG1')
        self.assertEquals(rev, revisions[1])
        self.assertEquals(getRevisions(self.wc), revisions)

    def testPullTip(self):
        clone(self.repodir, self.wc)
        run_cmd(['hg', 'tag', '-f', 'TAG1'], cwd=self.repodir)
        revisions = getRevisions(self.repodir)

        rev = pull(self.repodir, self.wc, revision='tip')
        self.assertEquals(rev, revisions[0])
        self.assertEquals(getRevisions(self.wc), revisions)

    def testPullDefault(self):
        clone(self.repodir, self.wc)
        run_cmd(['hg', 'tag', '-f', 'TAG1'], cwd=self.repodir)
        revisions = getRevisions(self.repodir)

        rev = pull(self.repodir, self.wc, revision='default')
        self.assertEquals(rev, revisions[0])
        self.assertEquals(getRevisions(self.wc), revisions)

    def testPullUnrelated(self):
        # Create a new repo
        repo2 = os.path.join(self.tmpdir, 'repo2')
        run_cmd(['%s/init_hgrepo.sh' % os.path.dirname(__file__), repo2],
                env={'extra': 'unrelated'})

        self.assertNotEqual(self.revisions, getRevisions(repo2))

        # Clone the original repo
        clone(self.repodir, self.wc, update_dest=False)

        # Try and pull in changes from the new repo
        self.assertRaises(subprocess.CalledProcessError, pull,
                          repo2, self.wc, update_dest=False)

    def testShareUnrelated(self):
        # Create a new repo
        repo2 = os.path.join(self.tmpdir, 'repo2')
        run_cmd(['%s/init_hgrepo.sh' % os.path.dirname(__file__), repo2],
            env={'extra': 'share_unrelated'})

        self.assertNotEqual(self.revisions, getRevisions(repo2))

        shareBase = os.path.join(self.tmpdir, 'share')

        # Clone the original repo
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        # Clone the new repo
        mercurial(repo2, self.wc, shareBase=shareBase)

        self.assertEquals(getRevisions(self.wc), getRevisions(repo2))

    def testShareExtraFiles(self):
        shareBase = os.path.join(self.tmpdir, 'share')
        backup = os.path.join(self.tmpdir, 'backup')

        # Clone the original repo
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        clone(self.repodir, backup)

        # Make the working repo have a new file. We need it to have an earlier
        # timestamp (yesterday) to trigger the odd behavior in hg
        newfile = os.path.join(self.wc, 'newfile')
        touch(newfile, timestamp=yesterday_timestamp())
        run_cmd(['hg', 'add', 'newfile'], cwd=self.wc)
        run_cmd(['hg', 'commit', '-m', '"add newfile"'], cwd=self.wc)

        # Reset the share base to remove the 'add newfile' commit. We
        # overwrite repodir with the backup that doesn't have the commit,
        # then clone the repodir to a throwaway dir to create the new
        # shareBase. Now self.wc still points to shareBase, but the
        # changeset that self.wc was on is lost.
        shutil.rmtree(self.repodir)
        shutil.rmtree(shareBase)
        clone(backup, self.repodir)
        throwaway = os.path.join(self.tmpdir, 'throwaway')
        mercurial(self.repodir, throwaway, shareBase=shareBase)

        # Try and update our working copy
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        self.assertFalse(os.path.exists(os.path.join(self.wc, 'newfile')))

    def testShareExtraFilesReset(self):
        shareBase = os.path.join(self.tmpdir, 'share')

        # Clone the original repo
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        # Reset the repo
        run_cmd(
            ['%s/init_hgrepo.sh' % os.path.dirname(__file__), self.repodir],
            env={'extra': 'extra_files_reset'})

        # Make the working repo have a new file. We need it to have an earlier
        # timestamp (yesterday) to trigger the odd behavior in hg
        newfile = os.path.join(self.wc, 'newfile')
        touch(newfile, timestamp=yesterday_timestamp())
        run_cmd(['hg', 'add', 'newfile'], cwd=self.wc)
        run_cmd(['hg', 'commit', '-m', '"add newfile"'], cwd=self.wc)

        # Try and update our working copy
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        self.assertFalse(os.path.exists(os.path.join(self.wc, 'newfile')))

    def testShareReset(self):
        shareBase = os.path.join(self.tmpdir, 'share')

        # Clone the original repo
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        old_revs = self.revisions[:]

        # Reset the repo
        run_cmd(
            ['%s/init_hgrepo.sh' % os.path.dirname(__file__), self.repodir],
            env={'extra': 'share_reset'})

        self.assertNotEqual(old_revs, getRevisions(self.repodir))

        # Try and update our working copy
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        self.assertEquals(getRevisions(self.repodir), getRevisions(self.wc))
        self.assertNotEqual(old_revs, getRevisions(self.wc))

    def testPush(self):
        clone(self.repodir, self.wc, revision=self.revisions[-2],
              clone_by_rev=True)
        push(src=self.repodir, remote=self.wc)
        self.assertEquals(getRevisions(self.wc), self.revisions)

    def testPushWithBranch(self):
        clone(self.repodir, self.wc, revision=self.revisions[-1],
              clone_by_rev=True)
        push(src=self.repodir, remote=self.wc, branch='branch2')
        push(src=self.repodir, remote=self.wc, branch='default')
        self.assertEquals(getRevisions(self.wc), self.revisions)

    def testPushWithRevision(self):
        clone(self.repodir, self.wc, revision=self.revisions[-1],
              clone_by_rev=True)
        push(src=self.repodir, remote=self.wc, revision=self.revisions[-2])
        self.assertEquals(getRevisions(self.wc), self.revisions[-2:])

    def testPurgeUntrackedFile(self):
        clone(self.repodir, self.wc)
        fileToPurge = os.path.join(self.wc, 'fileToPurge')
        with file(fileToPurge, 'a') as f:
            f.write('purgeme')
        purge(self.wc)
        self.assertFalse(os.path.exists(fileToPurge))

    def testPurgeUntrackedDirectory(self):
        clone(self.repodir, self.wc)
        directoryToPurge = os.path.join(self.wc, 'directoryTopPurge')
        os.makedirs(directoryToPurge)
        purge(directoryToPurge)
        self.assertFalse(os.path.isdir(directoryToPurge))

    def testPurgeVeryLongPath(self):
        clone(self.repodir, self.wc)
        # now create a very long path name
        longPath = self.wc
        for new_dir in xrange(1, 64):
            longPath = os.path.join(longPath, str(new_dir))
        os.makedirs(longPath)
        self.assertTrue(os.path.isdir(longPath))
        purge(self.wc)
        self.assertFalse(os.path.isdir(longPath))

    def testPurgeAFreshClone(self):
        clone(self.repodir, self.wc)
        purge(self.wc)
        self.assertTrue(os.path.exists(os.path.join(self.wc, 'hello.txt')))

    def testPurgeTrackedFile(self):
        clone(self.repodir, self.wc)
        fileToModify = os.path.join(self.wc, 'hello.txt')
        with open(fileToModify, 'w') as f:
            f.write('hello!')
        purge(self.wc)
        with open(fileToModify, 'r') as f:
            content = f.read()
        self.assertEqual(content, 'hello!')

    def testMercurial(self):
        rev = mercurial(self.repodir, self.wc)
        self.assertEquals(rev, self.revisions[0])

    def testPushNewBranchesNotAllowed(self):
        clone(self.repodir, self.wc, revision=self.revisions[0],
              clone_by_rev=True)
        self.assertRaises(Exception, push, self.repodir, self.wc,
                          push_new_branches=False)

    def testPushWithForce(self):
        clone(self.repodir, self.wc, revision=self.revisions[0],
              clone_by_rev=True)
        newfile = os.path.join(self.wc, 'newfile')
        touch(newfile)
        run_cmd(['hg', 'add', 'newfile'], cwd=self.wc)
        run_cmd(['hg', 'commit', '-m', '"re-add newfile"'], cwd=self.wc)
        push(self.repodir, self.wc, push_new_branches=False, force=True)

    def testPushForceFail(self):
        clone(self.repodir, self.wc, revision=self.revisions[0],
              clone_by_rev=True)
        newfile = os.path.join(self.wc, 'newfile')
        touch(newfile)
        run_cmd(['hg', 'add', 'newfile'], cwd=self.wc)
        run_cmd(['hg', 'commit', '-m', '"add newfile"'], cwd=self.wc)
        self.assertRaises(Exception, push, self.repodir, self.wc,
                          push_new_branches=False, force=False)

    def testMercurialWithNewShare(self):
        shareBase = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(shareBase, self.repodir.lstrip("/"))
        os.mkdir(shareBase)
        mercurial(self.repodir, self.wc, shareBase=shareBase)
        self.assertEquals(getRevisions(self.repodir), getRevisions(self.wc))
        self.assertEquals(getRevisions(self.repodir), getRevisions(sharerepo))

    def testMercurialWithShareBaseInEnv(self):
        shareBase = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(shareBase, self.repodir.lstrip("/"))
        os.mkdir(shareBase)
        try:
            os.environ['HG_SHARE_BASE_DIR'] = shareBase
            mercurial(self.repodir, self.wc)
            self.assertEquals(
                getRevisions(self.repodir), getRevisions(self.wc))
            self.assertEquals(
                getRevisions(self.repodir), getRevisions(sharerepo))
        finally:
            del os.environ['HG_SHARE_BASE_DIR']

    def testMercurialWithExistingShare(self):
        shareBase = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(shareBase, self.repodir.lstrip("/"))
        os.mkdir(shareBase)
        mercurial(self.repodir, sharerepo)
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)
        mercurial(self.repodir, self.wc, shareBase=shareBase)
        self.assertEquals(getRevisions(self.repodir), getRevisions(self.wc))
        self.assertEquals(getRevisions(self.repodir), getRevisions(sharerepo))

    def testMercurialRelativeDir(self):
        os.chdir(os.path.dirname(self.repodir))

        repo = os.path.basename(self.repodir)
        wc = os.path.basename(self.wc)

        rev = mercurial(repo, wc, revision=self.revisions[-1])
        self.assertEquals(rev, self.revisions[-1])
        open(os.path.join(self.wc, 'test.txt'), 'w').write("hello!")

        rev = mercurial(repo, wc)
        self.assertEquals(rev, self.revisions[0])
        # Make sure our local file didn't go away
        self.failUnless(os.path.exists(os.path.join(self.wc, 'test.txt')))

    def testMercurialUpdateTip(self):
        rev = mercurial(self.repodir, self.wc, revision=self.revisions[-1])
        self.assertEquals(rev, self.revisions[-1])
        open(os.path.join(self.wc, 'test.txt'), 'w').write("hello!")

        rev = mercurial(self.repodir, self.wc)
        self.assertEquals(rev, self.revisions[0])
        # Make sure our local file didn't go away
        self.failUnless(os.path.exists(os.path.join(self.wc, 'test.txt')))

    def testMercurialUpdateRev(self):
        rev = mercurial(self.repodir, self.wc, revision=self.revisions[-1])
        self.assertEquals(rev, self.revisions[-1])
        open(os.path.join(self.wc, 'test.txt'), 'w').write("hello!")

        rev = mercurial(self.repodir, self.wc, revision=self.revisions[0])
        self.assertEquals(rev, self.revisions[0])
        # Make sure our local file didn't go away
        self.failUnless(os.path.exists(os.path.join(self.wc, 'test.txt')))

    # TODO: this test doesn't seem to be compatible with mercurial()'s
    # share() usage, and fails when HG_SHARE_BASE_DIR is set
    def testMercurialChangeRepo(self):
        # Create a new repo
        old_env = os.environ.copy()
        if 'HG_SHARE_BASE_DIR' in os.environ:
            del os.environ['HG_SHARE_BASE_DIR']

        try:
            repo2 = os.path.join(self.tmpdir, 'repo2')
            run_cmd(['%s/init_hgrepo.sh' % os.path.dirname(__file__), repo2],
                    env={'extra': 'change_repo'})

            self.assertNotEqual(self.revisions, getRevisions(repo2))

            # Clone the original repo
            mercurial(self.repodir, self.wc)
            self.assertEquals(getRevisions(self.wc), self.revisions)
            open(os.path.join(self.wc, 'test.txt'), 'w').write("hello!")

            # Clone the new one
            mercurial(repo2, self.wc)
            self.assertEquals(getRevisions(self.wc), getRevisions(repo2))
            # Make sure our local file went away
            self.failUnless(
                not os.path.exists(os.path.join(self.wc, 'test.txt')))
        finally:
            os.environ.clear()
            os.environ.update(old_env)

    def testMakeHGUrl(self):
        # construct an hg url specific to revision, branch and filename and try
        # to pull it down
        file_url = make_hg_url(
            "hg.mozilla.org",
            '//build/tools/',
            revision='FIREFOX_3_6_12_RELEASE',
            filename="/lib/python/util/hg.py"
        )
        expected_url = "https://hg.mozilla.org/build/tools/raw-file/FIREFOX_3_6_12_RELEASE/lib/python/util/hg.py"
        self.assertEquals(file_url, expected_url)

    def testMakeHGUrlNoFilename(self):
        file_url = make_hg_url(
            "hg.mozilla.org",
            "/build/tools",
            revision="default"
        )
        expected_url = "https://hg.mozilla.org/build/tools/rev/default"
        self.assertEquals(file_url, expected_url)

    def testMakeHGUrlNoRevisionNoFilename(self):
        repo_url = make_hg_url(
            "hg.mozilla.org",
            "/build/tools"
        )
        expected_url = "https://hg.mozilla.org/build/tools"
        self.assertEquals(repo_url, expected_url)

    def testMakeHGUrlDifferentProtocol(self):
        repo_url = make_hg_url(
            "hg.mozilla.org",
            "/build/tools",
            protocol='ssh'
        )
        expected_url = "ssh://hg.mozilla.org/build/tools"
        self.assertEquals(repo_url, expected_url)

    def testShareRepo(self):
        repo3 = os.path.join(self.tmpdir, 'repo3')
        share(self.repodir, repo3)
        # make sure shared history is identical
        self.assertEquals(self.revisions, getRevisions(repo3))

    def testMercurialShareOutgoing(self):
        # ensure that outgoing changesets in a shared clone affect the shared
        # history
        repo5 = os.path.join(self.tmpdir, 'repo5')
        repo6 = os.path.join(self.tmpdir, 'repo6')
        mercurial(self.repodir, repo5)
        share(repo5, repo6)
        open(os.path.join(repo6, 'test.txt'), 'w').write("hello!")
        # modify the history of the new clone
        run_cmd(['hg', 'add', 'test.txt'], cwd=repo6)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=repo6)
        self.assertNotEquals(self.revisions, getRevisions(repo6))
        self.assertNotEquals(self.revisions, getRevisions(repo5))
        self.assertEquals(getRevisions(repo5), getRevisions(repo6))

    def testApplyAndPush(self):
        clone(self.repodir, self.wc)

        def c(repo, attempt):
            run_cmd(['hg', 'tag', '-f', 'TEST'], cwd=repo)
        apply_and_push(self.wc, self.repodir, c)
        self.assertEquals(getRevisions(self.wc), getRevisions(self.repodir))

    def testApplyAndPushFail(self):
        clone(self.repodir, self.wc)

        def c(repo, attempt, remote):
            run_cmd(['hg', 'tag', '-f', 'TEST'], cwd=repo)
            run_cmd(['hg', 'tag', '-f', 'CONFLICTING_TAG'], cwd=remote)
        self.assertRaises(HgUtilError, apply_and_push, self.wc, self.repodir,
                          lambda r, a: c(r, a, self.repodir), max_attempts=2)

    def testApplyAndPushWithRebase(self):
        clone(self.repodir, self.wc)

        def c(repo, attempt, remote):
            run_cmd(['hg', 'tag', '-f', 'TEST'], cwd=repo)
            if attempt == 1:
                run_cmd(['hg', 'rm', 'hello.txt'], cwd=remote)
                run_cmd(['hg', 'commit', '-m', 'test'], cwd=remote)
        apply_and_push(self.wc, self.repodir,
                       lambda r, a: c(r, a, self.repodir), max_attempts=2)
        self.assertEquals(getRevisions(self.wc), getRevisions(self.repodir))

    def testApplyAndPushRebaseFails(self):
        clone(self.repodir, self.wc)

        def c(repo, attempt, remote):
            run_cmd(['hg', 'tag', '-f', 'TEST'], cwd=repo)
            if attempt in (1, 2):
                run_cmd(['hg', 'tag', '-f', 'CONFLICTING_TAG'], cwd=remote)
        apply_and_push(self.wc, self.repodir,
                       lambda r, a: c(r, a, self.repodir), max_attempts=3)
        self.assertEquals(getRevisions(self.wc), getRevisions(self.repodir))

    def testApplyAndPushOnBranch(self):
        clone(self.repodir, self.wc)

        def c(repo, attempt):
            run_cmd(['hg', 'branch', 'branch3'], cwd=repo)
            run_cmd(['hg', 'tag', '-f', 'TEST'], cwd=repo)
        apply_and_push(self.wc, self.repodir, c)
        self.assertEquals(getRevisions(self.wc), getRevisions(self.repodir))

    def testApplyAndPushWithNoChange(self):
        clone(self.repodir, self.wc)

        def c(r, a):
            pass
        self.assertRaises(
            HgUtilError, apply_and_push, self.wc, self.repodir, c)

    def testApplyAndPushForce(self):
        clone(self.repodir, self.wc)

        def c(repo, attempt, remote, local):
            newfile_remote = os.path.join(remote, 'newfile')
            newfile_local = os.path.join(local, 'newfile')
            touch(newfile_remote)
            run_cmd(['hg', 'add', 'newfile'], cwd=remote)
            run_cmd(['hg', 'commit', '-m', '"add newfile"'], cwd=remote)
            touch(newfile_local)
            run_cmd(['hg', 'add', 'newfile'], cwd=local)
            run_cmd(['hg', 'commit', '-m', '"re-add newfile"'], cwd=local)
        apply_and_push(self.wc, self.repodir,
                       (lambda r, a: c(r, a, self.repodir, self.wc)), force=True)

    def testApplyAndPushForceFail(self):
        clone(self.repodir, self.wc)

        def c(repo, attempt, remote, local):
            newfile_remote = os.path.join(remote, 'newfile')
            newfile_local = os.path.join(local, 'newfile')
            touch(newfile_remote)
            run_cmd(['hg', 'add', 'newfile'], cwd=remote)
            run_cmd(['hg', 'commit', '-m', '"add newfile"'], cwd=remote)
            touch(newfile_local)
            run_cmd(['hg', 'add', 'newfile'], cwd=local)
            run_cmd(['hg', 'commit', '-m', '"re-add newfile"'], cwd=local)
        self.assertRaises(HgUtilError,
                          apply_and_push, self.wc, self.repodir,
                          (lambda r, a: c(r, a, self.repodir, self.wc)), force=False)

    def testPath(self):
        clone(self.repodir, self.wc)
        p = path(self.wc)
        self.assertEquals(p, self.repodir)

    def testBustedHgrcWithShare(self):
        # Test that we can recover from hgrc being lost
        shareBase = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(shareBase, self.repodir.lstrip("/"))
        os.mkdir(shareBase)
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        # Delete .hg/hgrc
        for d in sharerepo, self.wc:
            f = os.path.join(d, '.hg', 'hgrc')
            os.unlink(f)

        # path is busted now
        p = path(self.wc)
        self.assertEquals(p, None)

        # cloning again should fix this up
        mercurial(self.repodir, self.wc, shareBase=shareBase)

        p = path(self.wc)
        self.assertEquals(p, self.repodir)

    def testBustedHgrc(self):
        # Test that we can recover from hgrc being lost
        mercurial(self.repodir, self.wc)

        # Delete .hg/hgrc
        os.unlink(os.path.join(self.wc, '.hg', 'hgrc'))

        # path is busted now
        p = path(self.wc)
        self.assertEquals(p, None)

        # cloning again should fix this up
        mercurial(self.repodir, self.wc)

        p = path(self.wc)
        self.assertEquals(p, self.repodir)

    def testInit(self):
        tmpdir = os.path.join(self.tmpdir, 'new')
        self.assertEquals(False, os.path.exists(tmpdir))
        init(tmpdir)
        self.assertEquals(True, os.path.exists(tmpdir))
        self.assertEquals(True, os.path.exists(os.path.join(tmpdir, '.hg')))

    def testUnbundle(self):
        # First create the bundle
        bundle = os.path.join(self.tmpdir, 'bundle')
        run_cmd(['hg', 'bundle', '-a', bundle], cwd=self.repodir)

        # Now unbundle it in a new place
        newdir = os.path.join(self.tmpdir, 'new')
        init(newdir)
        unbundle(bundle, newdir)

        self.assertEquals(self.revisions, getRevisions(newdir))

    def testCloneWithBundle(self):
        # First create the bundle
        bundle = os.path.join(self.tmpdir, 'bundle')
        run_cmd(['hg', 'bundle', '-a', bundle], cwd=self.repodir)

        # Wrap unbundle so we can tell if it got called
        orig_unbundle = unbundle
        try:
            called = []

            def new_unbundle(*args, **kwargs):
                called.append(True)
                return orig_unbundle(*args, **kwargs)
            hg.unbundle = new_unbundle

            # Now clone it using the bundle
            clone(self.repodir, self.wc, bundles=[bundle])
            self.assertEquals(self.revisions, getRevisions(self.wc))
            self.assertEquals(called, [True])
            self.assertEquals(path(self.wc), self.repodir)
        finally:
            hg.unbundle = orig_unbundle

    def testCloneWithUnrelatedBundle(self):
        # First create the bundle
        bundle = os.path.join(self.tmpdir, 'bundle')
        run_cmd(['hg', 'bundle', '-a', bundle], cwd=self.repodir)

        # Create an unrelated repo
        repo2 = os.path.join(self.tmpdir, 'repo2')
        run_cmd(['%s/init_hgrepo.sh' % os.path.dirname(__file__),
                 repo2], env={'extra': 'clone_unrelated'})

        self.assertNotEqual(self.revisions, getRevisions(repo2))

        # Clone repo2 using the unrelated bundle
        clone(repo2, self.wc, bundles=[bundle])

        # Make sure we don't have unrelated revisions
        self.assertEquals(getRevisions(repo2), getRevisions(self.wc))
        self.assertEquals(set(),
                          set(getRevisions(self.repodir)).intersection(set(getRevisions(self.wc))))

    def testCloneWithBadBundle(self):
        # First create the bad bundle
        bundle = os.path.join(self.tmpdir, 'bundle')
        open(bundle, 'w').write('ruh oh!')

        # Wrap unbundle so we can tell if it got called
        orig_unbundle = unbundle
        try:
            called = []

            def new_unbundle(*args, **kwargs):
                called.append(True)
                return orig_unbundle(*args, **kwargs)
            hg.unbundle = new_unbundle

            # Now clone it using the bundle
            clone(self.repodir, self.wc, bundles=[bundle])
            self.assertEquals(self.revisions, getRevisions(self.wc))
            self.assertEquals(called, [True])
        finally:
            hg.unbundle = orig_unbundle

    def testCloneWithBundleMissingRevs(self):
        # First create the bundle
        bundle = os.path.join(self.tmpdir, 'bundle')
        run_cmd(['hg', 'bundle', '-a', bundle], cwd=self.repodir)

        # Create a commit
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)

        # Wrap unbundle so we can tell if it got called
        orig_unbundle = unbundle
        try:
            called = []

            def new_unbundle(*args, **kwargs):
                called.append(True)
                return orig_unbundle(*args, **kwargs)
            hg.unbundle = new_unbundle

            # Now clone it using the bundle
            clone(self.repodir, self.wc, bundles=[bundle])
            self.assertEquals(
                getRevisions(self.repodir), getRevisions(self.wc))
            self.assertEquals(called, [True])
        finally:
            hg.unbundle = orig_unbundle

    def testCloneWithMirror(self):
        mirror = os.path.join(self.tmpdir, 'repo2')
        clone(self.repodir, mirror)

        # Create a commit in the original repo
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)

        # Now clone from the mirror
        clone(self.repodir, self.wc, mirrors=[mirror])

        # We'll be missing the new revision from repodir
        self.assertNotEquals(getRevisions(self.repodir), getRevisions(self.wc))
        # But we should have everything from the mirror
        self.assertEquals(getRevisions(mirror), getRevisions(self.wc))
        # Our default path should point to the original repo though.
        self.assertEquals(self.repodir, path(self.wc))

    def testCloneWithBadMirror(self):
        mirror = os.path.join(self.tmpdir, 'repo2')

        # Now clone from the mirror
        clone(self.repodir, self.wc, mirrors=[mirror])

        # We still end up with a valid repo
        self.assertEquals(self.revisions, getRevisions(self.wc))
        self.assertEquals(self.repodir, path(self.wc))

    def testCloneWithOneBadMirror(self):
        mirror1 = os.path.join(self.tmpdir, 'mirror1')
        mirror2 = os.path.join(self.tmpdir, 'mirror2')

        # Mirror 2 is ok
        clone(self.repodir, mirror2)

        # Create a commit in the original repo
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)

        # Now clone from the mirror
        clone(self.repodir, self.wc, mirrors=[mirror1, mirror2])

        # We'll be missing the new revision from repodir
        self.assertNotEquals(getRevisions(self.repodir), getRevisions(self.wc))
        # But we should have everything from the mirror
        self.assertEquals(getRevisions(mirror2), getRevisions(self.wc))
        # Our default path should point to the original repo though.
        self.assertEquals(self.repodir, path(self.wc))

    def testCloneWithRevAndMirror(self):
        mirror = os.path.join(self.tmpdir, 'repo2')
        clone(self.repodir, mirror)

        # Create a commit in the original repo
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)

        # Now clone from the mirror
        revisions = getRevisions(self.repodir)
        clone(self.repodir, self.wc, revision=revisions[0],
              mirrors=[mirror], clone_by_rev=True)

        # We'll be missing the middle revision (on another branch)
        self.assertEquals(revisions[:2] + revisions[3:], getRevisions(self.wc))
        # But not from the mirror
        self.assertNotEquals(getRevisions(mirror), getRevisions(self.wc))
        # Our default path should point to the original repo though.
        self.assertEquals(self.repodir, path(self.wc))

    def testPullWithMirror(self):
        mirror = os.path.join(self.tmpdir, 'repo2')
        clone(self.repodir, mirror)

        # Create a new commit in the mirror repo
        open(os.path.join(mirror, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=mirror)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=mirror)

        # Now clone from the original
        clone(self.repodir, self.wc)

        # Pull using the mirror
        pull(self.repodir, self.wc, mirrors=[mirror])

        self.assertEquals(getRevisions(self.wc), getRevisions(mirror))

        # Our default path should point to the original repo
        self.assertEquals(self.repodir, path(self.wc))

    def testPullWithBadMirror(self):
        mirror = os.path.join(self.tmpdir, 'repo2')

        # Now clone from the original
        clone(self.repodir, self.wc)

        # Create a new commit in the original repo
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)

        # Pull using the mirror
        pull(self.repodir, self.wc, mirrors=[mirror])

        self.assertEquals(getRevisions(self.wc), getRevisions(self.repodir))

        # Our default path should point to the original repo
        self.assertEquals(self.repodir, path(self.wc))

    def testPullWithUnrelatedMirror(self):
        mirror = os.path.join(self.tmpdir, 'repo2')
        run_cmd(['%s/init_hgrepo.sh' % os.path.dirname(__file__), mirror],
                env={'extra': 'pull_unrealted'})

        # Now clone from the original
        clone(self.repodir, self.wc)

        # Create a new commit in the original repo
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)

        # Pull using the mirror
        pull(self.repodir, self.wc, mirrors=[mirror])

        self.assertEquals(getRevisions(self.wc), getRevisions(self.repodir))
        # We shouldn't have anything from the unrelated mirror
        self.assertEquals(set(),
                          set(getRevisions(mirror)).intersection(set(getRevisions(self.wc))))

        # Our default path should point to the original repo
        self.assertEquals(self.repodir, path(self.wc))

    def testMercurialWithShareAndBundle(self):
        # First create the bundle
        bundle = os.path.join(self.tmpdir, 'bundle')
        run_cmd(['hg', 'bundle', '-a', bundle], cwd=self.repodir)

        # Create a commit
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)

        # Wrap unbundle so we can tell if it got called
        orig_unbundle = unbundle
        try:
            called = []

            def new_unbundle(*args, **kwargs):
                called.append(True)
                return orig_unbundle(*args, **kwargs)
            hg.unbundle = new_unbundle

            shareBase = os.path.join(self.tmpdir, 'share')
            sharerepo = os.path.join(shareBase, self.repodir.lstrip("/"))
            os.mkdir(shareBase)
            mercurial(
                self.repodir, self.wc, shareBase=shareBase, bundles=[bundle])

            self.assertEquals(called, [True])
            self.assertEquals(
                getRevisions(self.repodir), getRevisions(self.wc))
            self.assertEquals(
                getRevisions(self.repodir), getRevisions(sharerepo))
        finally:
            hg.unbundle = orig_unbundle

    def testMercurialWithShareAndMirror(self):
        # First create the mirror
        mirror = os.path.join(self.tmpdir, 'repo2')
        clone(self.repodir, mirror)

        # Create a commit
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        run_cmd(['hg', 'add', 'test.txt'], cwd=self.repodir)
        run_cmd(['hg', 'commit', '-m', 'adding changeset'], cwd=self.repodir)

        shareBase = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(shareBase, self.repodir.lstrip("/"))
        os.mkdir(shareBase)
        mercurial(self.repodir, self.wc, shareBase=shareBase, mirrors=[mirror])

        # Since we used the mirror, we should be missing a commit
        self.assertNotEquals(getRevisions(self.repodir), getRevisions(self.wc))
        self.assertNotEquals(
            getRevisions(self.repodir), getRevisions(sharerepo))
        self.assertEquals(getRevisions(mirror), getRevisions(self.wc))

    def testMercurialByRevWithShareAndMirror(self):
        # First create the mirror
        mirror = os.path.join(self.tmpdir, 'repo2')
        clone(self.repodir, mirror)

        shareBase = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(shareBase, self.repodir.lstrip("/"))
        os.mkdir(shareBase)
        mercurial(self.repodir, self.wc, shareBase=shareBase, mirrors=[
                  mirror], clone_by_rev=True, revision=self.revisions[-1])

        # We should only have the one revision
        self.assertEquals(getRevisions(sharerepo), self.revisions[-1:])
        self.assertEquals(getRevisions(self.wc), self.revisions[-1:])

    def testMercurialSkipPull(self):
        # Clone once into our working copy
        mercurial(self.repodir, self.wc)

        # The second clone should avoid calling pull()
        with patch('util.hg.pull') as patched_pull:
            mercurial(self.repodir, self.wc, revision=self.revisions[-1])
            self.assertEquals(patched_pull.call_count, 0)

    def testAdjustPaths(self):
        mercurial(self.repodir, self.wc)

        # Make sure our default path is correct
        self.assertEquals(path(self.wc), self.repodir)

        # Add a comment, make sure it's still there if we don't change
        # anything
        hgrc = os.path.join(self.wc, '.hg', 'hgrc')
        open(hgrc, 'a').write("# Hello world")
        adjust_paths(self.wc, default=self.repodir)
        self.assert_("Hello world" in open(hgrc).read())

        # Add a path, and the comment goes away
        adjust_paths(self.wc, test=self.repodir)
        self.assert_("Hello world" not in open(hgrc).read())

        # Make sure our paths are correct
        self.assertEquals(path(self.wc), self.repodir)
        self.assertEquals(path(self.wc, 'test'), self.repodir)

    def testOutofSyncMirrorFailingMaster(self):
        # First create the mirror
        mirror = os.path.join(self.tmpdir, 'repo2')
        clone(self.repodir, mirror)

        shareBase = os.path.join(self.tmpdir, 'share')
        os.mkdir(shareBase)
        mercurial(self.repodir, self.wc, shareBase=shareBase, mirrors=[mirror])

        # Create a bundle
        bundle = os.path.join(self.tmpdir, 'bundle')
        run_cmd(['hg', 'bundle', '-a', bundle], cwd=self.repodir)

        # Move our repodir out of the way so that pulling/cloning from it fails
        os.rename(self.repodir, self.repodir + "-bad")

        # Try and update to a non-existent revision using our mirror and
        # bundle, with the master failing. We should fail
        self.assertRaises(subprocess.CalledProcessError, mercurial,
                          self.repodir, self.wc, shareBase=shareBase,
                          mirrors=[mirror], bundles=[bundle],
                          revision="1234567890")

    def testCommit(self):
        newfile = os.path.join(self.repodir, 'newfile')
        touch(newfile)
        run_cmd(['hg', 'add', 'newfile'], cwd=self.repodir)
        rev = commit(self.repodir, user='unittest', msg='gooooood')
        info = getRevInfo(self.repodir, rev)
        self.assertEquals(info['msg'], 'gooooood')
        # can't test for user, because it depends on local hg configs.

    def testCommitWithUser(self):
        newfile = os.path.join(self.repodir, 'newfile')
        touch(newfile)
        run_cmd(['hg', 'add', 'newfile'], cwd=self.repodir)
        rev = commit(self.repodir, user='unittest', msg='new stuff!')
        info = getRevInfo(self.repodir, rev)
        self.assertEquals(info['user'], 'unittest')
        self.assertEquals(info['msg'], 'new stuff!')

    def testTag(self):
        tag(self.repodir, ['test_tag'])
        self.assertTrue('test_tag' in getTags(self.repodir))

    def testMultitag(self):
        tag(self.repodir, ['tag1', 'tag2'])
        tags = getTags(self.repodir)
        self.assertTrue('tag1' in tags)
        self.assertTrue('tag2' in tags)

    def testMultitagSet(self):
        tag(self.repodir, set(['tag1', 'tag5']))
        tags = getTags(self.repodir)
        self.assertTrue('tag1' in tags)
        self.assertTrue('tag5' in tags)

    def testTagWithMsg(self):
        rev = tag(self.repodir, ['tag'], msg='I made a tag!')
        info = getRevInfo(self.repodir, rev)
        self.assertEquals('I made a tag!', info['msg'])

    def testTagWithUser(self):
        rev = tag(self.repodir, ['taggy'], user='tagger')
        info = getRevInfo(self.repodir, rev)
        self.assertEquals('tagger', info['user'])

    def testTagWithRevision(self):
        tag(self.repodir, ['tag'], rev='1')
        info = getRevInfo(self.repodir, '1')
        self.assertEquals(['tag'], info['tags'])

    def testMultitagWithRevision(self):
        tag(self.repodir, ['tag1', 'tag2'], rev='1')
        info = getRevInfo(self.repodir, '1')
        self.assertEquals(['tag1', 'tag2'], info['tags'])

    def testTagFailsIfExists(self):
        run_cmd(['hg', 'tag', '-R', self.repodir, 'tagg'])
        self.assertRaises(
            subprocess.CalledProcessError, tag, self.repodir, 'tagg')

    def testForcedTag(self):
        run_cmd(['hg', 'tag', '-R', self.repodir, 'tag'])
        tag(self.repodir, ['tag'], force=True)
        self.assertTrue('tag' in getTags(self.repodir))

    def testFailingBackend(self):
        # Test that remote failures get retried
        num_calls = [0]

        def _my_get_hg_output(cmd, **kwargs):
            num_calls[0] += 1
            if cmd[0] == 'clone':
                e = subprocess.CalledProcessError(-1, cmd)
                e.output = "error: Name or service not known"
                raise e
            else:
                return get_hg_output(cmd, **kwargs)

        # Two retries are forced in setUp() above
        with patch('util.hg.get_hg_output', new=_my_get_hg_output):
            self.assertRaises(subprocess.CalledProcessError,
                              clone, "http://nxdomain.nxnx", self.wc)
            self.assertEquals(num_calls, [2])

    @patch("util.commands.get_output")
    def testGetHgOutputAlwaysLogsException(self, get_output):
        get_output.side_effect = subprocess.CalledProcessError(255, "kaboom!")

        with patch("util.hg.log") as mocked_log:
            try:
                get_hg_output(["status"])
                self.fail("CalledProcessError not raised")
            except subprocess.CalledProcessError:
                self.assertTrue(mocked_log.exception.called, "log.exception not called")

    def testHasRev(self):
        self.assertTrue(has_rev(self.repodir, self.revisions[0]))
        self.assertFalse(has_rev(self.repodir, self.revisions[0] + 'g'))
