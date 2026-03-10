package dev.cobalt.util.startupguardexceptions;

/**
 * Custom runtime exception for startup-related guard failures.
 */
public class StartupGuardException45 extends RuntimeException {

    // Default constructor
    public StartupGuardException45() {
        super();
    }

    // Constructor that accepts a custom error message
    public StartupGuardException45(String message) {
        super(message);
    }

    // Constructor that accepts a message and a cause (for exception chaining)
    public StartupGuardException45(String message, Throwable cause) {
        super(message, cause);
    }

    // Constructor that accepts only a cause
    public StartupGuardException45(Throwable cause) {
        super(cause);
    }
}
