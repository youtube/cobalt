#!/usr/bin/env python
"""%prog

Iterates over a series of commit logs generated from hg merge operations,
collating bug numbers, and generating links to all commits. Formats the
output for insertion into the Maintenance wiki:

https://wiki.mozilla.org/ReleaseEngineering/Maintenance
"""

import glob
import os
import re
import requests

found_commits = {}
unique_bugs = {}

def get_revision_url_for_tag(repo_basedir, repo_name, tag='production'):
    """Find the corresponding revision for a given tag in a local repo,
    and return a web URL for that revision.
    """
    base_hg_url = "https://hg.mozilla.org/build/%s/rev/" % repo_name
    try:
        from mercurial import hg, ui
        repo = hg.repository(ui.ui(), os.path.join(repo_basedir, repo_name))
        return base_hg_url + str(repo[tag])
    except:
        return base_hg_url + tag

def clean_commit_entry(entry):
    """Remove common preceding/trailing characters from commit messages.
    """
    return entry.rstrip(' \.\,\-\:\;').lstrip(' \.\,\-\:\;')

def collate_merge_previews(logdir):
    """Iterate over commit logs to create a data structure containing
    the commit information.
    """
    repo_p = re.compile('^(.*)_preview_changes\.txt$')
    changeset_p = re.compile('changeset:\s+\d+:(.*)$')
    summary_p = re.compile('summary:\s+(.*)$')
    bug_p = re.compile('[Bb]ug\s*(\d+)[\s\-\.\,]*(.*)$')
    review_p = re.compile('(.*)[\s\-\.\,]*(r=.*)$')
    for preview_file in glob.glob(os.path.join(logdir, '*_preview_changes.txt')):
        repo = ""
        m = repo_p.match(os.path.basename(preview_file))
        if m:
            repo = m.group(1)
        else:
            print "Error determining repo"

        revision = ""
        with open(preview_file) as f:
            for line in f:
                m = changeset_p.match(line)
                if m:
                    revision = m.group(1)
                    continue
                m = summary_p.match(line)
                bug_number = 'None'
                summary = 'None'
                review = 'None'
                if m:
                    commit_message = m.group(1)
                    bug_m = bug_p.match(commit_message)
                    if bug_m:
                        bug_number = bug_m.group(1)
                        if not unique_bugs.has_key(bug_number):
                            unique_bugs[bug_number] = 1
                        summary = bug_m.group(2)
                    else:
                        summary = commit_message
                    review_m = review_p.match(summary)
                    if review_m:
                        summary = review_m.group(1)
                        review = review_m.group(2)
                    if not found_commits.has_key(repo):
                        found_commits[repo] = {}
                    if not found_commits[repo].has_key(bug_number):
                        found_commits[repo][bug_number] = []
                    found_commits[repo][bug_number].append({'revision': revision,
                                                            'summary': clean_commit_entry(summary),
                                                            'review': clean_commit_entry(review)})

def process_results(update_bugzilla, wiki_markup_file, logdir):
    """Write our formatted commit data out to a file.
    """
    if not found_commits:
        print "  * No commits found; nothing to do"
        return

    if wiki_markup_file:
        f = open(wiki_markup_file,"w")
        f.write('<div style="border: thin grey solid; background-color: lightgrey; float: right; text-align: right; text-size: 80%; padding-left: 5px; padding-right: 5px;">\n')
        f.write('[https://bugzilla.mozilla.org/buglist.cgi?bug_id=' + \
                ','.join(sorted(unique_bugs)) + \
                '&query_format=advanced&order=bug_status%2Cbug_id&tweak=1 ' +\
                'View list in Bugzilla]\n')
        f.write('</div>\n')
    for repo in sorted(found_commits):
        if wiki_markup_file:
            f.write('[https://hg.mozilla.org/build/%s %s]\n' % (repo, repo))
        for bug_number in sorted(found_commits[repo]):
            bug_link = '{{bug|%s}}' % bug_number
            if bug_number == 'None':
                bug_link = 'No bug'
            for commit in found_commits[repo][bug_number]:
                hg_changeset_url = 'https://hg.mozilla.org/build/%s/rev/%s' % (repo,
                                                                               commit['revision'])
                if wiki_markup_file:
                    review_display = ' - %s' % commit['review']
                    if commit['review'] == 'None':
                        review_display = ''
                    f.write('* %s - %s%s ([%s %s])\n' % (bug_link,
                                                        commit['summary'],
                                                        review_display,
                                                        hg_changeset_url,
                                                        commit['revision']))
                if args.update_bugzilla:
                    comment = 'In production: %s' % hg_changeset_url
                    if repo == "mozharness":
                        comment = 'mozharness production tag moved to: %s' % get_revision_url_for_tag(logdir, 'mozharness', tag='production')
                    payload = {'comment': comment,
                               'login': os.environ['BUGZILLA_USERNAME'],
                               'password': os.environ['BUGZILLA_PASSWORD']}
                    requests.post('https://bugzilla.mozilla.org/rest/bug/%s/comment' % bug_number, data=payload)
                    print "  * curl --data-urlencode 'comment=In production: %s' --data-urlencode 'login=%s' --data-urlencode 'password=XXXXX' 'https://bugzilla.mozilla.org/rest/bug/%s/comment'" % (hg_changeset_url, os.environ['BUGZILLA_USERNAME'], bug_number)

    if wiki_markup_file:
        f.close()

if __name__ == '__main__':
    import argparse
    import sys

    parser = argparse.ArgumentParser(description='Collate and format commit messages for Maintenance wiki.')
    parser.add_argument("-l", "--logdir", dest="logdir", default='.',
                        help="directory containing merge preview output")
    parser.add_argument("-b", "--update-bugzilla", action="store_true", dest="update_bugzilla", help="post comments on landed bugs in Bugzilla")
    parser.add_argument("-w", "--wiki-markup-file", action="store", dest="wiki_markup_file", help="file to write wiki markup to", default=None)

    args = parser.parse_args()

    if not os.path.isdir(args.logdir):
        print "ERROR: Log directory specified ('%s') does not exist. Exiting..." % args.logdir
        sys.exit(1)

    collate_merge_previews(args.logdir)
    process_results(update_bugzilla=args.update_bugzilla,
                    wiki_markup_file=args.wiki_markup_file,
                    logdir=args.logdir)
