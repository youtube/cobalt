type ObservableCallback<T> = (next: T, prev: T) => void;

/**
 * An observable value. Observers can attach a callback that gets called when
 * the value changes.
 */
export class Observable<T> {
  /** The actual value. */
  private value: T;

  /** Callbacks to trigger when the value changes. */
  private callbacks: Array<ObservableCallback<T>> = [];

  constructor(initValue: T) {
    this.value = initValue;
  }

  /**
   * Sets a new value and triggers the observer callbacks if the value changes.
   */
  set(value: T) {
    if (this.value === value) {
      return;
    }

    const oldValue = value;
    this.value = value;
    for (const callback of this.callbacks) {
      callback(this.value, oldValue);
    }
  }

  get(): T {
    return this.value;
  }

  /**
   * Triggers the callback on value changes.
   * @return The unsubscriber.
   */
  observe(callback: ObservableCallback<T>): () => void {
    this.callbacks.push(callback);
    return () => {
      const index = this.callbacks.indexOf(callback);
      if (index !== -1) {
        this.callbacks.splice(index, 1);
      }
    }
  }
}
