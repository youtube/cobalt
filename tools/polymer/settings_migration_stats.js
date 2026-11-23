// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Helper script to gather Settings Lit migration stats.
 *
 * The script can be directly invoked as follows
 *
 * /third_party/node/linux/node-linux-x64/bin/node \
 *   tools/polymer/settings_migration_stats.js \
 *   --revision=1466790 \
 *   --csv=tools/polymer/settings_migration_stats.csv
 *
 *   where 'revision' is only used when populating the output csv file.
 *
 * The script is primarily intended to be invoked for multiple revisions via the
 * parent script as follows
 *
 * ./third_party/node/linux/node-linux-x64/bin/node \
 *   tools/polymer/gather_per_revision_stats.js \
 *   --bug=393471368 \
 *   --known-revisions=tools/polymer/known_revisions.txt \
 *   --stats-script=tools/polymer/settings_migration_stats.js \
 *   --after="2026-06-09" \
 *   -- \
 *   '--csv=tools/polymer/progress.csv'
 */

import assert from 'node:assert';
import {execSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import {parseArgs} from 'node:util';

const DIRECTORIES = [
  'chrome/browser/resources/settings',
  'chrome/browser/resources/settings_shared',
];

// Helper to recursively list files
function getFiles(dir, fileList = []) {
  const files = fs.readdirSync(dir, {withFileTypes: true});
  for (const file of files) {
    const filePath = path.join(dir, file.name);
    if (file.isDirectory()) {
      getFiles(filePath, fileList);
    } else {
      fileList.push(filePath);
    }
  }
  return fileList;
}

// Map to store classification of TS files: path -> 'polymer' | 'lit' |
// 'neither'
const tsClassification = new Map();

function classifyFile(filePath) {
  const content = fs.readFileSync(filePath, 'utf-8');

  if (filePath.endsWith('.ts')) {
    const hasPolymerImport = content.includes('resources/polymer/');
    const hasLitImport = content.includes('resources/lit/v3_0/lit.rollup.js');

    if (hasPolymerImport) {
      return 'polymer';
    }

    if (hasLitImport) {
      return 'lit';
    }

    return 'neither';
  }

  if (filePath.endsWith('.css')) {
    if (content.includes('#type=style-lit')) {
      return 'lit';
    }
    if (content.includes('#type=style')) {
      return 'polymer';
    }
    return 'neither';
  }

  return 'neither';
}

function main() {
  const optionsConfig = {revision: {type: 'string'}, csv: {type: 'string'}};

  const {values} = parseArgs({options: optionsConfig, strict: true});

  const csvFile = values.csv;
  const revision = values.revision;
  const WORKSPACE_ROOT = process.cwd();

  const polymerFiles = [];
  const litFiles = [];
  const neutralFiles = [];

  assert.ok(revision, 'Error: --revision=<revision> is required.');
  assert.ok(csvFile, 'Error: --csv=<path> is required.');

  console.info(`Chromium Revision: ${revision}`);
  console.info(`Scanning in workspace: ${WORKSPACE_ROOT}`);

  const allFiles = [];
  for (const dir of DIRECTORIES) {
    const fullPath = path.join(WORKSPACE_ROOT, dir);
    if (fs.existsSync(fullPath)) {
      getFiles(fullPath, allFiles);
    } else {
      console.error(`Directory does not exist: ${fullPath}`);
    }
  }

  // First pass: Classify TS and CSS files
  for (const file of allFiles) {
    if (file.endsWith('.ts') || file.endsWith('.css')) {
      const classification = classifyFile(file);
      if (file.endsWith('.ts')) {
        tsClassification.set(file, classification);
      }

      const relativePath = path.relative(WORKSPACE_ROOT, file);
      if (classification === 'polymer') {
        polymerFiles.push(relativePath);
      } else if (classification === 'lit') {
        litFiles.push(relativePath);
      } else {
        neutralFiles.push(relativePath);
      }
    }
  }

  // Second pass: Classify HTML files
  for (const file of allFiles) {
    if (file.endsWith('.html')) {
      const relativePath = path.relative(WORKSPACE_ROOT, file);
      // Corresponding TS file
      const tsFile = file.slice(0, -5) + '.ts';

      if (fs.existsSync(tsFile)) {
        const classification = tsClassification.get(tsFile);
        if (classification === 'polymer') {
          polymerFiles.push(relativePath);
        } else if (classification === 'lit') {
          litFiles.push(relativePath);
        } else {
          neutralFiles.push(relativePath);
        }
      } else {
        neutralFiles.push(relativePath);
      }
    }
  }

  // Sort files for consistent output
  polymerFiles.sort();
  litFiles.sort();

  // Write outputs
  const polymerPath = path.join(WORKSPACE_ROOT, 'using_polymer.txt');
  fs.writeFileSync(polymerPath, polymerFiles.join('\n') + '\n');

  const litPath = path.join(WORKSPACE_ROOT, 'using_lit.txt');
  fs.writeFileSync(litPath, litFiles.join('\n') + '\n');

  const neutralPath = path.join(WORKSPACE_ROOT, 'using_neutral.txt');
  neutralFiles.sort();
  fs.writeFileSync(neutralPath, neutralFiles.join('\n') + '\n');

  const totalPolymer = polymerFiles.length;
  const totalLit = litFiles.length;
  const total = totalPolymer + totalLit;
  const migrationPercentage =
      total > 0 ? ((totalLit / total) * 100).toFixed(2) : '0.00';


  console.info('Scan completed.');
  console.info(`Polymer: ${totalPolymer}, Lit: ${totalLit}, Neutral: ${
      neutralFiles.length}, Stats: ${migrationPercentage}%`);

  console.info('Wrote file lists:');
  for (const file of [litPath, neutralPath, polymerPath]) {
    console.info(`  ${file}`);
  }

  const csvPath = path.resolve(csvFile);
  // Find the commit that matches the Cr-Commit-Position for the given revision,
  // and extract its commit date in ISO 8601 format.
  const dateStr =
      execSync(
          `git log -n 1 --grep="^Cr-Commit-Position: refs/heads/main@{#${
              revision}}" --format=%cI`,
          {cwd: WORKSPACE_ROOT, encoding: 'utf-8'})
          .trim();
  assert.ok(
      dateStr, `Error: Failed to get commit date for revision ${revision}`);

  const csvLine = `${dateStr},${revision},${neutralFiles.length},${
      totalPolymer},${totalLit}\n`;

  const writeHeader = !fs.existsSync(csvPath);
  if (writeHeader) {
    fs.writeFileSync(csvPath, 'date,revision,neutral,polymer,lit\n');
  }
  fs.appendFileSync(csvPath, csvLine);
  console.info('Appended stats to CSV:');
  console.info(`  ${csvPath}`);
}

main();
