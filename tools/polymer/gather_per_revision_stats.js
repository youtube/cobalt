// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper script to trigger another script across multiple
 * revisions of the code with the intention to gather stats across revisions.
 *
 * From the root of the repository execute:
 *
 * ./third_party/node/linux/node-linux-x64/bin/node \
 *    tools/polymer/gather_per_revision_stats.js \
 *    --bug=<bug number> \
 *    --known-revisions=/tmp/known_revisions.txt \
 *    --stats-script=<path to stats script> \
 *    --after="2026-06-09" \
 *    -- \
 *    '--flag1=foo --flag2=bar'
 *
 * Steps:
 *  1) Searches 'git log' for any references to the provided bug number. If
 *     --after is provided it only searches for commits after the given date.
 *     If a --dry-run flag is present, simply prints all revisions found and
 *     exits.
 *  2) Repeatedly checks out the repository at the revisions that are detected
 *     in step #1
 *  3) Invokes the --stats-script for each revision. Internally it makes a
 *     temporary copy of the script to ensure it exists even when checking out
 *     older revisions, which may predate the addition of the script to the
 *     repo.
 *  4) Restores the repository to its initial state.
 *  5) Updates the file specified by --known-revisions to avoid processing the
 *     same revisions the next time it is invoked.
 *
 *
 *  See docs in tools/polymer/settings_migration_stats.js for a fully working
 *  command example.
 */

