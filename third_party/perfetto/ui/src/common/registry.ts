// Copyright (C) 2018 The Android Open Source Project
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

export interface HasKind { kind: string; }

export class Registry<T> {
  private key: (t: T) => string;
  protected registry: Map<string, T>;

  static kindRegistry<T extends HasKind>(): Registry<T> {
    return new Registry<T>((t) => t.kind);
  }

  constructor(key: (t: T) => string) {
    this.registry = new Map<string, T>();
    this.key = key;
  }

  register(registrant: T) {
    const kind = this.key(registrant);
    if (this.registry.has(kind)) {
      throw new Error(`Registrant ${kind} already exists in the registry`);
    }
    this.registry.set(kind, registrant);
  }

  has(kind: string): boolean {
    return this.registry.has(kind);
  }

  get(kind: string): T {
    const registrant = this.registry.get(kind);
    if (registrant === undefined) {
      throw new Error(`${kind} has not been registered.`);
    }
    return registrant;
  }

  // Support iteration: for (const foo of fooRegistry.values()) { ... }
  * values() {
    yield* this.registry.values();
  }

  unregisterAllForTesting(): void {
    this.registry.clear();
  }
}
