package dev.cobalt.util;

import androidx.annotation.Nullable;
import java.util.concurrent.CountDownLatch;

/**
 * A generic, thread-safe holder for a single object that allows threads to block
 * until the object is provided.
 *
 * <p>This is useful for asynchronous initialization where one thread needs to wait
 * for a result from another. The {@link #set(T)} method should ideally be called only once.
 *
 * @param <T> The type of the object to hold.
 */
public class BlockingHolder<T> {

    /**
     * A latch that starts at 1 and is counted down to 0 when {@link #set(T)} is called.
     * Threads calling {@link #getBlocking()} will wait on this latch.
     */
    private final CountDownLatch latch = new CountDownLatch(1);

    /**
     * The object being held. It is marked as 'volatile' to ensure that writes to this
     * variable are immediately visible to other threads once the latch is released.
     */
    private volatile T instance;

    /**
     * Sets the object instance. This will release any threads that are currently
     * waiting in {@link #getBlocking()}.
     *
     * <p>If this method is called more than once, it will overwrite the instance, but it
     * will not affect threads that have already been released.
     *
     * @param instance The object instance to hold.
     */
    public void set(@Nullable T instance) {
        this.instance = instance;
        // This decrements the latch count to 0, releasing all waiting threads.
        latch.countDown();
    }

    /**
     * Blocks the calling thread until the value has been set, then returns the value.
     * <p>
     * <strong>DANGER:</strong> Do not call this from the main/UI thread, as it will
     * freeze your application and likely cause an "Application Not Responding" (ANR) error.
     * This method is intended for use on background threads.
     *
     * @return The instance held by this holder.
     * @throws InterruptedException if the current thread is interrupted while waiting.
     */
    public T getBlocking() throws InterruptedException {
        // This causes the current thread to wait until the latch has counted down to 0.
        latch.await();
        return instance;
    }

    /**
     * Returns the instance immediately without blocking.
     *
     * @return The instance if it has been set, or {@code null} otherwise.
     */
    @Nullable
    public T get() {
        return instance;
    }

    /**
     * Checks if the value has been set yet.
     *
     * @return {@code true} if the value has been set, {@code false} otherwise.
     */
    public boolean isSet() {
        // The latch count is 0 if and only if countDown() has been called.
        return latch.getCount() == 0;
    }
}