import assert from 'node:assert';
import {execSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import {parseArgs} from 'node:util';

function runCommand(cmd, options = {}) {
  console.info(`Running command '${cmd}'`);
  try {
    const result = execSync(cmd, {encoding: 'utf-8', ...options});
    return result ? result.trim() : '';
  } catch (error) {
    console.error(`Error running command: ${cmd}`);
    if (error.status !== undefined && error.stderr) {
      console.error(`Stderr: ${error.stderr}`);
    } else {
      console.error(error);
    }
    throw error;
  }
}

function isRepoDirty() {
  const status = runCommand('git status --porcelain -uno');
  return status.length > 0;
}

function extractRevision(commitBody) {
  // Match Cr-Commit-Position only at the absolute beginning of a line.
  // Quoted positions (like in reverts) start with '>' and will be ignored.
  const match = commitBody.match(/^Cr-Commit-Position:.*@{#(\d+)}/m);
  return match ? match[1] : null;
}

function getMatchingCommits(bugId, after) {
  // Use extended regex to match "Bug: <bugId>" or similar
  let cmd =
      `git log -E --grep="[Bb]ug:.*${bugId}" --format="COMMIT_START%n%H%n%B"`;
  if (after) {
    cmd += ` --after="${after}"`;
  }

  const output = runCommand(cmd);

  if (!output) {
    return [];
  }

  const results = [];

  const commits = output.split('COMMIT_START\n').slice(1);
  for (const commit of commits) {
    const lines = commit.split('\n');
    const sha = lines[0].trim();
    const body = lines.slice(1).join('\n');
    const rev = extractRevision(body);
    assert.ok(
        rev !== null,
        `Could not find revision for commit ${sha.substring(0, 8)}`);
    const subject = lines[1] ? lines[1].trim() : '';
    results.push({rev, sha, subject});
  }

  return results;
}

function loadKnownRevisions(filePath) {
  if (fs.existsSync(filePath)) {
    const data = fs.readFileSync(filePath, 'utf-8');
    return new Set(data.split('\n').map(line => line.trim()).filter(Boolean));
  }

  return new Set();
}

function saveKnownRevisions(filePath, knownRevisionsSet) {
  const data = Array.from(knownRevisionsSet)
                   .sort((a, b) => {
                     return Number.parseInt(a) - Number.parseInt(b);
                   })
                   .join('\n');

  const dir = path.dirname(filePath);
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, {recursive: true});
  }
  fs.writeFileSync(filePath, data + '\n', 'utf-8');
}

function main() {
  const argv = process.argv.slice(2);
  const doubleDashIndex = argv.indexOf('--');
  const outerArgs =
      doubleDashIndex !== -1 ? argv.slice(0, doubleDashIndex) : argv;
  const innerArgs =
      doubleDashIndex !== -1 ? argv.slice(doubleDashIndex + 1) : [];

  const optionsConfig = {
    'bug': {type: 'string'},
    'known-revisions': {type: 'string'},
    'stats-script': {type: 'string'},
    'dry-run': {type: 'boolean'},
    'after': {type: 'string'}
  };

  const {values} =
      parseArgs({args: outerArgs, options: optionsConfig, strict: true});
  const forwardedArgs = innerArgs.join(' ');

  const bugId = values.bug;
  const knownRevisionsPath = values['known-revisions'];
  const statsScriptPath = values['stats-script'];
  const dryRun = !!values['dry-run'];
  const after = values.after;

  // Validation
  assert.ok(bugId, 'Error: --bug=<bug_id> is required.');
  assert.ok(knownRevisionsPath, 'Error: --known-revisions=<path> is required.');
  assert.ok(
      dryRun || statsScriptPath,
      'Error: --stats-script=<path> is required (unless --dry-run is specified).');
  assert.ok(!isRepoDirty(), 'Error: Repository has uncommitted changes.');

  const originalState = runCommand('git branch --show-current');

  const knownRevisions = loadKnownRevisions(knownRevisionsPath);
  const allCommits = getMatchingCommits(bugId, after);
  const newCommits = allCommits.filter(c => !knownRevisions.has(c.rev));
  newCommits.reverse();

  if (newCommits.length === 0) {
    console.info(`No new revisions found referencing bug ${bugId}`);
    return;
  }

  console.info(`Found ${newCommits.length} new revisions.`);
  for (let i = 0; i < newCommits.length; i++) {
    const c = newCommits[i];
    console.info(` ${i + 1}) Revision: ${c.rev} (SHA: ${
        c.sha.substring(0, 8)}) ${c.subject}`);
  }

  if (dryRun) {
    console.info('--dry-run was present, stopping early');
    console.info('DONE');
    return;
  }

  const processedSuccessfully = [];
  // Make a temporary copy of the stats script to ensure it remains available
  // when checking out older revisions where the script might not exist yet.
  const tempStatsScriptPath = path.join(
      path.dirname(statsScriptPath), `.temp_${path.basename(statsScriptPath)}`);
  console.info(
      `Creating temporary copy of stats script at ${tempStatsScriptPath}`);
  fs.copyFileSync(statsScriptPath, tempStatsScriptPath);

  try {
    for (let i = 0; i < newCommits.length; i++) {
      const c = newCommits[i];
      console.info(
          '============================================================');
      console.info(`[${i + 1}/${newCommits.length}] Processing revision ${
          c.rev} (commit ${c.sha.substring(0, 8)})`);
      console.info(c.subject);
      runCommand(`git checkout -q ${c.sha}`);
      console.info(
          '------------------------------------------------------------');

      try {
        const nodeBinary = './third_party/node/linux/node-linux-x64/bin/node';
        const extraArgs = forwardedArgs ? ` ${forwardedArgs}` : '';
        const cmd = `${nodeBinary} ${tempStatsScriptPath} --revision=${c.rev}${
            extraArgs}`;
        runCommand(cmd, {stdio: 'inherit'});
        processedSuccessfully.push(c.rev);
        console.info(
            '------------------------------------------------------------');
        console.info(`Successfully processed revision ${c.rev}`);
      } catch (e) {
        console.error(`Command failed for revision ${c.rev}. Stopping.`);
        break;
      }
    }
  } finally {
    runCommand(`git checkout -q ${originalState}`);

    if (fs.existsSync(tempStatsScriptPath)) {
      console.info(
          `Cleaning up temporary stats script: ${tempStatsScriptPath}`);
      fs.unlinkSync(tempStatsScriptPath);
    }

    if (processedSuccessfully.length > 0) {
      for (const rev of processedSuccessfully) {
        knownRevisions.add(rev);
      }
      saveKnownRevisions(knownRevisionsPath, knownRevisions);
    }
    console.info('DONE');
  }
}

main();
