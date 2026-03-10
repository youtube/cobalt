package dev.cobalt.util.startupguardexceptions;

/**
 * Custom runtime exception for startup-related guard failures.
 */
public class StartupGuardException59 extends RuntimeException {

    // Default constructor
    public StartupGuardException59() {
        super();
    }

    // Constructor that accepts a custom error message
    public StartupGuardException59(String message) {
        super(message);
    }

    // Constructor that accepts a message and a cause (for exception chaining)
    public StartupGuardException59(String message, Throwable cause) {
        super(message, cause);
    }

    // Constructor that accepts only a cause
    public StartupGuardException59(Throwable cause) {
        super(cause);
    }
}
