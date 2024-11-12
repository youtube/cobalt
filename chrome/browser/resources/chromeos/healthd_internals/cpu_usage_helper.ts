// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {HealthdApiCpuExecutionTimeUserHz, HealthdApiCpuResult} from './externs.js';

// The CPU usage info since last snapshot.
export interface CpuUsage {
  // The usage in percentage.
  usagePercentage: number;
  // The amount of time the CPU is running in user space.
  userTime: number;
  // The amount of time the CPU is running in kernel space.
  systemTime: number;
  // The amount of time the CPU is not used.
  idleTime: number;
}

/**
 * Helper class to calculate CPU usage.
 */
export class CpuUsageHelper {
  // Last execution time for logical CPUs. The index for the first dimension is
  // physical CPU ID and the index for the second dimension is logical CPU ID.
  private lastExecutionTime: HealthdApiCpuExecutionTimeUserHz[][] = [];

  /**
   * Calculate the CPU usage from execution time.
   *
   * @returns - CPU usage for each logical CPU, or null when the value can not
   *            be determined. The index for the first dimension is physical CPU
   *            ID and the index for the second dimension is logical CPU ID.
   *
   *            Return null if the last execution time is not found.
   */
  getCpuUsage(cpu: HealthdApiCpuResult): (CpuUsage|null)[][]|null {
    if (this.lastExecutionTime.length === 0) {
      for (const [physicalCpuId, physicalCpu] of cpu.physicalCpus.entries()) {
        this.lastExecutionTime[physicalCpuId] = [];
        for (const logicalCpu of physicalCpu.logicalCpus) {
          this.lastExecutionTime[physicalCpuId].push(logicalCpu.executionTime);
        }
      }
      return null;
    }

    const output: (CpuUsage|null)[][] = [];
    for (const [physicalCpuId, physicalCpu] of cpu.physicalCpus.entries()) {
      output[physicalCpuId] = [];
      for (const [logicalCpuId, logicalCpu] of physicalCpu.logicalCpus
               .entries()) {
        const lastExecTime: HealthdApiCpuExecutionTimeUserHz|undefined =
            this.lastExecutionTime[physicalCpuId][logicalCpuId];
        assert(lastExecTime !== undefined);

        output[physicalCpuId].push(
            this.getLogicalCpuUsage(logicalCpu.executionTime, lastExecTime!));
        this.lastExecutionTime[physicalCpuId][logicalCpuId] =
            logicalCpu.executionTime;
      }
    }
    return output;
  }

  private getLogicalCpuUsage(
      currentExecTime: HealthdApiCpuExecutionTimeUserHz,
      lastExecTime: HealthdApiCpuExecutionTimeUserHz,
      ): CpuUsage|null {
    const user: number|null = this.getCpuTimeDiff(
        parseInt(currentExecTime.user), parseInt(lastExecTime.user));
    const system: number|null = this.getCpuTimeDiff(
        parseInt(currentExecTime.system), parseInt(lastExecTime.system));
    const idle: number|null = this.getCpuTimeDiff(
        parseInt(currentExecTime.idle), parseInt(lastExecTime.idle));
    if (user === null || system === null || idle === null) {
      return null;
    }
    const total: number = user + system + idle;
    if (total === 0) {
      return null
    }
    return {
      usagePercentage: (user + system) / total * 100,
      userTime: user,
      systemTime: system,
      idleTime: idle,
    };
  }

  private getCpuTimeDiff(current: number, last: number): number|null {
    if (current < last) {
      // The increment is negative when the execution time counter exceeds
      // maximum value.
      return null;
    }
    return current - last;
  }
}
