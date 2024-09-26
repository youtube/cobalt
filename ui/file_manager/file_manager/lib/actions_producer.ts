// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseAction} from './base_store.js';

/**
 * ActionsProducer is a generator function that yields and/or returns Actions.
 *
 * It's a function that return a AsyncGenerator.
 *
 * @template T: Is the type of the yielded/returned action.
 * @template Args: It's the inferred types of the arguments in the function.
 *
 * Example of an ActionsProducer:
 *   async function* doSomething(): ActionsProducerGen<SomeAction> {
 *     yield {type: 'some-action', status: 'STARTING'};
 *     // Call Some API.
 *     const result = await someApi();
 *     yield {type: 'some-action', status: 'SUCCESS', result};
 *   }
 */
export type ActionsProducer<T extends BaseAction, Args extends any[]> =
    (...args: Args) => ActionsProducerGen<T>;

/**
 * This is the type of the generator that is returned by the ActionsProducer.
 *
 * This is used to enforce the type for the yield.
 * The return should be always void, because consuming the generator using `for
 * await()` doesn't consume the value from the return.
 *
 * Yielding `undefined` or a bare yield like `yield;` gives the concurrency
 * model a chance to interrupt the ActionsProducer, when it's invalidated.
 * @template T the type for the action yielded by the ActionsProducer.
 */
export type ActionsProducerGen<T> = AsyncGenerator<void|T, void>;

/**
 * Exception used to stop ActionsProducer when they're no longer valid.
 *
 * The concurrency model function uses this exception to force the
 * ActionsProducer to stop.
 */
export class ConcurrentActionInvalidatedError extends Error {}

/** Helper to distinguish the Action from a ActionsProducer.  */
export function isActionsProducer(
    value: BaseAction|
    ActionsProducerGen<BaseAction>): value is ActionsProducerGen<BaseAction> {
  return (
      (value as ActionsProducerGen<BaseAction>).next !== undefined &&
      (value as ActionsProducerGen<BaseAction>).throw !== undefined);
}
