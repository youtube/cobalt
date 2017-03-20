import unittest
import tempfile
import shutil
import os
import subprocess
from util.commands import run_cmd, get_output

import util.git as git
from util.file import touch


def getRevisions(dest, branches=None):
    retval = []
    cmd = ['git', 'log', '--pretty=oneline']
    if branches:
        cmd.extend(branches)
    for line in get_output(cmd, cwd=dest).split('\n'):
        line = line.strip()
        if not line:
            continue
        rev, ref = line.split(" ", 1)
        rev = rev.strip()
        if not rev:
            continue
        if rev not in retval:
            retval.append(rev)
    retval.reverse()
    return retval


class TestGitFunctions(unittest.TestCase):
    def test_get_repo_name(self):
        self.assertEquals(git.get_repo_name("https://git.mozilla.org/releases/gecko.git"),
                          "git.mozilla.org/releases%2Fgecko.git")
        self.assertEquals(git.get_repo_name("git://git.mozilla.org/releases/gecko.git"),
                          "git.mozilla.org/releases%2Fgecko.git")
        self.assertEquals(git.get_repo_name("http://git.mozilla.org/releases/gecko.git"),
                          "git.mozilla.org/releases%2Fgecko.git")


class TestGit(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.repodir = os.path.join(self.tmpdir, 'repo')
        run_cmd(['%s/init_gitrepo.sh' % os.path.dirname(__file__),
                self.repodir])

        self.revisions = getRevisions(
            self.repodir, branches=['master', 'branch2'])
        self.wc = os.path.join(self.tmpdir, 'wc')
        self.pwd = os.getcwd()

        # Make sure we're not setting GIT_SHARE_BASE_DIR
        if 'GIT_SHARE_BASE_DIR' in os.environ:
            del os.environ['GIT_SHARE_BASE_DIR']

    def tearDown(self):
        shutil.rmtree(self.tmpdir)
        os.chdir(self.pwd)

    def testHasRevision(self):
        self.assertTrue(git.has_revision(self.repodir, self.revisions[0]))
        self.assertFalse(git.has_revision(self.repodir, "foooooooooooooooo"))

    def testClean(self):
        git.git(self.repodir, self.wc)
        try:
            f = open(os.path.join(self.wc, 'blaaah'), 'w')
            f.write('blah')
        finally:
            if not f.closed:
                f.close()
        git.clean(self.wc)
        self.assertFalse(os.path.exists(os.path.join(self.wc, 'blaaah')))

    def testCleanFail(self):
        self.assertRaises(subprocess.CalledProcessError, git.clean, '/tmp')

    def testClone(self):
        rev = git.clone(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)
        # This clones the master branch by default
        # This has the 1st and 3rd commit
        self.assertEquals(
            [self.revisions[0], self.revisions[2]], getRevisions(self.wc))
        self.assertEquals(sorted(os.listdir(self.wc)), ['.git'])

    def testCloneIntoNonEmptyDir(self):
        os.mkdir(self.wc)
        open(os.path.join(self.wc, 'test.txt'), 'w').write('hello')
        git.clone(self.repodir, self.wc, update_dest=False)
        self.failUnless(not os.path.exists(os.path.join(self.wc, 'test.txt')))

    def testCloneUpdate(self):
        rev = git.clone(self.repodir, self.wc, update_dest=True)
        self.assertEquals(rev, self.revisions[-1])

    def testCloneBranch(self):
        git.clone(self.repodir, self.wc, refname='branch2', update_dest=False)
        # This should have just the 1st and 2nd commit
        self.assertEquals(
            [self.revisions[0], self.revisions[1]], getRevisions(self.wc))
        # We should also have no working copy
        self.failUnless(not os.path.exists(os.path.join(self.wc, 'hello.txt')))

    def testUpdateRevision(self):
        rev = git.clone(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)

        rev = git.update(self.wc, revision=self.revisions[2])
        self.assertEquals(rev, self.revisions[2])

    def testUpdateBranch(self):
        rev = git.clone(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)

        rev = git.update(self.wc, refname='branch2')
        self.assertEquals(rev, self.revisions[1])

    def testUpdateBranchDefault(self):
        rev = git.clone(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)

        rev = git.update(self.wc)
        self.assertEquals(rev, self.revisions[2])

    def testFetchBranch(self):
        # Clone just the main branch
        git.clone(self.repodir, self.wc, refname='master', update_dest=False)
        self.assertEquals(
            [self.revisions[0], self.revisions[2]], getRevisions(self.wc))

        # Now pull in branch2
        git.fetch(self.repodir, self.wc, refname='branch2')

        self.assertEquals(getRevisions(self.wc, branches=[
                          'origin/master', 'origin/branch2']), self.revisions)

    def testFetchAll(self):
        # Clone just the main branch
        git.clone(self.repodir, self.wc, update_dest=False)
        # Now pull in branch2
        git.fetch(self.repodir, self.wc, refname='branch2')

        # Change the original repo
        newfile = os.path.join(self.repodir, 'newfile')
        touch(newfile)
        run_cmd(['git', 'add', 'newfile'], cwd=self.repodir)
        run_cmd(['git', 'commit', '-q', '-m', 'add newfile'], cwd=self.repodir)

        # Now pull in everything from master branch
        git.fetch(self.repodir, self.wc, refname='master')

        for branch in 'master', 'branch2':
            self.assertEquals(getRevisions(self.wc, branches=['origin/%s' % branch]), getRevisions(self.repodir, branches=[branch]))

        # Make sure we actually changed something
        self.assertNotEqual(getRevisions(
            self.repodir, branches=['master', 'branch2']), self.revisions)

    def testGit(self):
        rev = git.git(self.repodir, self.wc)
        self.assertEquals(rev, self.revisions[-1])
        self.assertEquals(getRevisions(self.wc, branches=[
                          'origin/master', 'origin/branch2']), self.revisions)
        self.assertTrue(os.path.exists(os.path.join(self.wc, 'hello.txt')))
        self.assertTrue("Is this thing on" in open(
            os.path.join(self.wc, 'hello.txt')).read())

        # Switch branches, make sure we get the right content
        rev = git.git(self.repodir, self.wc, refname='branch2')
        self.assertEquals(rev, self.revisions[1])
        self.assertFalse("Is this thing on" in open(
            os.path.join(self.wc, 'hello.txt')).read())

    def testGitCleanDest(self):
        git.git(self.repodir, self.wc)
        try:
            f = open(os.path.join(self.wc, 'blaaah'), 'w')
            f.write('blah')
        finally:
            if not f.closed:
                f.close()
        git.git(self.repodir, self.wc, clean_dest=True)
        self.assertFalse(os.path.exists(os.path.join(self.wc, 'blaaah')))

    def testGitRev(self):
        rev = git.git(self.repodir, self.wc)
        self.assertEquals(rev, self.revisions[-1])

        # Update to a different rev
        rev = git.git(self.repodir, self.wc, revision=self.revisions[1])
        self.assertEquals(rev, self.revisions[1])

        # Make a new commit in repodir
        newfile = os.path.join(self.repodir, 'newfile')
        touch(newfile)
        run_cmd(['git', 'add', 'newfile'], cwd=self.repodir)
        run_cmd(['git', 'commit', '-q', '-m', 'add newfile'], cwd=self.repodir)
        new_rev = getRevisions(self.repodir)[-1]

        rev = git.git(self.repodir, self.wc, revision=new_rev)

        self.assertEquals(new_rev, rev)

    def testGitBranch(self):
        rev = git.git(self.repodir, self.wc, refname='branch2')
        self.assertEquals(rev, self.revisions[1])
        self.assertEquals(getRevisions(self.wc, branches=['origin/branch2']
                                       ), getRevisions(self.repodir, branches=['branch2']))
        self.assertTrue(os.path.exists(os.path.join(self.wc, 'hello.txt')))
        self.assertFalse("Is this thing on" in open(
            os.path.join(self.wc, 'hello.txt')).read())

        # Switch branches, make sure we get all the revisions
        rev = git.git(self.repodir, self.wc, refname='master')
        self.assertEquals(rev, self.revisions[-1])
        self.assertEquals(getRevisions(self.wc, branches=[
                          'origin/master', 'origin/branch2']), self.revisions)
        self.assertTrue("Is this thing on" in open(
            os.path.join(self.wc, 'hello.txt')).read())

    def testGitShare(self):
        shareBase = os.path.join(self.tmpdir, 'git-repos')
        rev = git.git(self.repodir, self.wc, shareBase=shareBase)
        shareDir = os.path.join(shareBase, git.get_repo_name(self.repodir))
        self.assertEquals(rev, self.revisions[-1])

        # We should see all the revisions
        revs = getRevisions(
            self.wc, branches=['origin/master', 'origin/branch2'])
        shared_revs = getRevisions(
            shareDir, branches=['origin/master', 'origin/branch2'])

        self.assertEquals(revs, shared_revs)
        self.assertEquals(revs, self.revisions)

        # Update to a different rev
        rev = git.git(self.repodir, self.wc,
                      revision=self.revisions[0], shareBase=shareBase)
        self.assertEquals(rev, self.revisions[0])
        self.assertFalse(os.path.exists(os.path.join(self.wc, 'newfile')))

        # Add a commit to the original repo
        newfile = os.path.join(self.repodir, 'newfile')
        touch(newfile)
        run_cmd(['git', 'add', 'newfile'], cwd=self.repodir)
        run_cmd(['git', 'commit', '-q', '-m', 'add newfile'], cwd=self.repodir)
        new_rev = getRevisions(self.repodir)[-1]

        # Update to the new rev
        rev = git.git(
            self.repodir, self.wc, revision=new_rev, shareBase=shareBase)
        self.assertEquals(rev, new_rev)
        self.assertTrue(os.path.exists(os.path.join(self.wc, 'newfile')))

    def testGitBadDest(self):
        # Create self.wc without .git
        os.makedirs(self.wc)
        self.assertFalse(git.is_git_repo(self.wc))

        rev = git.git(self.repodir, self.wc)
        self.assertEquals(rev, self.revisions[-1])
        self.assertTrue(os.path.exists(os.path.join(self.wc, 'hello.txt')))
        self.assertTrue("Is this thing on" in open(
            os.path.join(self.wc, 'hello.txt')).read())

    def testGitBadShare(self):
        shareBase = os.path.join(self.tmpdir, 'git-repos')
        open(shareBase, 'w').write("hello!")
        rev = git.git(self.repodir, self.wc, shareBase=shareBase)
        self.assertEquals(rev, self.revisions[-1])

    def testGitMirrors(self):
        # Create a bad mirror and a good mirror
        mirror1 = os.path.join(self.tmpdir, 'mirror1')
        mirror2 = os.path.join(self.tmpdir, 'mirror2')
        git.git(self.repodir, mirror2, update_dest=False)

        rev = git.git(self.repodir, self.wc, mirrors=[mirror1, mirror2])
        self.assertEquals(rev, self.revisions[-1])

        # Add a commit to the mirror
        newfile = os.path.join(mirror2, 'newfile')
        touch(newfile)
        run_cmd(['git', 'add', 'newfile'], cwd=mirror2)
        run_cmd(['git', 'commit', '-q', '-m', 'add newfile'], cwd=mirror2)
        new_rev = getRevisions(mirror2)[-1]

        # Now clone using the mirror. We should get the new commit that's in
        # the mirror
        rev = git.git(self.repodir, self.wc, mirrors=[mirror1, mirror2])
        self.assertEquals(rev, new_rev)

    def testGitBadRev(self):
        rev = git.git(self.repodir, self.wc)
        self.assertEquals(rev, self.revisions[-1])

        # Now try and update to a bad revision
        self.assertRaises(subprocess.CalledProcessError, git.git,
                          self.repodir, self.wc, revision="badrev")

    def testGitTag(self):
        rev = git.git(self.repodir, self.wc)
        self.assertEquals(rev, self.revisions[-1])

        # Now try and update to a tag
        rev = git.git(self.repodir, self.wc, revision="TAG1")
        self.assertEquals(rev, self.revisions[0])

# TODO: Test blowing away shared repo and then updating working copy
