package dev.cobalt.shell.startupguardexceptions;

/**
 * Custom runtime exception for startup-related guard failures.
 */
public class StartupGuardException9 extends RuntimeException {

    // Default constructor
    public StartupGuardException9() {
        super();
    }

    // Constructor that accepts a custom error message
    public StartupGuardException9(String message) {
        super(message);
    }

    // Constructor that accepts a message and a cause (for exception chaining)
    public StartupGuardException9(String message, Throwable cause) {
        super(message, cause);
    }

    // Constructor that accepts only a cause
    public StartupGuardException9(Throwable cause) {
        super(cause);
    }
}
