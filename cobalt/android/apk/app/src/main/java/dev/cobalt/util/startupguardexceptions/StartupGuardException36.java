package dev.cobalt.util.startupguardexceptions;

/**
 * Custom runtime exception for startup-related guard failures.
 */
public class StartupGuardException36 extends RuntimeException {

    // Default constructor
    public StartupGuardException36() {
        super();
    }

    // Constructor that accepts a custom error message
    public StartupGuardException36(String message) {
        super(message);
    }

    // Constructor that accepts a message and a cause (for exception chaining)
    public StartupGuardException36(String message, Throwable cause) {
        super(message, cause);
    }

    // Constructor that accepts only a cause
    public StartupGuardException36(Throwable cause) {
        super(cause);
    }
}
