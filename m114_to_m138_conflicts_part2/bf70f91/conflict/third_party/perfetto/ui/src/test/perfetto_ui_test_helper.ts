// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

<<<<<<< HEAD
import {
  expect,
  Locator,
  Page,
  PageAssertionsToHaveScreenshotOptions,
} from '@playwright/test';
import fs from 'fs';
import path from 'path';
import {IdleDetectorWindow} from '../frontend/idle_detector_interface';
import {assertExists} from '../base/logging';
import {Size2D} from '../base/geom';

export class PerfettoTestHelper {
  private cachedSidebarSize?: Size2D;

  constructor(readonly page: Page) {}

  resetFocus(): Promise<void> {
    return this.page.click('.sidebar img.brand');
  }

  async sidebarSize(): Promise<Size2D> {
    if (this.cachedSidebarSize === undefined) {
      const size = await this.page.locator('main > .sidebar').boundingBox();
      this.cachedSidebarSize = assertExists(size);
    }
    return this.cachedSidebarSize;
  }

  async navigate(fragment: string): Promise<void> {
    await this.page.goto('/?testing=1' + fragment);
    await this.waitForPerfettoIdle();
    await this.page.click('body');
  }

  async openTraceFile(traceName: string, args?: {}): Promise<void> {
    args = {testing: '1', ...args};
    const qs = Object.entries(args ?? {})
      .map(([k, v]) => `${k}=${v}`)
      .join('&');
    await this.page.goto('/?' + qs);
    const file = await this.page.waitForSelector('input.trace_file', {
      state: 'attached',
    });
    await this.page.evaluate(() =>
      localStorage.setItem('dismissedPanningHint', 'true'),
    );
    const tracePath = this.getTestTracePath(traceName);
    assertExists(file).setInputFiles(tracePath);
    await this.waitForPerfettoIdle();
    await this.page.mouse.move(0, 0);
  }

  waitForPerfettoIdle(idleHysteresisMs?: number): Promise<void> {
    return this.page.evaluate(
      async (ms) =>
        (window as {} as IdleDetectorWindow).waitForPerfettoIdle(ms),
      idleHysteresisMs,
    );
  }

  async waitForIdleAndScreenshot(
    screenshotName: string,
    opts?: PageAssertionsToHaveScreenshotOptions,
  ) {
    await this.page.mouse.move(0, 0); // Move mouse out of the way.
    await this.waitForPerfettoIdle();
    await expect.soft(this.page).toHaveScreenshot(screenshotName, opts);
  }

  async toggleTrackGroup(locator: Locator) {
    await locator.locator('.pf-track__shell').first().click();
    await this.waitForPerfettoIdle();
  }

  locateTrack(name: string, trackGroup?: Locator): Locator {
    return (trackGroup ?? this.page).locator(`.pf-track[ref="${name}"]`);
  }

  pinTrackUsingShellBtn(track: Locator) {
    track.locator('button[title="Pin to top"]').click({force: true});
  }

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  async runCommand(cmdId: string, ...args: any[]) {
    await this.page.evaluate(
      (arg) => self.app.commands.runCommand(arg.cmdId, ...arg.args),
      {cmdId, args},
    );
  }

  async searchSlice(name: string) {
    const omnibox = this.page.locator('input[ref=omnibox]');
    await omnibox.focus();
    await omnibox.fill(name);
    await this.waitForPerfettoIdle();
    await omnibox.press('Enter');
    await this.waitForPerfettoIdle();
  }

  getTestTracePath(fname: string): string {
    const parts = ['test', 'data', fname];
    if (process.cwd().endsWith('/ui')) {
      parts.unshift('..');
    }
    const fPath = path.join(...parts);
    if (!fs.existsSync(fPath)) {
      throw new Error(`Could not locate file ${fPath}, cwd=${process.cwd()}`);
    }
    return fPath;
  }

  async clickMenuItem(text: string | RegExp) {
    await this.page
      .locator('.pf-popup-content .pf-menu-item', {hasText: text})
      .click();
  }

