package dev.cobalt.util.startupguardexceptions;

/**
 * Custom runtime exception for startup-related guard failures.
 */
public class StartupGuardException61 extends RuntimeException {

    // Default constructor
    public StartupGuardException61() {
        super();
    }

    // Constructor that accepts a custom error message
    public StartupGuardException61(String message) {
        super(message);
    }

    // Constructor that accepts a message and a cause (for exception chaining)
    public StartupGuardException61(String message, Throwable cause) {
        super(message, cause);
    }

    // Constructor that accepts only a cause
    public StartupGuardException61(Throwable cause) {
        super(cause);
    }
}
