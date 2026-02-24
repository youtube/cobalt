package dev.cobalt.shell.startupguardexceptions;

/**
 * Custom runtime exception for startup-related guard failures.
 */
public class StartupGuardException32 extends RuntimeException {

    // Default constructor
    public StartupGuardException32() {
        super();
    }

    // Constructor that accepts a custom error message
    public StartupGuardException32(String message) {
        super(message);
    }

    // Constructor that accepts a message and a cause (for exception chaining)
    public StartupGuardException32(String message, Throwable cause) {
        super(message, cause);
    }

    // Constructor that accepts only a cause
    public StartupGuardException32(Throwable cause) {
        super(cause);
    }
}