  async switchToTab(text: string | RegExp) {
    await this.page
      .locator('.pf-tab-handle .pf-tab-handle__tab', {hasText: text})
      .click();
  }
=======
import fs from 'fs';
import net from 'net';
import path from 'path';
import pixelmatch from 'pixelmatch';
import {PNG} from 'pngjs';
import {Page} from 'puppeteer';

// These constants have been hand selected by comparing the diffs of screenshots
// between Linux on Mac. Unfortunately font-rendering is platform-specific.
// Even though we force the same antialiasing and hinting settings, some minimal
// differences exist.
const DIFF_PER_PIXEL_THRESHOLD = 0.35;
const DIFF_MAX_PIXELS = 50;

// Waits for the Perfetto UI to be quiescent, using a union of heuristics:
// - Check that the progress bar is not animating.
// - Check that the omnibox is not showing a message.
// - Check that no redraws are pending in our RAF scheduler.
// - Check that all the above is satisfied for |minIdleMs| consecutive ms.
export async function waitForPerfettoIdle(page: Page, minIdleMs?: number) {
  minIdleMs = minIdleMs || 3000;
  const tickMs = 250;
  const timeoutMs = 60000;
  const minIdleTicks = Math.ceil(minIdleMs / tickMs);
  const timeoutTicks = Math.ceil(timeoutMs / tickMs);
  let consecutiveIdleTicks = 0;
  let reasons: string[] = [];
  for (let ticks = 0; ticks < timeoutTicks; ticks++) {
    await new Promise((r) => setTimeout(r, tickMs));
    const isShowingMsg = !!(await page.$('.omnibox.message-mode'));
    const isShowingAnim = !!(await page.$('.progress.progress-anim'));
    const hasPendingRedraws =
        await (
            await page.evaluateHandle('globals.rafScheduler.hasPendingRedraws'))
            .jsonValue();

    if (isShowingAnim || isShowingMsg || hasPendingRedraws) {
      consecutiveIdleTicks = 0;
      reasons = [];
      if (isShowingAnim) {
        reasons.push('showing progress animation');
      }
      if (isShowingMsg) {
        reasons.push('showing omnibox message');
      }
      if (hasPendingRedraws) {
        reasons.push('has pending redraws');
      }
      continue;
    }
    if (++consecutiveIdleTicks >= minIdleTicks) {
      return;
    }
  }
  throw new Error(
      `waitForPerfettoIdle() failed. Did not reach idle after ${
          timeoutMs} ms. ` +
      `Reasons not considered idle: ${reasons.join(', ')}`);
}

export function getTestTracePath(fname: string): string {
  const fPath = path.join('test', 'data', fname);
  if (!fs.existsSync(fPath)) {
    throw new Error('Could not locate trace file ' + fPath);
  }
  return fPath;
}

export async function compareScreenshots(
    reportPath: string, actualFilename: string, expectedFilename: string) {
  if (!fs.existsSync(expectedFilename)) {
    throw new Error(
        `Could not find ${expectedFilename}. Run wih REBASELINE=1.`);
  }
  const actualImg = PNG.sync.read(fs.readFileSync(actualFilename));
  const expectedImg = PNG.sync.read(fs.readFileSync(expectedFilename));
  const {width, height} = actualImg;
  expect(width).toEqual(expectedImg.width);
  expect(height).toEqual(expectedImg.height);
  const diffPng = new PNG({width, height});
  const diff = await pixelmatch(
      actualImg.data, expectedImg.data, diffPng.data, width, height, {
        threshold: DIFF_PER_PIXEL_THRESHOLD,
      });
  if (diff > DIFF_MAX_PIXELS) {
    const diffFilename = actualFilename.replace('.png', '-diff.png');
    fs.writeFileSync(diffFilename, PNG.sync.write(diffPng));
    fs.appendFileSync(
        reportPath,
        `${path.basename(actualFilename)};${path.basename(diffFilename)}\n`);
    fail(`Diff test failed on ${diffFilename}, delta: ${diff} pixels`);
  }
  return diff;
}


// If the user has a trace_processor_shell --httpd instance open, bail out,
// as that will invalidate the test loading different data.
export async function failIfTraceProcessorHttpdIsActive() {
  return new Promise<void>((resolve, reject) => {
    const client = new net.Socket();
    client.connect(9001, '127.0.0.1', () => {
      const err = 'trace_processor_shell --httpd detected on port 9001. ' +
          'Bailing out as it interferes with the tests. ' +
          'Please kill that and run the test again.';
      console.error(err);
      client.destroy();
      reject(err);
    });
    client.on('error', (e: {code: string}) => {
      expect(e.code).toBe('ECONNREFUSED');
      resolve();
    });
    client.end();
  });
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}
